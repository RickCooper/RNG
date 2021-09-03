/*******************************************************************************

    File:       pl.h
    Contents:   Type definitions and function declarations for COGENT's Prolog interface
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

*******************************************************************************/

#ifndef _pl_h

#define _pl_h

#include "lib_error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char _flsbuf();
extern char _filbuf();

#define CLAUSE_LENGTH                 4096

/********* Type defs for Prolog terms: ****************************************/

/* Operators: */

typedef enum {PREFIX, INFIX, POSTFIX} OpClass;
typedef enum {FX, FY, XF, YF, XFX, XFY, YFX} TypeOfOperator;

typedef struct {
    char           *op_name;
    int             op_precedence;
    TypeOfOperator  op_type;
} OperatorType;

/* Tokens: */

typedef enum {PL_SYM, PL_FUNCTOR, PL_VAR, PL_INT, PL_REAL, PL_CUT, PL_LIST_SEP,
    LEFT_RD_BRACKET, RIGHT_RD_BRACKET, LEFT_SQ_BRACKET, RIGHT_SQ_BRACKET,
    LEFT_PARENTHESIS, RIGHT_PARENTHESIS, PL_QUOTED_CHARS, PL_FULL_STOP,
    PL_EOF} TypeOfToken;

typedef struct {
    TypeOfToken      token_type;
    char            *token_name;
    long             number;
    double           real;
} Token;

/* Clauses: */

typedef enum {NULL_TERM, COMPLEX, EMPTY_LIST, VAR, INT_NUMBER, REAL_NUMBER, STRING} TypeOfTerm;

typedef struct clause_list {
    struct clause        *head;
    struct clause_list   *tail;
} ClauseList;

typedef struct clause {
    TypeOfTerm      sort;
    char           *functor;
    int             arity;
    long            number;
    double          real;
    ClauseList     *args;
} ClauseType;

/******************************************************************************/

/* Global variables defined in pl_scan.c: */

extern int             pl_context_char_count;
extern char           *pl_context_filename;
extern int             pl_context_line_count;

/* From pl_misc.c: */

extern void        pl_clause_free(ClauseType *clause);
extern ClauseType *pl_clause_copy(ClauseType *clause);
extern int         pl_clause_embed(ClauseType *clause, const char *functor);
extern void        pl_clause_swap(ClauseType *c1, ClauseType *c2);
extern ClauseType *pl_clause_from_list(ClauseList *input);
extern ClauseType *pl_clause_make_from_string(char *string);
extern int         pl_clause_build(ClauseType *term, TypeOfTerm tt, ...);
extern int         pl_clause_compare(ClauseType *clause1, ClauseType *clause2);
extern void        pl_clause_list_free(ClauseList *clauses);
extern void        pl_clause_list_free_links(ClauseList *clauses);
extern int         pl_clause_list_length(ClauseList *clauses);
extern ClauseList *pl_clause_list_copy(ClauseList *clauses);
extern ClauseList *pl_clause_list_reverse(ClauseList *terms);
extern ClauseList *pl_clause_list_prepend_element(ClauseList *list, ClauseType *head);
extern ClauseList *pl_clause_list_add_element_to_tail(ClauseList *cl, ClauseType *element);
extern ClauseList *pl_clause_list_read_from_file(char *filename, ClauseList *declarations, ErrorType *err);
extern int         pl_clause_list_write_to_file(ClauseList *list, char *filename, char *description);
extern int         functor_comp(ClauseType *clause, const char *name, int arity);
extern ClauseType *pl_arg_get(ClauseType *clause, int n);
extern int         pl_arg_set(ClauseType *clause, int n, ClauseType *arg);
extern void        pl_arg_set_to_int(ClauseType *clause, int arg, long val);
extern void        pl_arg_set_to_real(ClauseType *clause, int arg, double val);
extern void        pl_arg_set_to_string(ClauseType *clause, int arg, char *string);
extern int         pl_functor_replace(ClauseType *clause, const char *new_functor);
extern int         pl_arity_adjust(ClauseType *clause, int arity);
extern int         pl_list_length(ClauseType *list);
extern ClauseType *pl_list_assemble(ClauseType *head, ClauseType *tail);
extern ClauseType *pl_list_append(ClauseType *embedded_list, ClauseType *tail);
extern int         pl_list_add_to_tail(ClauseType *list, ClauseType *new_element);
extern int         pl_list_delete_head(ClauseType *list);
extern int         pl_list_write_to_file(ClauseType *list, char *filename, char *description);
extern char       *pl_list_to_char_string(ClauseType *clause);
extern int         pl_is_list(ClauseType *clause);
extern int         pl_is_real(ClauseType *clause, double *r);
extern int         pl_is_integer(ClauseType *clause, long *i);
extern int         pl_is_number(ClauseType *clause, double *r);
extern int         pl_is_atom(ClauseType *clause, char **s);
extern int         pl_is_variable(ClauseType *clause, char **s);
extern int         pl_is_string(ClauseType *clause, char **s);

