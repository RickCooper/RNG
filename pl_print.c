/*******************************************************************************

    File:       pl_print.c
    Contents:   Routines for printing Prolog-style terms.
    Author:     Rick Cooper
    Copyright (c) 1997 Richard P. Cooper

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

    Public procedures:
        int fprint_clause(FILE *fp, ClauseType *clause)
        int sprint_clause(char *s, ClauseType *clause)
        int fprint_clause_without_quotes(FILE *fp, ClauseType *clause)
        int sprint_clause_without_quotes(char *s, ClauseType *clause)
        int fprint_clause_without_extfloats(FILE *fp, ClauseType *clause)
        int sprint_clause_without_extfloats(char *s, ClauseType *clause)
        void pl_set_output_precision(int p)

*******************************************************************************/

/* NOTE: These printing routines make use of sprintf, which is in non-  *
 * standard in gcc. Rather than returning an integer (the number of     *
 * characters printed), it returns a pointer to char, corresponding to  *
 * the string of characters printed. Hence, all calls to sprintf are    *
 * followed by a call to strlen to get the length of the answer, rather *
 * than just using the return as the length of the answer, so that this *
 * code works okay under both gcc and cc.                               */

/******** Include files: ******************************************************/

#include "pl.h"
#include <ctype.h>

#define TRUE    1
#define FALSE   0

#ifdef MALLOC_CHECK
#include "../lib/zmalloc.h"
#endif

/******************************************************************************/
/*-------- Forward Declarations: ---------------------------------------------*/
/******************************************************************************/

static int fprint_term(FILE *fp, ClauseType clause, int precedence, int quotes, int pretty_floats);
static int fprint_pterm(FILE *fp, ClauseType *clause, int precedence, int quotes, int pretty_floats);
static int sprint_term(char *s, ClauseType clause, int precedence, int quotes, int pretty_floats);
static int sprint_pterm(char *s, ClauseType *clause, int precedence, int quotes, int pretty_floats);

/********* Local variables: ***************************************************/

/* By default print values to 8 decimal places:                               */
static short numeric_precision = 8;

/******************************************************************************/

static OperatorType *is_operator(ClauseType clause, OpClass op_class)
{
    Token tmp;

    tmp.token_type = PL_SYM;
    tmp.token_name = pl_functor(&clause);

    if ((op_class == INFIX) && (pl_arity(&clause) == 2)) {
        return(lookup_operator(&tmp, op_class));
    }
    else if ((op_class != INFIX) && (pl_arity(&clause) == 1)) {
        return(lookup_operator(&tmp, op_class));
    }
    else {
        return(NULL);
    }
}

