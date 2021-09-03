/*******************************************************************************

    File:       pl_scan.c
    Contents:   Routines for scanning/tokenising Prolog.
    Author:     Rick Cooper
    Copyright (c) 1996 Richard P. Cooper

    This file is part of COGENT.

    COGENT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    COGENT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with COGENT; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    ------------------------------------------------------------------------

    This scanner operates on a character stream of input (FILE * or char *)
    and, on successive calls, returns tokens for use by the parser. Note
    that the scanner ignores comments: Prolog comments in the input stream
    are completely skipped. While the parser doesn't need such comments,
    this does mean that the input cannot be reconstructed in a form suitable
    for human viewing.

    The scanner returns both the token scanned and the number of characters
    scanned. Two kinds of error might arise: syntax errors (which are directed
    to pl_error_syntax/1) and memory allocation failures.

    Public procedures:
        void  fsyntax_error(char *error)
        void  ssyntax_error(char *error)
        int   fscan_token(FILE *fp, Token *token)
        int   sscan_token(char *s, Token *token)
        char *fscan_to_eoln(FILE *fp)
        int   pl_is_symbol(int c)
        int   pl_is_alpha(int c)

*******************************************************************************/

/******** Include files: ******************************************************/

#include "pl.h"
#include "lib_error.h"
#include <glib.h>

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#include <ctype.h>
#include <math.h>

extern void pl_error_syntax(char *error_message);

#ifdef MALLOC_CHECK
#include "../lib/zmalloc.h"
#endif

/******** Static and external variables: **************************************/

static int prev_line_count;
static int prev_char_count;
int pl_context_line_count;
int pl_context_char_count;
char *pl_context_filename;

/******************************************************************************/
/******** Report a syntax error: **********************************************/
/******************************************************************************/

static char error_buffer[512];

void fsyntax_error(char *error)
{
    g_snprintf(error_buffer, 512, "SYNTAX ERROR: %s line %d (char %d):\n\n%s", pl_context_filename, prev_line_count, prev_char_count, error);
    pl_error_syntax(error_buffer);
}

void ssyntax_error(char *error)
{
    g_snprintf(error_buffer, 512, "SYNTAX ERROR:\nCharacter %d of \"%s\":\n\n%s", prev_char_count, pl_context_filename, error);
    pl_error_syntax(error_buffer);
}

/******************************************************************************/
/******** Routines for Manipulating LongStrings: ******************************/
/******************************************************************************/

/**  All char strings are temporarily stored as type LongString. This type   **
 ** can be easily extended as the string grows. Once the true size of the    **
 ** string has been determined, the string is converted to type char *.      **/

#define  STRING_CHUNK            64

typedef struct string_chunk {
    char    contents[STRING_CHUNK];
    struct  string_chunk *next;
} StringChunk;

typedef struct {
    StringChunk *contents;
    int          length;
} LongString;

/*----------------------------------------------------------------------------*/

static void initialise_string(LongString *s)
{
    s->length = 0;
    s->contents = NULL;
}

/*----------------------------------------------------------------------------*/

