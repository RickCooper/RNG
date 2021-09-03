/*******************************************************************************

    File:       pl_parse.c
    Contents:   Routines for Prolog parsing.
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

    This is COGENT's main parser for handling all Prolog input. It is quite
    a straightfoward recursive descent parser, but as a result it is a bit
    clunky and relatively slow. It could do with a re-write using some kind
    of look-ahead algorithm ... if anyone has time.

    Error handling: 

    Public procedures:
        int   fscan_clause(FILE *fp, ClauseType *clause)
        int   sscan_clause(char *s, ClauseType *clause)

*******************************************************************************/

/******** Include files: ******************************************************/

#include "pl.h"
#include "lib_error.h"

#include <stdarg.h>

#ifdef MALLOC_CHECK
#include "../lib/zmalloc.h"
#endif

/******** External procedures: ************************************************/

#define FALSE 0
#define TRUE 1

extern int   fscan_token(FILE *fp, Token *token);
extern int   sscan_token(char *s, Token *token);
extern void  fsyntax_error(char *error);
extern void  ssyntax_error(char *error);

/******************************************************************************/
/********* Utility procedures: ************************************************/
/******************************************************************************/

static int clause_list_length(ClauseList *list)
{
    int i = 0;

    while (list != NULL) {
        list = list->tail;
        i++;
    }
    return(i);
}

static int potential_subterm(Token *token, int precedence)
{
    switch (token->token_type) {
        case LEFT_RD_BRACKET:
        case LEFT_PARENTHESIS:
        case LEFT_SQ_BRACKET:
        case PL_FUNCTOR:
        case PL_QUOTED_CHARS:
        case PL_INT:
        case PL_REAL:
        case PL_VAR: {
            return(TRUE);
        }
        case PL_CUT:
        case RIGHT_RD_BRACKET:
        case RIGHT_SQ_BRACKET:
        case RIGHT_PARENTHESIS:
        case PL_LIST_SEP:
        case PL_FULL_STOP:
        case PL_EOF: {
            return(FALSE);
        }
        case PL_SYM: {
            OperatorType *op;

            if ((op = lookup_operator(token, PREFIX)) != NULL) {
                return(op->op_precedence <= precedence);
            }
            else if ((op = lookup_operator(token, INFIX)) != NULL) {
                return(op->op_precedence <= precedence);
            }
            else if ((op = lookup_operator(token, POSTFIX)) != NULL) {
                return(op->op_precedence <= precedence);
            }
            else {
                return(TRUE);
            }
        }
        default: {
            return(FALSE);
        }
    }
}

/******************************************************************************/
/********** Glue together a sequence of arguments into a generic list: ********/
/******************************************************************************/

static ClauseList *assemble_arguments(int arity, ...)
{
    ClauseList *first = NULL, *last = NULL, *tmp;
    va_list argp;
    int i;

    va_start(argp, arity);

    for (i = 0; i < arity; i++) {
        if ((tmp = (ClauseList *)malloc(sizeof(ClauseList))) != NULL) {
            tmp->head  = (void *)va_arg(argp, ClauseType *);
            tmp->tail = NULL;
            if (first == NULL) {
                first = tmp;
                last = tmp;
            }
            else {
                last->tail = tmp;
                last = last->tail;
            }
        }
        else {
            lib_error_report(ERROR_MALLOC_FAILED, "while building argument list");
        }
    }

    va_end(argp);
    return(first);
}

static void make_term(ClauseType *term, char *functor_name, ClauseList *arguments)
{
    pl_clause_build(term, COMPLEX, functor_name, 0);

    pl_arity_set(term, clause_list_length(arguments));
    pl_arguments_set(term, arguments);
}

/******************************************************************************/
/*-------- Forward Declarations: ---------------------------------------------*/

static int fparse_term(FILE *fp, Token *current, char *message, int n, ClauseType *term);
static int sparse_term(char *s,  Token *current, char *message, int n, ClauseType *term);