static int quotes_required(const char *s)
{
    if ((s == NULL) || (s[0] == '\0')) {
        return(TRUE);
    }
    else if ((s[0] == ',') || (s[0] == ';')) {
        return(s[1] != '\0');
    }
    else if (pl_is_symbol((int) s[0])) {
        int i;
        for (i = 1; (s[i] != '\0'); i++) {
            if (!pl_is_symbol((int) s[i])) {
                return(TRUE);
            }
        }
        return(FALSE);
    }
    else if (islower((int) s[0])) {
        int i;
        for (i = 1; (s[i] != '\0'); i++) {
            if (!pl_is_alpha((int) s[i])) {
                return(TRUE);
            }
        }
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

static int sprint_argument_list(char *s, ClauseList *arguments, int quotes, int pretty_floats)
{
    int j = 0;

    if (arguments != NULL) {
        while (arguments->tail != NULL) {
            j += sprint_pterm(&s[j], arguments->head, 999, quotes, pretty_floats);
            s[j++] = ',';
            s[j++] = ' ';
            arguments = arguments->tail;
        }
        j += sprint_pterm(&s[j], arguments->head, 999, quotes, pretty_floats);
    }
    return(j);
}

static int fprint_argument_list(FILE *fp, ClauseList *arguments, int quotes, int pretty_floats)
{
    int j = 0;

    if (arguments != NULL) {
        while (arguments->tail != NULL) {
            j += fprint_pterm(fp, arguments->head, 999, quotes, pretty_floats);
            j += fprintf(fp, ", ");
            arguments = arguments->tail;
        }
        j += fprint_pterm(fp, arguments->head, 999, quotes, pretty_floats);
    }
    return(j);
}

static int sprint_quoted_string(char *s, char quote, const char *string)
{
    int i = 0, j = 0;
    s[j++] = quote;
    if (string != NULL) {
        while (string[i] != '\0') {
            if (string[i] == quote) {
                s[j++] = quote;
            }
            s[j++] = string[i++];
        }
    }
    s[j++] = quote;
    return(j);
}

static int fprint_quoted_string(FILE *fp, char quote, const char *string)
{
    int i = 0, j = 0;
    fputc(quote, fp); j++;
    if (string != NULL) {
        while (string[i] != '\0') {
            if (string[i] == quote) {
                fputc(quote, fp); j++;
            }
            fputc(string[i++], fp); j++;
        }
    }
    fputc(quote, fp); j++;
    return(j);
}

static int sprint_atom(char *s, const char *string, int quotes)
{
    int j = 0;
    if (quotes && quotes_required(string)) {
        j += sprint_quoted_string(s, '\'', string);
    }
    else {
        int i = 0;
        while (string[i] != '\0') {
            s[j++] = string[i++];
        }
    }
    return(j);
}

static int fprint_atom(FILE *fp, const char *string, int quotes)
{
    int j = 0;
    if (quotes && quotes_required(string)) {
        j += fprint_quoted_string(fp, '\'', string);
    }
    else {
        j += fprintf(fp, "%s", string);
    }
    return(j);
}

static int sprint_term(char *s, ClauseType clause, int precedence, int quotes, int pretty_floats)
{
    int j = 0;

    switch (pl_clause_type(&clause)) {
        case COMPLEX: {
            int max_precedence;
            OperatorType *op;
            char *string;

            if (pl_arity(&clause) == 0) {
                j += sprint_atom(&s[j], pl_functor(&clause), quotes);
            }
            else if ((string = pl_list_to_char_string(&clause)) != NULL) {
                j += sprint_quoted_string(&s[j], '"', string);
                free(string);
            }
            else if (functor_comp(&clause, ".", 2)) {
                ClauseType *next;

                s[j++] = '[';
                j += sprint_pterm(&s[j], pl_arg_get(&clause, 1), 999, quotes, pretty_floats);
		next = pl_arg_get(&clause, 2);
                while (functor_comp(next, ".", 2)) {
                    s[j++] = ',';
                    s[j++] = ' ';
                    j += sprint_pterm(&s[j], pl_arg_get(next, 1), 999, quotes, pretty_floats);
                    next = pl_arg_get(next, 2);
                }
                if (pl_clause_type(next) != EMPTY_LIST) {
                    s[j++] = '|';
                    j += sprint_pterm(&s[j], next, 999, quotes, pretty_floats);
                }
                s[j++] = ']';
            }
            else if (pretty_floats && functor_comp(&clause, "ext_float", 3)) {
                ClauseType *tmp = pl_arg_get(&clause, 1);
                sprintf(&s[j], "%.*f", (int) numeric_precision, pl_double(tmp));
                j += strlen(&s[j]);
            }
            else if ((op = is_operator(clause, PREFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    s[j++] = '(';
                }
                j += sprint_atom(&s[j], pl_functor(&clause), quotes);
                s[j++] = ' ';
                max_precedence = ((op->op_type == FX)?(op->op_precedence - 1):op->op_precedence);
                j += sprint_pterm(&s[j], pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                if (op->op_precedence > precedence) {
                    s[j++] = ')';
                }
            }
            else if ((op = is_operator(clause, INFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    s[j++] = '(';
                }
                max_precedence = (((op->op_type == XFX) || (op->op_type == XFY))?(op->op_precedence - 1):op->op_precedence);
                j += sprint_pterm(&s[j], pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                s[j++] = ' ';
                j += sprint_atom(&s[j], pl_functor(&clause), quotes);
                s[j++] = ' ';
                max_precedence = (((op->op_type == XFX) || (op->op_type == YFX))?(op->op_precedence - 1):op->op_precedence);
                j += sprint_pterm(&s[j], pl_arg_get(&clause, 2), max_precedence, quotes, pretty_floats);
                if (op->op_precedence > precedence) {
                    s[j++] = ')';
                }
            }
            else if ((op = is_operator(clause, POSTFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    s[j++] = '(';
                }
                max_precedence = ((op->op_type == XF)?(op->op_precedence - 1):op->op_precedence);
                j += sprint_pterm(&s[j], pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                s[j++] = ' ';
                j += sprint_atom(&s[j], pl_functor(&clause), quotes);
                if (op->op_precedence > precedence) {
                    s[j++] = ')';
                }
            }
            else {
                j += sprint_atom(&s[j], pl_functor(&clause), quotes);
                s[j++] = '(';
                j += sprint_argument_list(&s[j], pl_arguments(&clause), quotes, pretty_floats);
                s[j++] = ')';
            }
            break;
        }
        case EMPTY_LIST: {
            s[j++] = '[';
            s[j++] = ']';
            break;
        }
        case VAR: {
            sprintf(&s[j], "%s", pl_functor(&clause));
            j += strlen(&s[j]);
            break;
        }
        case INT_NUMBER: {
            sprintf(&s[j], "%ld", pl_integer(&clause));
            j += strlen(&s[j]);
            break;
        }
        case REAL_NUMBER: {
            sprintf(&s[j], "%.*f", (int) numeric_precision, pl_double(&clause));
            j += strlen(&s[j]);
            break;
        }
        case STRING: {
            j += sprint_quoted_string(&s[j], '\"', pl_functor(&clause));
            break;
        }
        case NULL_TERM: {
            j += sprint_atom(&s[j], "error", quotes);
            break;
        }
    }
    s[j] ='\0';
    return(j);
}

static int fprint_term(FILE *fp, ClauseType clause, int precedence, int quotes, int pretty_floats)
{
    int j = 0;

    switch (pl_clause_type(&clause)) {
        case COMPLEX: {
            int max_precedence;
            OperatorType *op;
            char *string;

            if (pl_arity(&clause) == 0) {
                j += fprint_atom(fp, pl_functor(&clause), quotes);
            }
            else if ((string = pl_list_to_char_string(&clause)) != NULL) {
                fprint_quoted_string(fp, '"', string);
                free(string);
            }
            else if (functor_comp(&clause, ".", 2)) {
                ClauseType *next;

                fputc('[', fp); j++;
                j += fprint_pterm(fp, pl_arg_get(&clause, 1), 999, quotes, pretty_floats);
		next = pl_arg_get(&clause, 2);
                while (functor_comp(next, ".", 2)) {
                    fputc(',', fp); j++;
                    fputc(' ', fp); j++;
                    j += fprint_pterm(fp, pl_arg_get(next, 1), 999, quotes, pretty_floats);
                    next = pl_arg_get(next, 2);
                }
                if (pl_clause_type(next) != EMPTY_LIST) {
                    fputc('|', fp); j++;
                    j += fprint_pterm(fp, next, 999, quotes, pretty_floats);
                }
                fputc(']', fp); j++;
            }
            else if (pretty_floats && functor_comp(&clause, "ext_float", 3)) {
                ClauseType *tmp = pl_arg_get(&clause, 1);
                j += fprintf(fp, "%.*f", (int) numeric_precision, pl_double(tmp));
            }
            else if ((op = is_operator(clause, PREFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    fputc('(', fp); j++;
                }
                j += fprint_atom(fp, pl_functor(&clause), quotes);
                fputc(' ', fp); j++;
                max_precedence = ((op->op_type == FX)?(op->op_precedence - 1):op->op_precedence);
                j += fprint_pterm(fp, pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                if (op->op_precedence > precedence) {
                    fputc(')', fp); j++;
                }
            }
            else if ((op = is_operator(clause, INFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    fputc('(', fp); j++;
                }
                max_precedence = (((op->op_type == XFX) || (op->op_type == XFY))?(op->op_precedence - 1):op->op_precedence);
                j += fprint_pterm(fp, pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                fputc(' ', fp); j++;
                j += fprint_atom(fp, pl_functor(&clause), quotes);
                fputc(' ', fp); j++;
                max_precedence = (((op->op_type == XFX) || (op->op_type == YFX))?(op->op_precedence - 1):op->op_precedence);
                j += fprint_pterm(fp, pl_arg_get(&clause, 2), max_precedence, quotes, pretty_floats);
                if (op->op_precedence > precedence) {
                    fputc(')', fp); j++;
                }
            }
            else if ((op = is_operator(clause, POSTFIX)) != NULL) {
                if (op->op_precedence > precedence) {
                    fputc('(', fp); j++;
                }
                max_precedence = ((op->op_type == XF)?(op->op_precedence - 1):op->op_precedence);
                j += fprint_pterm(fp, pl_arg_get(&clause, 1), max_precedence, quotes, pretty_floats);
                fputc(' ', fp); j++;
                j += fprint_atom(fp, pl_functor(&clause), quotes);
                if (op->op_precedence > precedence) {
                    fputc(')', fp); j++;
                }
            }
            else {
                j += fprint_atom(fp, pl_functor(&clause), quotes);
                fputc('(', fp); j++;
                j += fprint_argument_list(fp, pl_arguments(&clause), quotes, pretty_floats);
                fputc(')', fp); j++;
            }
            break;
        }
        case EMPTY_LIST: {
            fputc('[', fp); j++; 
            fputc(']', fp); j++;
            break;
        }
        case VAR: {
            j += fprintf(fp, "%s", pl_functor(&clause));
            break;
        }
        case INT_NUMBER: {
            j += fprintf(fp, "%ld", pl_integer(&clause));
            break;
        }
        case REAL_NUMBER: {
            j += fprintf(fp, "%.*f", (int) numeric_precision, pl_double(&clause));
            break;
        }
        case STRING: {
            j += fprint_quoted_string(fp, '\"', pl_functor(&clause));
            break;
        }
        case NULL_TERM: {
            j += fprint_atom(fp, "error", quotes);
            break;
        }
    }
    fflush(fp);
    return(j);
}

static int sprint_pterm(char *s, ClauseType *clause, int precedence, int quotes, int pretty_floats)
{
    if (clause == NULL) {
        sprintf(s, "error");
        return(strlen(s));
    }
    else {
        return(sprint_term(s, *clause, precedence, quotes, pretty_floats));
    }
}

static int fprint_pterm(FILE *fp, ClauseType *clause, int precedence, int quotes, int pretty_floats)
{
    if (clause == NULL) {
        return(fprintf(fp, "error"));
    }
    else {
        return(fprint_term(fp, *clause, precedence, quotes, pretty_floats));
    }
}

int sprint_clause(char *s, ClauseType *clause)
{
    if (clause == NULL) {
        sprintf(s, "NULL");
        return(-1);
    }
    else {
        return(sprint_pterm(s, clause, 1200, TRUE, FALSE));
    }
}

int fprint_clause(FILE *fp, ClauseType *clause)
{
    if (clause == NULL) {
        fprintf(fp, "NULL");
        return(-1);
    }
    else {
        return(fprint_pterm(fp, clause, 1200, TRUE, FALSE));
    }
}

int sprint_clause_without_quotes(char *s, ClauseType *clause)
{
    if (clause == NULL) {
        sprintf(s, "NULL");
        return(-1);
    }
    else {
        return(sprint_pterm(s, clause, 1200, FALSE, FALSE));
    }
}

int fprint_clause_without_quotes(FILE *fp, ClauseType *clause)
{
    if (clause == NULL) {
        fprintf(fp, "NULL");
        return(-1);
    }
    else {
        return(fprint_pterm(fp, clause, 1200, FALSE, FALSE));
    }
}

int sprint_clause_without_extfloats(char *s, ClauseType *clause)
{
    if (clause == NULL) {
        sprintf(s, "NULL");
        return(-1);
    }
    else {
        return(sprint_pterm(s, clause, 1200, TRUE, TRUE));
    }
}

int fprint_clause_without_extfloats(FILE *fp, ClauseType *clause)
{
    if (clause == NULL) {
        fprintf(fp, "NULL");
        return(-1);
    }
    else {
        return(fprint_pterm(fp, clause, 1200, TRUE, TRUE));
    }
}

void pl_set_output_precision(int p)
{
    numeric_precision = (short) p;
}

int fprint_string_as_atom(FILE *fp, const char *string)
{
    return(fprint_atom(fp, string, TRUE));
}

/******************************************************************************/
/**************************  E N D   O F   F I L E  ***************************/
/******************************************************************************/