static int copy_char_to_long_string(int c, LongString *s)
{
    StringChunk **array = &(s->contents);
    int index = s->length;

    while (index >= STRING_CHUNK) {
        index = index - STRING_CHUNK;
        array = &((*array)->next);
    }
    if (index == 0) {
        if (((*array) = (StringChunk *)malloc(sizeof(StringChunk))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while extending long string");
            return(FALSE); // error: failed_malloc
        }
    }
    (*array)->contents[index] = c;
    (*array)->next = NULL;
    s->length++;
    return(TRUE);
}

/*----------------------------------------------------------------------------*/

static char *long_string_to_char_string(LongString s)
{
    int i = 0, j = 0;
    StringChunk *array = s.contents;
    char *answer = string_new(s.length + 1);

    if (answer == NULL) {
        while (array != NULL) {
            StringChunk *tmp = array->next;
            free(array);
            array = tmp;
        }
        lib_error_report(ERROR_MALLOC_FAILED, "while allocating long string");
        return(NULL); // error: failed malloc
    }
    else {
        while (i < s.length) {
            answer[i++] = array->contents[j++];
            if (j == STRING_CHUNK) {
                StringChunk *tmp = array->next;
                j = 0;
                free(array);
                array = tmp;
            }
        }
        if (j != 0) {
            free(array);
        }
        answer[i] = '\0';
        return(answer);
    }
}

/******************************************************************************/
/******** Routines for Classifying Characters *********************************/
/******************************************************************************/

int pl_is_symbol(int c)
{
    return((strchr("+-/*~<>=^.:?@$#&\\", c) != NULL));
}

int pl_is_alpha(int c)
{
    return(((c == '_') || islower(c) || isupper(c) || isdigit(c)));
}

int pl_is_punctuation(int c)
{
    return((strchr("()[]{}|,;!%", c) != NULL));
}

static int map_char_to_token_type(int c, TypeOfToken *token_type)
{
    if (c == '(') {
        *token_type = LEFT_RD_BRACKET;
    }
    else if (c == ')') {
        *token_type = RIGHT_RD_BRACKET;
    }
    else if (c == '{') {
        *token_type = LEFT_PARENTHESIS;
    }
    else if (c == '}') {
        *token_type = RIGHT_PARENTHESIS;
    }
    else if (c == '[') {
        *token_type = LEFT_SQ_BRACKET;
    }
    else if (c == ']') {
        *token_type = RIGHT_SQ_BRACKET;
    }
    else if (c == '|') {
        *token_type = PL_LIST_SEP;
    }
    else if (c == '!') {
        *token_type = PL_CUT;
    }
    else {
        return(FALSE);
    }
    return(TRUE);
}

/******************************************************************************/
/******** Routines for Getting Tokens *****************************************/
/******************************************************************************/

static int peek_next_char(FILE *file)
{
    return(ungetc(getc(file), file));
}

static int fscan_next_char(FILE *file, char *eof_message, int *c)
{
    if ((*c = getc(file)) == EOF) {
        fsyntax_error(eof_message);
    }
    else {
        pl_context_char_count++;
    }

    if (*c == '\n') {
        pl_context_line_count++; pl_context_char_count = 0;
    }

    return(1);
}

static int sscan_next_char(char *s, char *eof_message, int *c)
{
    if ((*c = s[0]) == '\0') {
        ssyntax_error(eof_message);
    }
    else {
        pl_context_char_count++;
    }

    if (*c == '\n') {
        pl_context_line_count++; pl_context_char_count = 0;
    }
    return(1);
}

static int fscan_integer(FILE *fp, int c, long *num)
{
    int d;
    int j = 0;

    *num = c - '0';

    while (((d = getc(fp)) != EOF) && (d != '\n') && (isdigit(d))) {
        *num = *num * 10 + (d - '0');
        pl_context_char_count++; j++;
    }

    ungetc(d, fp);
    return(j);
}

static int sscan_integer(char *s, int c, long *num)
{
    int d;
    int j = 0;

    *num = c - '0';
    while (((d = s[j++]) != '\0') && (d != '\n') && (isdigit(d))) {
        *num = *num * 10 + (d - '0');
        pl_context_char_count++;
    }
    j--;
    return(j);
}

static int fscan_post_decimal(FILE *fp, int c, double *num)
{
    int d;
    int exponent = 1, j = 0;

    *num = ((double) (c - '0')) / pow(10.0, (double) exponent++);

    while (((d = getc(fp)) != EOF) && (d != '\n') && (isdigit(d))) {
        *num = *num + (((double) (d - '0')) / pow(10.0, (double) exponent++));
        pl_context_char_count++; j++;
    }
    ungetc(d, fp);
    return(j);
}

static int sscan_post_decimal(char *s, int c, double *num)
{
    int d;
    int exponent = 1, j = 0;

    *num = ((double) (c - '0')) / pow(10.0, (double) exponent++);

    while (((d = s[j++]) != '\0') && (d != '\n') && (isdigit(d))) {
        *num = *num + (((double) (d - '0')) / pow(10.0, (double) exponent++));
        pl_context_char_count++;
    }
    j--;
    return(j);
}

static int fscan_exponent(FILE *fp, long *exponent)
{
    int j = 0;
    int c;

    c = getc(fp); pl_context_char_count++; j++;
    if (c == '-') {
        c = getc(fp); pl_context_char_count++; j++;
        if (isdigit(c)) {
            j += fscan_integer(fp, c, exponent);
            *exponent = -1 * *exponent;
        }
        else {
            ungetc(c, fp); pl_context_char_count--; j--;
            ssyntax_error("Malformed exponent in floating point expression");
        }
    }
    else if (c == '+') {
        c = getc(fp); pl_context_char_count++; j++;
        if (isdigit(c)) {
            j += fscan_integer(fp, c, exponent);
        }
        else {
            ungetc(c, fp); pl_context_char_count--; j--;
            ssyntax_error("Malformed exponent in floating point expression");
        }
    }
    else if (isdigit(c)) {
        j += fscan_integer(fp, c, exponent);
    }
    else {
        ungetc(c, fp); pl_context_char_count--; j--;
        ssyntax_error("Malformed exponent in floating point expression");
    }
    return(j);
}

static int sscan_exponent(char *s, long *exponent)
{
    int j = 0;

    if (s[j] == '-') {
        j++; pl_context_char_count++;
        if (isdigit((int) s[j])) {
            j++; pl_context_char_count++;
            j += sscan_integer(&s[j], s[j-1], exponent);
            *exponent = -1 * *exponent;
        }
        else {
            ssyntax_error("Malformed exponent in floating point expression");
        }
    }
    else if (s[j] == '+') {
        j++; pl_context_char_count++;
        if (isdigit((int) s[j])) {
            j++; pl_context_char_count++;
            j += sscan_integer(&s[j], s[j-1], exponent);
        }
        else {
            ssyntax_error("Malformed exponent in floating point expression");
        }
    }
    else if (isdigit((int) s[j])) {
        j++; pl_context_char_count++;
        j += sscan_integer(&s[j], s[j-1], exponent);
    }
    else {
        ssyntax_error("Malformed exponent in floating point expression");
    }
    return(j);
}

static int fscan_numeric_token(FILE *fp, int c1, Token *symbol, char *next)
{
    long n, exponent;
    double d;
    int c2, c3, c4;
    int j = 0;

    j += fscan_integer(fp, c1, &n);
    c2 = getc(fp); pl_context_char_count++; j++;

    if (c2 == '.') {
        if (isdigit((c3 = getc(fp)))) {
            pl_context_char_count++; j++;
            j += fscan_post_decimal(fp, c3, &d);
            symbol->token_type = PL_REAL;
            symbol->real = (double) n + d;
            c4 = peek_next_char(fp);
            if ((c4 == 'e') || (c4 == 'E')) {
                getc(fp); pl_context_char_count++; j++;
                j += fscan_exponent(fp, &exponent);
                symbol->real = symbol->real * pow(10.0, (double) exponent);
            }
            *next = '\0';
        }
        else {
            ungetc(c3, fp);
            symbol->token_type = PL_INT;
            symbol->number = n;
            *next = '.';
        }
    }
    else if ((c2 == 'e') || (c2 == 'E')) {
        symbol->token_type = PL_REAL;
        symbol->real = (double) n;
        j += fscan_exponent(fp, &exponent);
        symbol->real = symbol->real * pow(10.0, (double) exponent);
    }
    else {
        ungetc(c2, fp); pl_context_char_count--; j--;
        symbol->token_type = PL_INT;
        symbol->number = n;
        *next = '\0';
    }
    return(j);
}

static int sscan_numeric_token(char *s, int c1, Token *symbol)
{
    long n, exponent;
    double d;
    int j = 0;

    j += sscan_integer(&s[j], c1, &n);

    if ((s[j] == '.') && isdigit((int) s[j+1])) {
        j++; pl_context_char_count++;
        j++; pl_context_char_count++;
        j += sscan_post_decimal(&s[j], s[j-1], &d);
        symbol->token_type = PL_REAL;
        symbol->real = (double) n + d;
        if ((s[j] == 'e') || (s[j] == 'E')) {
            j++; pl_context_char_count++;
            j += sscan_exponent(&s[j], &exponent);
            symbol->real = symbol->real * pow(10.0, (double) exponent);
        }
    }
    else if ((s[j] == 'e') || (s[j] == 'E')) {
        symbol->token_type = PL_REAL;
        symbol->real = (double) n;
        j++; pl_context_char_count++;
        j += sscan_exponent(&s[j], &exponent);
        symbol->real = symbol->real * pow(10.0, (double) exponent);
    }
    else {
        symbol->token_type = PL_INT;
        symbol->number = n;
    }
    return(j);
}

static int fscan_quoted_name(FILE *fp, int quote, char **name)
{
    int current;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    while (((current = getc(fp)) != EOF) && (current != '\n')) {
        pl_context_char_count++; j++;
        if  (current != quote) {
            if (!copy_char_to_long_string(current, &tmp)) {
                *name = NULL;
                return(0); 
            }
        }
        else if ((current = getc(fp)) == quote) {
            pl_context_char_count++; j++;
            if (!copy_char_to_long_string(quote, &tmp)) {
                *name = NULL;
                return(0); 
            }
        }
        else {
            ungetc(current, fp);
            current = '\'';
            break;
        }
    }
    if (current == EOF) {
        ungetc(current, fp);
        fsyntax_error("Quote not closed before end of file");
    }
    else if (current == '\n') {
        fsyntax_error("Quote not closed before end of line");
        pl_context_line_count++; pl_context_char_count = 0; j++;
    }
    *name = long_string_to_char_string(tmp);
    return((*name == NULL) ? 0 : j);
}

static int sscan_quoted_name(char *s, int quote, char **name)
{
    int current;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    while (((current = s[j++]) != '\0') && (current != '\n')) {
        pl_context_char_count++;
        if  (current != quote) {
            if (!copy_char_to_long_string(current, &tmp)) {
                *name = NULL;
                return(0); 
            }
        }
        else if ((current = s[j++]) == quote) {
            pl_context_char_count++;
            if (!copy_char_to_long_string(quote, &tmp)) {
                *name = NULL;
                return(0); 
            }
        }
        else {
            j--;
            current = '\'';
            break;
        }
    }
    if (current == '\0') {
        ssyntax_error("Quote not closed before end of string");
    }
    else if (current == '\n') {
        ssyntax_error("Quote not closed before end of line");
        pl_context_line_count++; pl_context_char_count = 0; j++;
    }
    *name = long_string_to_char_string(tmp);
    return((*name == NULL) ? 0 : j);
}

static int fscan_name(FILE *fp, int current, char **name)
{
    LongString n;
    int c;
    int j = 0;

    if (current == '\'') {
        return(fscan_quoted_name(fp, current, name));
    }
    else {
        initialise_string(&n);
        if (!copy_char_to_long_string(current, &n)) {
            *name = NULL;
            return(0);
        }

        if (pl_is_symbol(current)) {
            while (((c = getc(fp)) != '\n') && (c != EOF) && pl_is_symbol(c)) {
                if (!copy_char_to_long_string(c, &n)) {
                    *name = NULL;
                    return(0);
                }
                pl_context_char_count++; j++;
            }
            ungetc(c, fp);
        }
        else if (!pl_is_punctuation(current)) {
            while (((c = getc(fp)) != '\n') && (c != EOF) && pl_is_alpha(c)) {
                if (!copy_char_to_long_string(c, &n)) {
                    *name = NULL;
                    return(0);
                }
                pl_context_char_count++; j++;
            }
            ungetc(c, fp);
        }
        *name = long_string_to_char_string(n);
        return((*name == NULL) ? 0 : j);
    }
}

static int sscan_name(char *s, int current, char **name)
{
    LongString n;
    int c;
    int j = 0;

    if (current == '\'') {
        return(sscan_quoted_name(&s[j], current, name));
    }
    else {
        initialise_string(&n);
        if (!copy_char_to_long_string(current, &n)) {
            *name = NULL;
            return(0);
        }

        if (pl_is_symbol(current)) {
            while (((c = s[j++]) != '\n') && (c != '\0') && pl_is_symbol(c)) {
                if (!copy_char_to_long_string(c, &n)) {
                    *name = NULL;
                    return(0);
                }
                pl_context_char_count++;
            }
            j--;
        }
        else if (!pl_is_punctuation(current)) {
            while (((c = s[j++]) != '\n') && (c != '\0') && pl_is_alpha(c)) {
                if (!copy_char_to_long_string(c, &n)) {
                    *name = NULL;
                    return(0);
                }
                pl_context_char_count++;
            }
            j--;
        }
        *name = long_string_to_char_string(n);
        return((*name == NULL) ? 0 : j);
    }
}

static int fscan_variable(FILE *fp, int current, char **name)
{
    LongString v;
    int c;
    int j = 0;

    initialise_string(&v);
    if (!copy_char_to_long_string(current, &v)) {
        *name = NULL;
        return(0);
    }

    while (((c = getc(fp)) != EOF) && (c != '\n') && (pl_is_alpha(c))) {
        if (!copy_char_to_long_string(c, &v)) {
            *name = NULL;
            return(0);
        }
        pl_context_char_count++; j++;
    }
    ungetc(c, fp);
    *name = long_string_to_char_string(v);
    return((*name == NULL) ? 0 : j);
}

static int sscan_variable(char *s, int current, char **name)
{
    LongString v;
    int c;
    int j = 0;

    initialise_string(&v);
    if (!copy_char_to_long_string(current, &v)) {
        *name = NULL;
        return(0);
    }

    while (((c = s[j++]) != '\0') && (c != '\n') && (pl_is_alpha(c))) {
        if (!copy_char_to_long_string(c, &v)) {
            *name = NULL;
            return(0);
        }
        pl_context_char_count++;
    }
    j--;
    *name = long_string_to_char_string(v);
    return((*name == NULL) ? 0 : j);
}

static int fskip_multiline_comment(FILE *fp, char **comment)
{
    int c, d;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    j += fscan_next_char(fp, "End of file during comment!", &c);
    j += fscan_next_char(fp, "End of file during comment!", &d);

    while (((c != '*') || (d != '/')) && (d != EOF)) {
        if (!copy_char_to_long_string(c, &tmp)) {
            *comment = NULL;
            return(0);
        }
        c = d;
        j += fscan_next_char(fp, "End of file during comment!", &d);
    }
    *comment = long_string_to_char_string(tmp);
    return((*comment == NULL) ? 0 : j);
}

static int sskip_multiline_comment(char *s, char **comment)
{
    int c, d;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    j += sscan_next_char(&s[j], "End of file during comment!", &c);
    j += sscan_next_char(&s[j], "End of file during comment!", &d);

    while (((c != '*') || (d != '/')) && (d != '\0')) {
        if (!copy_char_to_long_string(c, &tmp)) {
            *comment = NULL;
            return(0);
        }
        c = d;
        j += sscan_next_char(&s[j], "End of file during comment!", &d);
    }
    *comment = long_string_to_char_string(tmp);
    return((*comment == NULL) ? 0 : j);
}

static int fskip_to_eoln(FILE *fp, char **comment)
{
    int c;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    j += fscan_next_char(fp, "End of file before end of line!", &c);
    while ((c != '\n') && (c != EOF)) {
        if (!copy_char_to_long_string(c, &tmp)) {
            *comment = NULL;
            return(0);
        }
        j += fscan_next_char(fp, "End of file before end of line!", &c);
    }
    *comment = long_string_to_char_string(tmp);
    return((*comment == NULL) ? 0 : j);
}

static int sskip_to_eoln(char *s, char **comment)
{
    int c;
    LongString tmp;
    int j = 0;

    initialise_string(&tmp);

    j += sscan_next_char(&s[j], "End of file before end of line!", &c);
    while ((c != '\n') && (c != '\0')) {
        if (!copy_char_to_long_string(c, &tmp)) {
            *comment = NULL;
            return(0);
        }
        j += sscan_next_char(&s[j], "End of file before end of line!", &c);
    }
    *comment = long_string_to_char_string(tmp);
    return((*comment == NULL) ? 0 : j);
}

/******************************************************************************/
/******** The guts of the scanner: ********************************************/
/******************************************************************************/

int fscan_token(FILE *fp, Token *symbol)
{
    /* It is assumed that token points to preallocated space... */

    static char look_ahead = '\0';
    TypeOfToken token_type;
    int c;
    int j = 0;

    prev_line_count = pl_context_line_count;
    prev_char_count = pl_context_char_count;

    symbol->token_name = NULL;
    if (look_ahead == '\0') {
        c = getc(fp); pl_context_char_count++;
    }
    else {
        c = look_ahead;
        look_ahead = '\0';
    }
    j++;
    if (c == EOF) {
        symbol->token_type = PL_EOF;
    }
    else if (c == '\n') {
        pl_context_line_count++; pl_context_char_count = 0;
        j += fscan_token(fp, symbol);
    }
    else if (iscntrl(c) || isspace(c)) {
        j += fscan_token(fp, symbol);
    }
    else if (c == '%') {
        char *comment;
        j += fskip_to_eoln(fp, &comment);
        free(comment);
        j += fscan_token(fp, symbol);
    }
    else if ((c == '/') && (peek_next_char(fp) == '*')) {
        char *comment;
        getc(fp); pl_context_char_count++; j++;
        j += fskip_multiline_comment(fp, &comment);
        free(comment);
        j += fscan_token(fp, symbol);
    }
    else if (map_char_to_token_type(c, &token_type)) {
        symbol->token_type = token_type;
    }
    else if ((c == '.') && (peek_next_char(fp) <= ' ')) {
        symbol->token_type = PL_FULL_STOP;
    }
    else if (c == '"') {
        symbol->token_type = PL_QUOTED_CHARS;
        j += fscan_quoted_name(fp, '"', &(symbol->token_name));
    }
    else if (isdigit(c)) {
        j += fscan_numeric_token(fp, c, symbol, &look_ahead);
    }
    else if ((c == '_') || (isupper(c))) {
        symbol->token_type = PL_VAR;
        j += fscan_variable(fp, c, &(symbol->token_name));
    }
    else if ((c == ',') || (c == ';')) {
        j += fscan_name(fp, c, &(symbol->token_name));
        symbol->token_type = PL_SYM;
    }
    else {
        j += fscan_name(fp, c, &(symbol->token_name));
        if (peek_next_char(fp) == '(') {
            symbol->token_type = PL_FUNCTOR;
        }
        else {
            symbol->token_type = PL_SYM;
        }
    }
    return(j);
}

int sscan_token(char *s, Token *symbol)
{
    /* We assume that symbol points to preallocated space */

    TypeOfToken token_type;
    int c;
    int j = 0;

    prev_line_count = pl_context_line_count;
    prev_char_count = pl_context_char_count;

    symbol->token_name = NULL;
    c = s[j++]; pl_context_char_count++;

    if (c == '\0') {
        symbol->token_type = PL_EOF;
    }
    else if (c == '\n') {
        pl_context_line_count++; pl_context_char_count = 0;
        j += sscan_token(&s[j], symbol);
    }
    else if (iscntrl(c) || isspace(c)) {
        j += sscan_token(&s[j], symbol);
    }
    else if (c == '%') {
        char *comment;
        j += sskip_to_eoln(&s[j], &comment);
        free(comment);
        j += sscan_token(&s[j], symbol);
    }
    else if ((c == '/') && (s[j] == '*')) {
        char *comment;
        j++; pl_context_char_count++;
        j += sskip_multiline_comment(&s[j], &comment);
        free(comment);
        j += sscan_token(&s[j], symbol);
    }
    else if (map_char_to_token_type(c, &token_type)) {
        symbol->token_type = token_type;
    }
    else if ((c == '.') && (s[j] <= ' ')) {
        symbol->token_type = PL_FULL_STOP;
    }
    else if (c == '"') {
        symbol->token_type = PL_QUOTED_CHARS;
        j += sscan_quoted_name(&s[j], '"', &(symbol->token_name));
    }
    else if (isdigit(c)) {
        j += sscan_numeric_token(&s[j], c, symbol);
    }
    else if ((c == '_') || (isupper(c))) {
        symbol->token_type = PL_VAR;
        j += sscan_variable(&s[j], c, &(symbol->token_name));
    }
    else if ((c == ',') || (c == ';')) {
        j += sscan_name(&s[j], c, &(symbol->token_name));
        symbol->token_type = PL_SYM;
    }
    else {
        j += sscan_name(&s[j], c, &(symbol->token_name));

        if (s[j] == '(') {
            symbol->token_type = PL_FUNCTOR;
        }
        else {
            symbol->token_type = PL_SYM;
        }
    }
    return(j);
}

char *fscan_to_eoln(FILE *fp)
{
    char *s;

    fskip_to_eoln(fp, &s);
    return(s);
}

/******************************************************************************/
/**************************  E N D   O F   F I L E  ***************************/
/******************************************************************************/