/******************************************************************************/
/*                                                                            */
/*    PARSE_LIST: Parse a (possibly empty) list (using recursive decent)      */
/*                                                                            */
/*    list ---> [ ]                                                           */
/*              [ list_expr ]                                                 */
/*                                                                            */
/*    list_expr --> subterm(999)                                              */
/*                  subterm(999) , list_expr                                  */
/*                  subterm(999) | subterm(999)                               */
/*                                                                            */
/*    We assume that the opening `[' has been consumed. Baring syntax         */
/*    errors, on return current should point to the matching ']'.             */

static int fparse_list(FILE *fp, Token *current, ClauseType *answer)
{
    ClauseList *arguments;
    int j = 0;

    if (current->token_type == RIGHT_SQ_BRACKET) {
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building list");
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list head");
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->tail = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list tail");
        free(arguments->head);
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->tail->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list tail's head");
        free(arguments->head);
        free(arguments->tail);
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else {
        pl_clause_type_set(answer, COMPLEX);
        pl_functor_set(answer, string_copy("."));
        pl_arity_set(answer, 2);

        j += fparse_term(fp, current, "Internal Error: No tokens found in non-empty list!", 999, arguments->head);
        if ((current->token_type == PL_SYM) && (strcmp(current->token_name, ",") == 0)) {
            free(current->token_name);
            j += fscan_token(fp, current);
            j += fparse_list(fp, current, arguments->tail->head);
        }
        else if (current->token_type == PL_LIST_SEP) {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after list separator", 999, arguments->tail->head);
        }
        else {
            pl_clause_build(arguments->tail->head, EMPTY_LIST);
        }
        arguments->tail->tail = NULL;

        pl_arguments_set(answer, arguments);
    }
    return(j);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

static int sparse_list(char *s, Token *current, ClauseType *answer)
{
    ClauseList *arguments;
    int j = 0;

    if (current->token_type == RIGHT_SQ_BRACKET) {
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building list");
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list head");
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->tail = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list tail");
        free(arguments->head);
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else if ((arguments->tail->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating list tail's head");
        free(arguments->head);
        free(arguments->tail);
        free(arguments);
        pl_clause_build(answer, EMPTY_LIST);
    }
    else {
        pl_clause_type_set(answer, COMPLEX);
        pl_functor_set(answer, string_copy("."));
        pl_arity_set(answer, 2);

        j += sparse_term(&s[j], current, "Internal Error: No tokens found in non-empty list!", 999, arguments->head);
        if ((current->token_type == PL_SYM) && (strcmp(current->token_name, ",") == 0)) {
            free(current->token_name);
            j += sscan_token(&s[j], current);
            j += sparse_list(&s[j], current, arguments->tail->head);
        }
        else if (current->token_type == PL_LIST_SEP) {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after list separator", 999, arguments->tail->head);
        }
        else {
            pl_clause_build(arguments->tail->head, EMPTY_LIST);
        }
        arguments->tail->tail = NULL;

        pl_arguments_set(answer, arguments);
    }
    return(j);
}

/******************************************************************************/
/*                                                                            */
/*    PARSE_ARGUMENTS: Parse a comma separated sequence of arguments (using   */
/*    recursive decent)                                                       */
/*                                                                            */
/*    arguments ---> subterm(999)                                             */
/*                   subterm(999) , arguments                                 */
/*                                                                            */
/*    We assume that the opening `(' has been consumed. Baring syntax         */
/*    errors, on return current should point to the matching ')'.             */

static int fparse_arguments(FILE *fp, Token *current, ClauseList **list)
{
    int j = 0;

    if ((*list = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building argument list");
    }
    else if (((*list)->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building argument list head");
        free(*list);
        *list = NULL;
    }
    else {
        j += fparse_term(fp, current, "No tokens found in argument list", 999, (*list)->head);
        if ((current->token_type == PL_SYM) && (strcmp(current->token_name, ",") == 0)) {
            free(current->token_name);
            j += fscan_token(fp, current);
            j += fparse_arguments(fp, current, &((*list)->tail));
        }
        else {
            (*list)->tail = NULL;
        }
    }
    return(j);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

static int sparse_arguments(char *s, Token *current, ClauseList **list)
{
    int j = 0;

    if ((*list = (ClauseList *)malloc(sizeof(ClauseList))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building argument list");
    }
    else if (((*list)->head = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building argument list head");
        free(*list);
        *list = NULL;
    }
    else {
        j += sparse_term(&s[j], current, "No tokens found in argument list", 999, (*list)->head);
        if ((current->token_type == PL_SYM) && (strcmp(current->token_name, ",") == 0)) {
            free(current->token_name);
            j += sscan_token(&s[j], current);
            j += sparse_arguments(&s[j], current, &((*list)->tail));
        }
        else {
            (*list)->tail = NULL;
        }
    }
    return(j);
}

/******************************************************************************/
/*                                                                            */
/*   PARSE_BASE_TERM: Parse a term of precedence 0 (using recursive decent)   */
/*                                                                            */
/*   term(0) --> functor ( arguments )                                        */
/*               ( subterm(1200) )                                            */
/*               { subterm(1200) }                                            */
/*               list                                                         */
/*               string                                                       */
/*               constant                                                     */
/*               variable                                                     */
/*                                                                            */
/* When this proc is called, answer is pointing to preallocated space         */

static int fparse_base_term(FILE *fp, Token *current, ClauseType *answer)
{
    int j = 0;

    switch (current->token_type) {
        case PL_FUNCTOR: {
            ClauseList *arguments = NULL;
            char *ctn = current->token_name;

            if (((j = fscan_token(fp, current)) > 0) && (current->token_type == LEFT_RD_BRACKET)) {
                j += fscan_token(fp, current);
                j += fparse_arguments(fp, current, &arguments);
            }
            else {
                /* This shouldn't happen: functors are always followed by an "(" */
                lib_error_report(ERROR_INTERNAL, "while parsing a malformed Prolog functor");
            }
            make_term(answer, ctn, arguments);
            if (current->token_type == RIGHT_RD_BRACKET) {
                j += fscan_token(fp, current);
            }
            else {
                fsyntax_error("Inserting \")\" to terminate arguments of functor");
            }
            break;
        }
        case LEFT_RD_BRACKET: {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after opening bracket", 1200, answer);
            if (current->token_type == RIGHT_RD_BRACKET) {
                j += fscan_token(fp, current);
            }
            else {
                fsyntax_error("Inserting \")\" to terminate subterm");
            }
            break;
        }
        case LEFT_PARENTHESIS: {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after opening parenthesis", 1200, answer);
            if (current->token_type == RIGHT_PARENTHESIS) {
                j += fscan_token(fp, current);
            }
            else {
                fsyntax_error("Inserting \"}\" to terminate subterm");
            }
            pl_clause_build(answer, COMPLEX, string_copy("{}"), 1, answer);
            break;
        }
        case LEFT_SQ_BRACKET: {
            j += fscan_token(fp, current);
            j += fparse_list(fp, current, answer);
            if (current->token_type == RIGHT_SQ_BRACKET) {
                j += fscan_token(fp, current);
            }
            else {
                fsyntax_error("Inserting \"]\" to terminate list");
            }
            break;
        }
        case PL_QUOTED_CHARS: {
            pl_clause_build(answer, STRING, current->token_name);
            j += fscan_token(fp, current);
            break;
        }
        case PL_VAR: {
            pl_clause_build(answer, VAR, current->token_name);
            j += fscan_token(fp, current);
            break;
        }
        case PL_INT: {
            pl_clause_build(answer, INT_NUMBER, current->number);
            j += fscan_token(fp, current);
            break;
        }
        case PL_REAL: {
            pl_clause_build(answer, REAL_NUMBER, current->real);
            j += fscan_token(fp, current);
            break;
        }
        case PL_CUT: {
            pl_clause_build(answer, COMPLEX, string_copy("!"), 0);
            j += fscan_token(fp, current);
            break;
        }
        case PL_SYM: {
            pl_clause_build(answer, COMPLEX, current->token_name, 0);
            j += fscan_token(fp, current);
            break;
        }
        case RIGHT_RD_BRACKET:
        case RIGHT_SQ_BRACKET:
        case RIGHT_PARENTHESIS: {
            fsyntax_error("Term expected, but closing bracket/brace found");
            pl_clause_build(answer, NULL_TERM);
            j += fscan_token(fp, current);
            break;
        }
        case PL_LIST_SEP: {
            fsyntax_error("Ignoring \"|\" found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += fscan_token(fp, current);
            break;
        }
        case PL_FULL_STOP: {
            fsyntax_error("Ignoring \".\" found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += fscan_token(fp, current);
            break;
        }
        case PL_EOF: {
            fsyntax_error("Ignoring EOF found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += fscan_token(fp, current);
            break;
        }
    }
    return(j);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

static int sparse_base_term(char *s, Token *current, ClauseType *answer)
{
    int j = 0;

    switch (current->token_type) {
        case PL_FUNCTOR: {
            ClauseList *arguments = NULL;
            char *ctn = current->token_name;

            if (((j = sscan_token(&s[j], current)) > 0) && (current->token_type == LEFT_RD_BRACKET)) {
                j += sscan_token(&s[j], current);
                j += sparse_arguments(&s[j], current, &arguments);
            }
            else {
                /* This shouldn't happen: functors are always followed by in "(" */
                lib_error_report(ERROR_INTERNAL, "while parsing a malformed Prolog functor");
	    }
            make_term(answer, ctn, arguments);
            if (current->token_type == RIGHT_RD_BRACKET) {
                j += sscan_token(&s[j], current);
            }
            else {
                ssyntax_error("Inserting \")\" to terminate arguments of functor");
            }
            break;
        }
        case LEFT_RD_BRACKET: {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after opening bracket", 1200, answer);
            if (current->token_type == RIGHT_RD_BRACKET) {
                j += sscan_token(&s[j], current);
            }
            else {
                ssyntax_error("Inserting \")\" to terminate subterm");
            }
            break;
        }
        case LEFT_PARENTHESIS: {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after opening parenthesis", 1200, answer);
            if (current->token_type == RIGHT_PARENTHESIS) {
                j += sscan_token(&s[j], current);
            }
            else {
                ssyntax_error("Inserting \"}\" to terminate subterm");
            }
            pl_clause_build(answer, COMPLEX, string_copy("{}"), 1, answer);
            break;
        }
        case LEFT_SQ_BRACKET: {
            j += sscan_token(&s[j], current);
            j += sparse_list(&s[j], current, answer);
            if (current->token_type == RIGHT_SQ_BRACKET) {
                j += sscan_token(&s[j], current);
            }
            else {
                ssyntax_error("Inserting \"]\" to terminate list");
            }
            break;
        }
        case PL_QUOTED_CHARS: {
            pl_clause_build(answer, STRING, current->token_name);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_VAR: {
            pl_clause_build(answer, VAR, current->token_name);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_INT: {
            pl_clause_build(answer, INT_NUMBER, current->number);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_REAL: {
            pl_clause_build(answer, REAL_NUMBER, current->real);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_CUT: {
            pl_clause_build(answer, COMPLEX, string_copy("!"), 0);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_SYM: {
            pl_clause_build(answer, COMPLEX, current->token_name, 0);
            j += sscan_token(&s[j], current);
            break;
        }
        case RIGHT_RD_BRACKET:
        case RIGHT_SQ_BRACKET:
        case RIGHT_PARENTHESIS: {
            ssyntax_error("Term expected, but closing bracket/brace found");
            pl_clause_build(answer, NULL_TERM);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_LIST_SEP: {
            ssyntax_error("Ignoring \"|\" found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_FULL_STOP: {
            ssyntax_error("Ignoring \".\" found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += sscan_token(&s[j], current);
            break;
        }
        case PL_EOF: {
            ssyntax_error("Ignoring EOF found while parsing term");
            pl_clause_build(answer, NULL_TERM);
            j += sscan_token(&s[j], current);
            break;
        }
    }
    return(j);
}

/******************************************************************************/
/*-------- Forward Declarations: ---------------------------------------------*/

static int fparse_post_base_term(FILE *fp, ClauseType *pre_term, Token *token, int m, int n, ClauseType *answer);
static int sparse_post_base_term(char *s, ClauseType *pre_term, Token *token, int m, int n, ClauseType *answer);

/******************************************************************************/
/*                                                                            */
/*    PARSE_TERM: Parse a complete term (possibly including subterm trees     */
/*    consisting of operators), using recursive decent                        */
/*                                                                            */
/*    fparse_term must not consume anything past the term (such as a `.')     */
/*                                                                            */
/*    fparse_term(FILE *fp, Token *current, char *message, int n, ClauseType *answer)             */
/*                                                                            */
/*    Arguments:                                                              */
/*        FILE *fp              The file containing the token stream          */
/*        Token *current        The token now in view (and presumably the     */
/*                              first token of the term)                      */
/*        char *message         The error message to print should something   */
/*                              go wrong                                      */
/*        int n                 The maximum precedence of the term            */
/*        ClauseType *answer    Pointer to allocated ClauseType for returning */
/*                              the answer                                    */

static int fparse_term(FILE *fp, Token *current, char *message, int n, ClauseType *answer)
{
    ClauseType *subterm;
    OperatorType *op;
    int m = 0, j = 0;

    if ((subterm  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building subterm");
    }
    else if ((op = lookup_operator(current, PREFIX)) != NULL) {
        char *functor = current->token_name;

        j += fscan_token(fp, current);
        if ((op->op_type == FY) && (op->op_precedence <= n) && (potential_subterm(current, op->op_precedence))) {
            ClauseType *arg;
            if ((arg  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
                lib_error_report(ERROR_MALLOC_FAILED, "while building FY argument");
            }
            else {
                m = op->op_precedence;
                j += fparse_term(fp, current, "No tokens found after prefix operator", m, arg);
            }
            pl_clause_build(subterm, COMPLEX, functor, 1, arg);
        }
	else if ((op->op_type == FX) && (op->op_precedence <= n) && (potential_subterm(current, op->op_precedence-1))) {
            ClauseType *arg;
            if ((arg  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
                lib_error_report(ERROR_MALLOC_FAILED, "while building FX argument");
            }
            else {
                m = op->op_precedence;
                j += fparse_term(fp, current, "No tokens found after prefix operator", m-1, arg);
            }
            pl_clause_build(subterm, COMPLEX, functor, 1, arg);
	}
        else {
            /* Parse the operator as a stand-alone term */
            pl_clause_build(subterm, COMPLEX, functor, 0);
        }
    }
    else if (lookup_operator(current, INFIX) != NULL) {
        /* We have a stand-alone infix operator */
        pl_clause_build(subterm, COMPLEX, current->token_name, 0);
        j += fscan_token(fp, current);
    }
    else if (lookup_operator(current, POSTFIX) != NULL) {
        /* We have a stand-alone postfix operator */
        pl_clause_build(subterm, COMPLEX, current->token_name, 0);
        j += fscan_token(fp, current);
    }
    else {
        /* We have a "normal" term */
        j += fparse_base_term(fp, current, subterm);
    }
    j += fparse_post_base_term(fp, subterm, current, m, n, answer);
    return(j);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

static int sparse_term(char *s, Token *current, char *message, int n, ClauseType *answer)
{
    ClauseType *subterm;
    OperatorType *op;
    int m = 0, j = 0;

    if ((subterm  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building subterm");
    }
    else if ((op = lookup_operator(current, PREFIX)) != NULL) {
        char *functor = current->token_name;

        j += sscan_token(&s[j], current);
	if ((op->op_type == FY) && (op->op_precedence <= n) && (potential_subterm(current, op->op_precedence))) {
            ClauseType *arg;
            if ((arg  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
                lib_error_report(ERROR_MALLOC_FAILED, "while building FY argument");
            }
            else {
                m = op->op_precedence;
                j += sparse_term(&s[j], current, "No tokens found after prefix operator", m, arg);
            }
            pl_clause_build(subterm, COMPLEX, functor, 1, arg);
        }
	else if ((op->op_type == FX) && (op->op_precedence <= n) && (potential_subterm(current, op->op_precedence-1))) {
            ClauseType *arg;
            if ((arg  = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
                lib_error_report(ERROR_MALLOC_FAILED, "while building FX argument");
            }
            else {
                m = op->op_precedence;
                j += sparse_term(&s[j], current, "No tokens found after prefix operator", m-1, arg);
            }
            pl_clause_build(subterm, COMPLEX, functor, 1, arg);
	}
        else {
            /* Parse the operator as a stand-alone term */
            pl_clause_build(subterm, COMPLEX, functor, 0);
        }
    }
    else if (lookup_operator(current, INFIX) != NULL) {
        /* We have a stand-alone infix operator */
        pl_clause_build(subterm, COMPLEX, current->token_name, 0);
        j += sscan_token(&s[j], current);
    }
    else if (lookup_operator(current, POSTFIX) != NULL) {
        /* We have a stand-alone postfix operator */
        pl_clause_build(subterm, COMPLEX, current->token_name, 0);
        j += sscan_token(&s[j], current);
    }
    else {
        /* We have a "normal" term */
        j += sparse_base_term(&s[j], current, subterm);
    }
    j += sparse_post_base_term(&s[j], subterm, current, m, n, answer);

    return(j);
}

/******************************************************************************/
/*                                                                            */
/*    PARSE_POST_BASE_TERM: Parse anything that follows a term(m)             */
/*                                                                            */
/*    If the current token is a postfix or infix operator of precedence < n,  */
/*    then the term just parsed is actually a subterm of something bigger,    */
/*    so we need to parse the rest of the term.                               */
/*                                                                            */
/*    Arguments:                                                              */
/*        ClauseType *pre_term  The term already parsed                       */
/*        Token *token          The token immediately following pre_term      */
/*        int m                 The precedence of pre_term                    */
/*        int n                 The maximum precedence of the result          */
/*        ClauseType *answer    Pointer to allocated ClauseType for returning */
/*                              the answer                                    */

static int fparse_post_base_term(FILE *fp, ClauseType *pre_term, Token *current, int m, int n, ClauseType *answer)
{
    ClauseType *term, *post_term;
    OperatorType *op;
    ClauseList *arguments = NULL;
    int p, j = 0;

    char *functor = current->token_name;

    if (((op = lookup_operator_in_term(current, POSTFIX)) != NULL) && (op->op_type == YF) && ((p = op->op_precedence) >= m) && (p <= n)) {
        arguments =  assemble_arguments(1, pre_term);
        j += fscan_token(fp, current);
    }
    else if (((op = lookup_operator_in_term(current, POSTFIX)) != NULL) && (op->op_type == XF) && ((p = op->op_precedence) > m) && (p <= n)) {
        arguments = assemble_arguments(1, pre_term);
        j += fscan_token(fp, current);
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == YFX) && ((p = op->op_precedence) >= m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building post-term");
        }
        else {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after infix operator", p-1, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == XFX) && ((p = op->op_precedence) > m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building XFX term");
        }
        else {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after infix operator", p-1, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == XFY) && ((p = op->op_precedence) > m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building XFY term");
        }
        else {
            j += fscan_token(fp, current);
            j += fparse_term(fp, current, "No tokens found after infix operator", p, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else {
        pl_clause_type_set(answer, pl_clause_type(pre_term));
        pl_functor_set(answer, pl_functor(pre_term));
        pl_arity_set(answer, pl_arity(pre_term));
        pl_integer_set(answer, pl_integer(pre_term));
        pl_double_set(answer, pl_double(pre_term));
        pl_arguments_set(answer, pl_arguments(pre_term));
        free(pre_term);

        return(j);
    }
    if ((term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building term");
    }
    else {
        make_term(term, functor, arguments);
    }
    j += fparse_post_base_term(fp, term, current, p, n, answer);
    return(j);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

static int sparse_post_base_term(char *s, ClauseType *pre_term, Token *current, int m, int n, ClauseType *answer)
{
    ClauseType *term, *post_term;
    OperatorType *op;
    ClauseList *arguments = NULL;
    int p, j = 0;

    char *functor = current->token_name;

    if (((op = lookup_operator_in_term(current, POSTFIX)) != NULL) && (op->op_type == YF) && ((p = op->op_precedence) >= m) && (p <= n)) {
        arguments =  assemble_arguments(1, pre_term);
        j += sscan_token(&s[j], current);
    }
    else if (((op = lookup_operator_in_term(current, POSTFIX)) != NULL) && (op->op_type == XF) && ((p = op->op_precedence) > m) && (p <= n)) {
        arguments = assemble_arguments(1, pre_term);
        j += sscan_token(&s[j], current);
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == YFX) && ((p = op->op_precedence) >= m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building post-term");
        }
        else {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after infix operator", p-1, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == XFX) && ((p = op->op_precedence) > m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building XFX term");
        }
        else {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after infix operator", p-1, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else if (((op = lookup_operator_in_term(current, INFIX)) != NULL) && (op->op_type == XFY) && ((p = op->op_precedence) > m) && (p <= n)) {
        if ((post_term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
            lib_error_report(ERROR_MALLOC_FAILED, "while building XFY term");
        }
        else {
            j += sscan_token(&s[j], current);
            j += sparse_term(&s[j], current, "No tokens found after infix operator", p, post_term);
            arguments = assemble_arguments(2, pre_term, post_term);
        }
    }
    else {
        pl_clause_type_set(answer, pl_clause_type(pre_term));
        pl_functor_set(answer, pl_functor(pre_term));
        pl_arity_set(answer, pl_arity(pre_term));
        pl_integer_set(answer, pl_integer(pre_term));
        pl_double_set(answer, pl_double(pre_term));
        pl_arguments_set(answer, pl_arguments(pre_term));
        free(pre_term);
        return(j);
    }
    if ((term = (ClauseType *)malloc(sizeof(ClauseType))) == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while building term");
    }
    else {
        make_term(term, functor, arguments);
    }
    j += sparse_post_base_term(&s[j], term, current, p, n, answer);
    return(j);
}

/******************************************************************************/
/* Recurse through a clause replacing subclauses like -(3) with the           */
/* equivalent purely numerical terms.                                         */
/******************************************************************************/

static void check_unary_arithmetic_operators(ClauseType *answer)
{
    /* First check all arguments (in a head-recursive sort of way) */

    if (pl_clause_type(answer) == COMPLEX) {
        ClauseList *args;
        for (args = pl_arguments(answer); args != NULL; args = args->tail) {
            check_unary_arithmetic_operators(args->head);
        }
    }

    /* Now, if we are left with a unary + or - followed by a number ... */

    if (functor_comp(answer, "-", 1)) {
        ClauseType *arg = pl_arg_get(answer, 1);
        if ((pl_clause_type(arg) == INT_NUMBER) || (pl_clause_type(arg) == REAL_NUMBER)) {
            pl_clause_type_set(answer, pl_clause_type(arg));
            pl_functor_free(answer);
            pl_functor_set(answer, NULL);
            pl_arity_set(answer, 0);
            pl_integer_set(answer, -1 * pl_integer(arg));
            pl_double_set(answer, -1.0 * pl_double(arg));
            free(pl_arguments(answer));
            pl_arguments_set(answer, NULL);
            pl_clause_free(arg);
        }
    }
    else if (functor_comp(answer, "+", 1)) {
        ClauseType *arg = pl_arg_get(answer, 1);
        if ((pl_clause_type(arg) == INT_NUMBER) || (pl_clause_type(arg) == REAL_NUMBER)) {
            pl_clause_type_set(answer, pl_clause_type(arg));
            pl_functor_free(answer);
            pl_functor_set(answer, NULL);
            pl_arity_set(answer, 0);
            pl_integer_set(answer, pl_integer(arg));
            pl_double_set(answer, pl_double(arg));
            free(pl_arguments(answer));
            pl_arguments_set(answer, NULL);
            pl_clause_free(arg);
        }
    }
}

/******** Fix for ext_float/3 (to maintain precision of floats in files): *****/

static void massage_floats(ClauseType *clause)
{
    if (functor_comp(clause, "ext_float", 3)) {
        ClauseType *value = pl_arg_get(clause, 1);
        if ((pl_clause_type(value) == REAL_NUMBER) || (pl_clause_type(value) == INT_NUMBER)) {
            pl_functor_free(clause);
            pl_functor_set(clause, NULL);
            pl_clause_type_set(clause, pl_clause_type(value));
            pl_double_set(clause, pl_double(value));
            pl_integer_set(clause, pl_integer(value));
            pl_clause_list_free(pl_arguments(clause));
            pl_arguments_set(clause, NULL);
        }
        else if (functor_comp(value, "-", 1)) {
            ClauseType *subvalue = pl_arg_get(value, 1);
            if ((pl_clause_type(subvalue) == REAL_NUMBER) || (pl_clause_type(subvalue) == INT_NUMBER)) {
                pl_clause_type_set(clause, pl_clause_type(subvalue));
                pl_double_set(clause, -1.0 * pl_double(subvalue));
                pl_integer_set(clause, -1 * pl_integer(subvalue));
                pl_functor_free(clause);
                pl_functor_set(clause, NULL);
                pl_clause_list_free(pl_arguments(clause));
                pl_arguments_set(clause, NULL);
            }
        }
    }
    else if (pl_clause_type(clause) == COMPLEX) {
        ClauseList *tmp;
        for (tmp = pl_arguments(clause); tmp != NULL; tmp = tmp->tail) {
            massage_floats((ClauseType *)tmp->head);
        }
    }
}

/******************************************************************************/
/******** Parse and return a clause from the file pointed to by fp: ***********/
/************** (assumes answer points to pre-allocated space) ****************/
/******************************************************************************/

int fscan_clause(FILE *fp, ClauseType *answer)
{
    Token      current;
    int        j  = 0;

    if (((j = fscan_token(fp, &current)) > 0) && (current.token_type != PL_EOF)) {
        j += fparse_term(fp, &current, "Internal Error: No tokens found", 1200, answer);
        if (current.token_type != PL_FULL_STOP) {
            fsyntax_error("Inserted missing \".\"");
        }
        check_unary_arithmetic_operators(answer);
        massage_floats(answer);
        return(j);
    }
    else {
        return(0);
    }
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

int sscan_clause(char *s, ClauseType *answer)
{
    int        tmp_char = pl_context_char_count;
    char      *tmp_file = pl_context_filename;
    Token      current;
    int        j = 0;

    pl_context_char_count = 0;
    pl_context_filename = s;

    if (((j = sscan_token(&s[j], &current)) > 0) && (current.token_type != PL_EOF)) {
        j += sparse_term(&s[j], &current, "Internal Error: No tokens found", 1200, answer);
        if (current.token_type != PL_FULL_STOP) {
            ssyntax_error("Inserted missing \".\"");
        }
        massage_floats(answer);
        check_unary_arithmetic_operators(answer);
    }

    pl_context_char_count = tmp_char;
    pl_context_filename = tmp_file;

    if (current.token_type == PL_EOF) {
        return(0);
    }
    else {
        return(j);
    }
}

/******************************************************************************/
/**************************  E N D   O F   F I L E  ***************************/
/******************************************************************************/