extern int pl_functor_is_qualifier(ClauseType *condition);
extern int pl_functor_is_term_comparison(ClauseType *clause);

extern ClauseType *pl_coordinates_make(short x, short y);

// extern long       *evaluate_integer_expression(ClauseType *clause);
// extern double     *evaluate_real_expression(ClauseType *clause);

/* From pl_operators.c: */

extern int    pl_operator_declare(ClauseType *dec);
extern void   pl_operator_table_destroy();
extern void   pl_operator_table_initialise();
extern int    load_oos_operator_declarations(char *ooslib);
extern OperatorType *lookup_operator(Token *token, OpClass op_class);
extern OperatorType *lookup_operator_in_term(Token *token, OpClass op_class);
extern int    pl_operator_declarations_push(ClauseList *declarations);
extern void   pl_operator_declarations_pop(int count);

/* From pl_parse.c: */

extern int fscan_clause(FILE *fp, ClauseType *answer);
extern int sscan_clause(char *s, ClauseType *answer);

/* From pl_print.c: */

extern int  fprint_clause(FILE *fp, ClauseType *clause);
extern int  sprint_clause(char *s, ClauseType *clause);
extern int  fprint_clause_without_quotes(FILE *fp, ClauseType *clause);
extern int  sprint_clause_without_quotes(char *s, ClauseType *clause);
extern int  fprint_clause_without_extfloats(FILE *fp, ClauseType *clause);
extern int  sprint_clause_without_extfloats(char *s, ClauseType *clause);
extern void pl_set_output_precision(int p);
extern int  fprint_string_as_atom(FILE *fp, const char *string);

/* From pl_scan.c: */

extern int   pl_is_alpha(int c);
extern int   pl_is_symbol(int c);
extern int   pl_is_punctuation(int c);

extern int fscan_token(FILE *fp, Token *symbol);
extern int sscan_token(char *s, Token *symbol);
extern char *fscan_to_eoln(FILE *fp);

/* Defined elsewhere but used in pl_*.c: */

extern char    *string_new(int l);
extern char    *string_copy(const char *s);

/******************************************************************************/

#ifdef MALLOC_CHECK
#include "zmalloc.h"
#endif

/******************************************************************************/
/* Access functions for the ClauseType data structure:                        */

#define pl_functor(A)             ((A)->functor)
#define pl_functor_set(A, B)      ((A)->functor = B)
#define pl_functor_free(A)        (free((A)->functor))

#define pl_arity(A)               ((A)->arity)
#define pl_arity_set(A, B)        ((A)->arity = B)

#define pl_clause_type(A)         ((A)->sort)
#define pl_clause_type_set(A, B)  ((A)->sort = B)

#define pl_integer(A)             ((A)->number)
#define pl_integer_set(A, B)      ((A)->number = B)

#define pl_double(A)              ((A)->real)
#define pl_double_set(A, B)       ((A)->real = B)

#define pl_arguments(A)           ((A)->args)
#define pl_arguments_set(A, B)    ((A)->args = B)

#endif

