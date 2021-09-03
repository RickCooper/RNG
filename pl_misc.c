/*******************************************************************************

    File:       pl_misc.c
    Contents:   Miscellaneous routines for interfacing with pl data structures
    Author:     Rick Cooper
    Copyright (c) 1998 Richard P. Cooper

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

    These routines build and access various prolog data structures. There
    are two forms of error handling: most functions return TRUE if there
    were no errors; some print an error using one of the pl_error functions.
    These functions attempt to tie the error message to the appropriate
    window.

    Problems:
    1) Many other parts of COGENT ignore the return value, so errors may not
    be reported.

*******************************************************************************/

#include "pl.h"
#include "lib_file.h"
#include "lib_error.h"
#include <time.h>
#include <glib.h>
#include <ctype.h>

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#include <stdarg.h>

#define WB_LENGTH 256
char warning_buffer[WB_LENGTH];

#ifdef MALLOC_CHECK
#include "../lib/zmalloc.h"
#endif

/******************************************************************************/
/***************** Operations on/returning Clause structures ******************/

/* Free a Clause: ------------------------------------------------------------*/

void pl_clause_free(ClauseType *clause)
{
    if (clause != NULL) {
        pl_clause_list_free(pl_arguments(clause));
        if (pl_functor(clause) != NULL) {
            pl_functor_free(clause);
        }
        free(clause);
    }
}

/* Copy a Clause: ------------------------------------------------------------*/

ClauseType *pl_clause_copy(ClauseType *clause)
{
    ClauseType *copy;
    ClauseList first_arg, *last_arg, *tmp;
    char *str;

    if (clause == NULL) {
        return(NULL);
    }
    else if ((copy = (ClauseType *) malloc(sizeof(ClauseType))) == NULL) {
        return(NULL);          // error: failed malloc
    }
    else {
        pl_clause_type_set(copy, pl_clause_type(clause));
        pl_arity_set(copy, pl_arity(clause));
        pl_integer_set(copy, pl_integer(clause));
        pl_double_set(copy, pl_double(clause));
        if (pl_functor(clause) == NULL) {
            pl_functor_set(copy, NULL);
        }
        else if ((str = string_new(1 + strlen(pl_functor(clause)))) == NULL) {
            free(copy);
            return(NULL);      // error: failed malloc
        }
        else {
            strcpy(str, pl_functor(clause));
            pl_functor_set(copy, str);
        }
        last_arg = &first_arg;
        last_arg->tail = NULL;
        for (tmp = pl_arguments(clause); tmp != NULL; tmp = tmp->tail) {
            last_arg->tail = (ClauseList *) malloc(sizeof(ClauseList));
            if (last_arg->tail != NULL) {
                last_arg = last_arg->tail;
                last_arg->head = pl_clause_copy(tmp->head);
                last_arg->tail = NULL;
            }
            else {
                return(NULL);  // error: failed malloc
            }
        }
        pl_arguments_set(copy, first_arg.tail);
        return(copy);
    }
}

/*----------------------------------------------------------------------------*/

int pl_clause_embed(ClauseType *clause, const char *functor)
{
    /* This procedure wraps the clause pointed to by its first argument   *
     * in (a copy of) the functor pointed to by its second argument.      *
     * Thus, if clause points to the representation of "term(arg1, arg2)" *
     * before, and functor points to the string "not", then after calling *
     * this procedure clause will point to the representation of          *
     * "not(term(arg1, arg2))"                                            */

    ClauseType *new;
    ClauseList *arguments;
    char *copy_of_functor;

    if ((new = (ClauseType *) malloc(sizeof(ClauseType))) == NULL) {
        return(FALSE);         // error: failed_malloc
    }
    else if ((arguments = (ClauseList *) malloc(sizeof(ClauseList))) == NULL) {
        free(new);
        return(FALSE);         // error: failed_malloc
    }
    else if ((copy_of_functor = string_copy(functor)) == NULL) {
        free(arguments);
        free(new);
        return(FALSE);         // error: failed_malloc
    }
    else {
        pl_clause_type_set(new, pl_clause_type(clause));
        pl_functor_set(new, pl_functor(clause));
        pl_arity_set(new, pl_arity(clause));
        pl_arguments_set(new, pl_arguments(clause));
        pl_integer_set(new, pl_integer(clause));
        pl_double_set(new, pl_double(clause));

        arguments->head = new;
        arguments->tail = NULL;

        pl_clause_type_set(clause, COMPLEX);
        pl_functor_set(clause, copy_of_functor);
        pl_arity_set(clause, 1);
        pl_arguments_set(clause, arguments);
        pl_integer_set(clause, 0);
        pl_double_set(clause, 0.0);
        return(TRUE);
    }
}

/*----------------------------------------------------------------------------*/

void pl_clause_swap(ClauseType *c1, ClauseType *c2)
{
    ClauseType tmp;

    pl_clause_type_set(&tmp, pl_clause_type(c1));
    pl_functor_set(&tmp, pl_functor(c1));
    pl_arity_set(&tmp, pl_arity(c1));
    pl_integer_set(&tmp, pl_integer(c1));
    pl_double_set(&tmp, pl_double(c1));
    pl_arguments_set(&tmp, pl_arguments(c1));

    pl_clause_type_set(c1, pl_clause_type(c2));
    pl_functor_set(c1, pl_functor(c2));
    pl_arity_set(c1, pl_arity(c2));
    pl_integer_set(c1, pl_integer(c2));
    pl_double_set(c1, pl_double(c2));
    pl_arguments_set(c1, pl_arguments(c2));

    pl_clause_type_set(c2, pl_clause_type(&tmp));
    pl_functor_set(c2, pl_functor(&tmp));
    pl_arity_set(c2, pl_arity(&tmp));
    pl_integer_set(c2, pl_integer(&tmp));
    pl_double_set(c2, pl_double(&tmp));
    pl_arguments_set(c2, pl_arguments(&tmp));
}

/*----------------------------------------------------------------------------*/

static int argument_comp(ClauseList *arguments1, ClauseList *arguments2)
{
    while ((arguments1 != NULL) && (arguments2 != NULL)) {
        if (!pl_clause_compare(arguments1->head, arguments2->head)) {
            return(FALSE);
        }
        else {
            arguments1 = arguments1->tail;
            arguments2 = arguments2->tail;
        }
    }
    return(TRUE);
}

int pl_clause_compare(ClauseType *clause1, ClauseType *clause2)
{
    if (clause1 == clause2) {
        return(TRUE);
    }
    else if ((clause1 == NULL) || (clause2 == NULL)) {
        return(FALSE);
    }
    else if (pl_clause_type(clause1) != pl_clause_type(clause2)) {
        return(FALSE);
    }
    else {
        switch (pl_clause_type(clause1)) {
        case NULL_TERM:{
                return(TRUE);
            }
        case COMPLEX:{
                if (pl_arity(clause1) != pl_arity(clause2)) {
                    return(FALSE);
                }
                else if (strcmp(pl_functor(clause1), pl_functor(clause2)) != 0) {
                    return(FALSE);
                }
                else {
                    return(argument_comp(pl_arguments(clause1), pl_arguments(clause2)));
                }
            }
        case EMPTY_LIST:{
                return(TRUE);
            }
        case VAR:{
                return(strcmp(pl_functor(clause1), pl_functor(clause2)) == 0);
            }
        case INT_NUMBER:{
                return(pl_integer(clause1) == pl_integer(clause2));
            }
        case REAL_NUMBER:{
                return(pl_double(clause1) == pl_double(clause2));
            }
        case STRING:{
                return(strcmp(pl_functor(clause1), pl_functor(clause2)) == 0);
            }
        default:{
                return(FALSE);
            }
        }
    }
}

/* Convert a ClauseList into a Prolog clause representing the list: ----------*/

ClauseType *pl_clause_from_list(ClauseList *input)
{
    if (input == NULL) {
        ClauseType *empty = (ClauseType *) malloc(sizeof(ClauseType));
        if (empty == NULL) {
            return(NULL);      // error: failed malloc
        }
        else if (pl_clause_build(empty, EMPTY_LIST)) {
            return(empty);
        }
        else {
            return(NULL);      // error: failed to biuld clause
        }
    }
    else {
        ClauseType *pl_tail = pl_clause_from_list(input->tail);
        ClauseType *pl_list = pl_list_assemble(input->head, pl_tail);
        free(input);
        return(pl_list);
    }
}

/* Make a Prolog clause from a string: ---------------------------------------*/

ClauseType *pl_clause_make_from_string(char *string)
{
    ClauseType *clause = (ClauseType *) malloc(sizeof(ClauseType));

    if (clause != NULL) {
        if (sscan_clause(string, clause) == 0) {
            free(clause);
            clause = NULL;
        }
    }
    return(clause);
}

/* Make a clause for each type of term: --------------------------------------*/

int pl_clause_build(ClauseType *term, TypeOfTerm tt, ...)
{
    va_list argp;
    int ok = TRUE;

    pl_clause_type_set(term, tt);
    pl_functor_set(term, NULL);
    pl_arity_set(term, 0);
    pl_integer_set(term, 0);
    pl_double_set(term, 0.0);
    pl_arguments_set(term, NULL);

    va_start(argp, tt);

    switch (tt) {
    case NULL_TERM:{
            break;
        }
    case EMPTY_LIST:{
            break;
        }
    case VAR:{
            pl_functor_set(term, (char *)va_arg(argp, char *));
            break;
        }
    case INT_NUMBER:{
            pl_integer_set(term, (long)va_arg(argp, long));
            break;
        }
    case REAL_NUMBER:{
            pl_double_set(term, (double)va_arg(argp, double));
            break;
        }
    case STRING:{
            pl_functor_set(term, (char *)va_arg(argp, char *));
            break;
        }
    case COMPLEX:{
            ClauseList *last = NULL, *tmp;
            int i;

            pl_functor_set(term, (char *)va_arg(argp, char *));
            pl_arity_set(term, (int)va_arg(argp, int));

            for (i = 0; i < pl_arity(term); i++) {
                if ((tmp = (ClauseList *) malloc(sizeof(ClauseList))) != NULL) {
                    tmp->head = (void *)va_arg(argp, ClauseType *);
                    tmp->tail = NULL;
                    if (pl_arguments(term) == NULL) {
                        pl_arguments_set(term, tmp);
                        last = tmp;
                    }
                    else {
                        last->tail = tmp;
                        last = last->tail;
                    }
                }
                else {
                    ok = FALSE; // error: failed malloc
                }
            }
            break;
        }
    }
    va_end(argp);
    return(ok);
}

/******************************************************************************/
/*************** Operations on/returning ClauseList structures ****************/

/* Free a ClauseList: --------------------------------------------------------*/

void pl_clause_list_free(ClauseList *clauses)
{
    while (clauses != NULL) {
        ClauseList *tmp = clauses->tail;
        pl_clause_free(clauses->head);
        free(clauses);
        clauses = tmp;
    }
}

void pl_clause_list_free_links(ClauseList *clauses)
{
    while (clauses != NULL) {
        ClauseList *tmp = clauses->tail;
        free(clauses);
        clauses = tmp;
    }
}

/* Length of a ClauseList: ---------------------------------------------------*/

int pl_clause_list_length(ClauseList *clauses)
{
    int j = 0;
    while (clauses != NULL) {
        clauses = clauses->tail;
        j++;
    }
    return(j);
}

/* Copy a ClauseList: --------------------------------------------------------*/

ClauseList *pl_clause_list_copy(ClauseList *clauses)
{
    ClauseList dummy, *tmp, *current = &dummy;

    for (tmp = clauses; tmp != NULL; tmp = tmp->tail) {
        if ((current->tail = (ClauseList *) malloc(sizeof(ClauseList))) == NULL) {
            return(NULL);      // failed malloc
        }
        else {
            current = current->tail;
            current->head = pl_clause_copy(tmp->head);
        }
    }
    current->tail = NULL;
    return(dummy.tail);
}

/* Add an element to the head of a ClauseList: -------------------------------*/

ClauseList *pl_clause_list_prepend_element(ClauseList *list, ClauseType *element)
{
    ClauseList *new = (ClauseList *) malloc(sizeof(ClauseList));

    if (new == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating node");
    }
    else {
        new->head = element;
        new->tail = list;
    }
    return(new);
}

/* Add an element to the end of a ClauseList: --------------------------------*/

ClauseList *pl_clause_list_add_element_to_tail(ClauseList *list, ClauseType *element)
{
    ClauseList *node = (ClauseList *) malloc(sizeof(ClauseList));

    if (node == NULL) {
        lib_error_report(ERROR_MALLOC_FAILED, "while creating node");
    }
    else if (element != NULL) {
        node->head = element;
        node->tail = NULL;
        if (list == NULL) {
            list = node;
        }
        else {
            ClauseList *tmp = list;
            while (tmp->tail != NULL) {
                tmp = tmp->tail;
            }
            tmp->tail = node;
        }
    }
    return(list);
}

/* Reverse the order of elements in a ClauseList: ----------------------------*/

ClauseList *pl_clause_list_reverse(ClauseList *list)
{
    ClauseList *prev = list;
    if (prev != NULL) {
        ClauseList *next = prev->tail;
        prev->tail = NULL;
        while (next != NULL) {
            ClauseList *tmp = next->tail;
            next->tail = prev;
            prev = next;
            next = tmp;
        }
    }
    return(prev);
}

/* Consult a Prolog file: ----------------------------------------------------*/

ClauseList *pl_clause_list_read_from_file(char *filename, ClauseList *declarations, ErrorType *error)
{
    FILE *fp;

    *error = ERROR_NONE;
    if (!file_exists(filename)) {
        *error = ERROR_FILE_OPEN_FAILED;
        return(NULL);
    }
    else if ((fp = fopen(filename, "r")) == NULL) {
        *error = ERROR_NO_READ_ACCESS;
        return(NULL);
    }
    else {
        ClauseType *clause = (ClauseType *) malloc(sizeof(ClauseType));
        ClauseList tmp = { NULL, NULL };
        ClauseList *list = &tmp;

        char *tmp_file = pl_context_filename;
        int tmp_line = pl_context_line_count;
        int tmp_char = pl_context_char_count;
        int dec_count;

        pl_context_filename = filename;
        pl_context_line_count = 1;
        pl_context_char_count = 0;

        dec_count = pl_operator_declarations_push(declarations);

        while ((clause != NULL) && (fscan_clause(fp, clause) > 0)) {
            if (pl_operator_declare(clause)) {
                dec_count++;
            }

            if ((list->tail = (ClauseList *) malloc(sizeof(ClauseList))) != NULL) {
                list = list->tail;
                list->head = clause;
                list->tail = NULL;
            }
            else {
                *error = ERROR_MALLOC_FAILED;
                pl_clause_free(clause);
            }
            if ((clause = (ClauseType *) malloc(sizeof(ClauseType))) == NULL) {
                *error = ERROR_MALLOC_FAILED;
            }
        }
        if (clause != NULL) {
            free(clause);
        }
        pl_context_filename = tmp_file;
        pl_context_line_count = tmp_line;
        pl_context_char_count = tmp_char;
        if (fclose(fp) != 0) {
            *error = ERROR_FILE_CLOSE_FAILED;
        }

        pl_operator_declarations_pop(dec_count);

        return(tmp.tail);
    }
}

/* Write a clause list to a file as a sequence of clauses: -------------------*/

int pl_clause_list_write_to_file(ClauseList *list, char *filename, char *description)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        return(FALSE);
    }
    else {
        int error = 0;
        time_t when;

        time(&when);
        error = (fprintf(fp, "%% %s: %s\n", description, ctime(&when)) < (strlen(description) + 4)) || error;
        while (!error && (list != NULL)) {
            error = (fprint_clause(fp, list->head) < 1) || error;
            error = (fprintf(fp, ".\n\n") < 3) || error;
            list = list->tail;
        }
        return((fclose(fp) == 0) && !error);
    }
}

/******************************************************************************/
/********************** Some list processing procedures ***********************/
/******************************************************************************/

/* Return the length of a Prolog list: ---------------------------------------*/

int pl_list_length(ClauseType *list)
{
    int n = 0;

    while (functor_comp(list, ".", 2)) {
        list = pl_arg_get(list, 2);
        n++;
    }
    if ((list == NULL) || (pl_clause_type(list) != EMPTY_LIST)) {
        return(-1);
    }
    else {
        return(n);
    }
}

/* Construct a list from a head and tail: ------------------------------------*/

ClauseType *pl_list_assemble(ClauseType *head, ClauseType *tail)
{
    ClauseType *list;

    if ((list = (ClauseType *) malloc(sizeof(ClauseType))) == NULL) {
        return(NULL);
    }
    else if (pl_clause_build(list, COMPLEX, string_copy("."), 2, head, tail)) {
        return(list);
    }
    else {
        /* Error: Failed to build */
        return(NULL);
    }
}

/* Append the second list to the end of the first: ---------------------------*/

ClauseType *pl_list_append(ClauseType *current_list, ClauseType *tail)
{
    while (functor_comp(current_list, ".", 2)) {
        current_list = pl_arg_get(current_list, 2);
    }
    if (pl_clause_type(current_list) != EMPTY_LIST) {
        /* Error: Malformed argument */
        return(NULL);
    }
    else {
        pl_clause_type_set(current_list, pl_clause_type(tail));
        pl_functor_set(current_list, pl_functor(tail));
        pl_arity_set(current_list, pl_arity(tail));
        pl_integer_set(current_list, pl_integer(tail));
        pl_double_set(current_list, pl_double(tail));
        pl_arguments_set(current_list, pl_arguments(tail));
        free(tail);
        return(current_list);
    }
}

/* Append an element to the end of a list: -----------------------------------*/

int pl_list_add_to_tail(ClauseType *list, ClauseType *new_element)
{
    if (list == NULL) {
        return(FALSE);
    }
    else {
        ClauseList *args;
        ClauseType *new;

        while (pl_clause_type(list) != EMPTY_LIST) {
            list = pl_arg_get(list, 2);
        }
        if ((args = (ClauseList *) malloc(sizeof(ClauseList))) == NULL) {
            return(FALSE);     // error: failed malloc
        }
        else if ((args->tail = (ClauseList *) malloc(sizeof(ClauseList))) == NULL) {
            free(args);
            return(FALSE);     // error: failed malloc
        }
        else if ((new = (ClauseType *) malloc(sizeof(ClauseType))) == NULL) {
            free(args->tail);
            free(args);
            return(FALSE);     // error: failed malloc
        }
        else {
            pl_clause_build(new, EMPTY_LIST);
            pl_clause_type_set(list, COMPLEX);
            pl_functor_set(list, string_copy("."));
            pl_arity_set(list, 2);
            args->head = new_element;
            args->tail->head = new;
            args->tail->tail = NULL;
            pl_arguments_set(list, args);
            return(TRUE);
        }
    }
}

/* Delete the first element of a list: ---------------------------------------*/

int pl_list_delete_head(ClauseType *list)
{
    /* list points to a clause node representing a list, the head   */
    /* of which is the clause which is to be deleted.               */

    if (list == NULL) {
        return(FALSE);         // error: NULL argument
    }
    else if (pl_arity(list) != 2) {
        return(FALSE);         // error: invalid argument
    }
    else {
        ClauseList *args = pl_arguments(list);
        ClauseType *next;

        /* Free the clause we are deleting, as well as the argument nodes    */
        /* which are no longer needed and the functor space of the list node */

        next = args->tail->head;
        free(args->tail);
        pl_clause_free(args->head);
        free(args);
        pl_functor_free(list);

        /* Now copy the term pointed to by next to the original list node */

        pl_clause_type_set(list, pl_clause_type(next));
        pl_functor_set(list, pl_functor(next));
        pl_arity_set(list, pl_arity(next));
        pl_integer_set(list, pl_integer(next));
        pl_double_set(list, pl_double(next));
        pl_arguments_set(list, pl_arguments(next));
        free(next);

        /* and return success */

        return(TRUE);
    }
}

/* Write a clause representing a list to a file as a sequence of clauses: ----*/

int pl_list_write_to_file(ClauseType *list, char *filename, char *description)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        return(FALSE);
    }
    else {
        int error = 0;
        time_t when;

        time(&when);
        error = (fprintf(fp, "%% %s: %s\n", description, ctime(&when)) < (strlen(description) + 4)) || error;
        while (!error && functor_comp(list, ".", 2)) {
            error = (fprint_clause(fp, pl_arg_get(list, 1)) < 1) || error;
            error = (fprintf(fp, ".\n\n") < 3) || error;
            list = pl_arg_get(list, 2);
        }
        return((fclose(fp) == 0) && !error);
    }
}


/* Attempt to convert a clause to a character string. If the clause doesn't   */
/* correspond to a string, return NULL: --------------------------------------*/

static int pl_char_list_length(ClauseType *clause)
{
    ClauseType *tmp = clause;
    int l = 0;
    long i;

    while (functor_comp(tmp, ".", 2)) {
        if ((pl_is_integer(pl_arg_get(tmp, 1), &i)) && (isprint(i))) {
            l++;
        }
        else {
            return(-1);
        }
        tmp = pl_arg_get(tmp, 2);
    }
    if (pl_clause_type(tmp) != EMPTY_LIST) {
        return(-1);
    }
    else {
        return(l);
    }
}

char *pl_list_to_char_string(ClauseType *clause)
{
    int l;
    char *s;

    if ((l = pl_char_list_length(clause)) < 1) {
        /* It's the empty list, or not a character string... */
        return(NULL);
    }
    else if ((s = string_new(l + 1)) == NULL) {
        /* Allocation failed; Don't bother converting... */
        return(NULL);
    }
    else {
        ClauseType *tmp;
        l = 0;
        for (tmp = clause; pl_clause_type(tmp) != EMPTY_LIST; tmp = pl_arg_get(tmp, 2)) {
            s[l++] = pl_integer(pl_arg_get(tmp, 1));
        }
        s[l] = '\0';
        return(s);
    }
}

/******************************************************************************/
/* Argument manipulation functions: ------------------------------------------*/

ClauseType *pl_arg_get(ClauseType *clause, int n)
{
    if (clause == NULL) {
        return(NULL);          // error: NULL argument
    }
    else if (n < 1) {
        return(NULL);          // error: Bad argument
    }
    else {
        ClauseList *arguments = pl_arguments(clause);

        while ((arguments != NULL) && (--n > 0)) {
            arguments = arguments->tail;
        }
        if (arguments != NULL) {
            return(arguments->head);
        }
        else {
            return(NULL);      // error: Cannot find argument
        }
    }
}

int pl_arg_set(ClauseType *clause, int n, ClauseType *arg)
{
    if (clause == NULL) {
        return(FALSE);         // error: NULL argument
    }
    else if (arg == NULL) {
        return(FALSE);         // error: NULL argument
    }
    else {
        ClauseList *arguments = pl_arguments(clause);
        while ((arguments != NULL) && (--n > 0)) {
            arguments = arguments->tail;
        }
        if (arguments != NULL) {
            pl_clause_free(arguments->head);
            arguments->head = arg;
            return(TRUE);
        }
        else {
            return(FALSE);     // error: Ran out of arguments
        }
    }
}

void pl_arg_set_to_int(ClauseType *clause, int arg, long val)
{
    ClauseType *tmp;

    if ((tmp = (ClauseType *) malloc(sizeof(ClauseType))) != NULL) {
        pl_clause_build(tmp, INT_NUMBER, val);
        pl_arg_set(clause, arg, tmp);
    }
}

void pl_arg_set_to_real(ClauseType *clause, int arg, double val)
{
    ClauseType *tmp;

    if ((tmp = (ClauseType *) malloc(sizeof(ClauseType))) != NULL) {
        pl_clause_build(tmp, REAL_NUMBER, val);
        pl_arg_set(clause, arg, tmp);
    }
}

void pl_arg_set_to_string(ClauseType *clause, int arg, char *string)
{
    ClauseType *tmp;

    if ((tmp = (ClauseType *) malloc(sizeof(ClauseType))) != NULL) {
        pl_clause_build(tmp, STRING, string_copy(string));
        pl_arg_set(clause, arg, tmp);
    }
}

/******************************************************************************/

int pl_is_list(ClauseType *clause)
{
    return((pl_clause_type(clause) == EMPTY_LIST) || functor_comp(clause, ".", 2));
}

int pl_is_real(ClauseType *clause, double *r)
{
    if ((clause != NULL) && (pl_clause_type(clause) == REAL_NUMBER)) {
        *r = pl_double(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

int pl_is_integer(ClauseType *clause, long *i)
{
    if ((clause != NULL) && (pl_clause_type(clause) == INT_NUMBER)) {
        *i = pl_integer(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

int pl_is_number(ClauseType *clause, double *r)
{
    if (clause == NULL) {
        return(FALSE);
    }
    else if (pl_clause_type(clause) == REAL_NUMBER) {
        *r = pl_double(clause);
        return(TRUE);
    }
    else if (pl_clause_type(clause) == INT_NUMBER) {
        *r = (double)pl_integer(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

int pl_is_atom(ClauseType *clause, char **s)
{
    if ((clause != NULL) && (pl_clause_type(clause) == COMPLEX) && (pl_arity(clause) == 0)) {
        *s = pl_functor(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

int pl_is_variable(ClauseType *clause, char **s)
{
    if ((clause != NULL) && (pl_clause_type(clause) == VAR)) {
        *s = pl_functor(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

int pl_is_string(ClauseType *clause, char **s)
{
    if ((clause != NULL) && (pl_clause_type(clause) == STRING)) {
        *s = pl_functor(clause);
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

int pl_functor_replace(ClauseType *clause, const char *new_functor)
{
    char *copy_of_new_functor = string_copy(new_functor);

    if (clause == NULL) {
        /* Error: NULL argument: */
        return(FALSE);
    }
    else if (copy_of_new_functor == NULL) {
        /* Error: failed malloc: */
        return(FALSE);
    }
    else {
        if (pl_functor(clause) != NULL) {
            pl_functor_free(clause);
        }
        pl_functor_set(clause, copy_of_new_functor);
        return(TRUE);
    }
}

/*----------------------------------------------------------------------------*/

int pl_arity_adjust(ClauseType *clause, int arity)
{
    ClauseList **tmp = &(pl_arguments(clause));

    pl_arity_set(clause, arity);

/* Run through the current argument list, decrementing the required arity     */

    while ((arity > 0) && ((*tmp) != NULL)) {
        tmp = &((*tmp)->tail);
        arity--;
    }

/* If arity is still greater than 0, then we need to add arguments            */

    while (arity-- > 0) {
        ClauseList *new = (ClauseList *) malloc(sizeof(ClauseList));
        if (new == NULL) {
            return(FALSE);     // error: failed malloc
        }
        else {
            new->head = pl_clause_make_from_string("_.");
            new->tail = NULL;
            *tmp = new;
            tmp = &(new->tail);
        }
    }

/* If *tmp != NULL then we need to remove the remaining arguments             */

    while ((*tmp) != NULL) {
        ClauseList *current = *tmp;
        *tmp = (*tmp)->tail;
        pl_clause_free(current->head);
        free(current);
    }
    return(TRUE);
}

/******************************************************************************/

#if FALSE

// These don't appear to be used anymore...

static long *evaluate_integer_expression(ClauseType *clause)
{
    if (clause == NULL) {
        return(NULL);
    }
    else if (pl_clause_type(clause) == INT_NUMBER) {
        return(&(pl_integer(clause)));
    }
    else if (functor_comp(clause, "+", 1)) {
        return(evaluate_integer_expression(pl_arg_get(clause, 1)));
    }
    else if (functor_comp(clause, "-", 1)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        if (a != NULL) {
            *a = -1 * *a;
        }
        return(a);
    }
    else if (functor_comp(clause, "+", 2)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        long *b = evaluate_integer_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a + *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "-", 2)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        long *b = evaluate_integer_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a - *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "*", 2)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        long *b = evaluate_integer_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a * *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "//", 2)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        long *b = evaluate_integer_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a / *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "mod", 2)) {
        long *a = evaluate_integer_expression(pl_arg_get(clause, 1));
        long *b = evaluate_integer_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a % *b;
        }
        return(a);
    }
    else {
        return(NULL);
    }
}

/*----------------------------------------------------------------------------*/

static double *evaluate_real_expression(ClauseType *clause)
{
    if (clause == NULL) {
        return(NULL);
    }
    else if (pl_clause_type(clause) == INT_NUMBER) {
        pl_double_set(clause, (double)pl_integer(clause));
        return(&(pl_double(clause)));
    }
    else if (pl_clause_type(clause) == REAL_NUMBER) {
        return(&(pl_double(clause)));
    }
    else if (functor_comp(clause, "+", 1)) {
        return(evaluate_real_expression(pl_arg_get(clause, 1)));
    }
    else if (functor_comp(clause, "-", 1)) {
        double *a = evaluate_real_expression(pl_arg_get(clause, 1));
        if (a != NULL) {
            *a = -1 * *a;
        }
        return(a);
    }
    else if (functor_comp(clause, "+", 2)) {
        double *a = evaluate_real_expression(pl_arg_get(clause, 1));
        double *b = evaluate_real_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a + *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "-", 2)) {
        double *a = evaluate_real_expression(pl_arg_get(clause, 1));
        double *b = evaluate_real_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a - *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "*", 2)) {
        double *a = evaluate_real_expression(pl_arg_get(clause, 1));
        double *b = evaluate_real_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a * *b;
        }
        return(a);
    }
    else if (functor_comp(clause, "/", 2)) {
        double *a = evaluate_real_expression(pl_arg_get(clause, 1));
        double *b = evaluate_real_expression(pl_arg_get(clause, 2));
        if ((a != NULL) && (b != NULL)) {
            *a = *a / *b;
        }
        return(a);
    }
    else {
        return(NULL);
    }
}

#endif

/******************************************************************************/

int functor_comp(ClauseType *clause, const char *name, int arity)
{
    if (clause == NULL) {
        return(FALSE);
    }
    else if ((pl_clause_type(clause) == COMPLEX) && (pl_arity(clause) == arity)) {
        return(strcmp(pl_functor(clause), name) == 0);
    }
    else {
        return(FALSE);
    }
}

/*----------------------------------------------------------------------------*/
/* Return true if the functor of the given clause is a qualifier: *************/

int pl_functor_is_qualifier(ClauseType *condition)
{
    if (pl_arity(condition) != 1) {
        return(FALSE);
    }
    else if (strcmp(pl_functor(condition), "not") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(condition), "exists") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(condition), "once") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(condition), "unique") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(condition), "trace") == 0) {
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

/*----------------------------------------------------------------------------*/
/* Return true if the functor of the given clause is an infix operator: *******/

int pl_functor_is_term_comparison(ClauseType *clause)
{
    if (pl_arity(clause) != 2) {
        return(FALSE);
    }
    else if (strcmp(pl_functor(clause), "=") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "==") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "\\==") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "@<") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "@>") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "@=<") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "@>=") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "<") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), ">") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "=<") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), ">=") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "=:=") == 0) {
        return(TRUE);
    }
    else if (strcmp(pl_functor(clause), "=\\=") == 0) {
        return(TRUE);
    }
    else {
        return(FALSE);
    }
}

/******************************************************************************/

ClauseType *pl_coordinates_make(short x, short y)
{
    char buffer[16];
    g_snprintf(buffer, 16, "(%d,%d).", x, y);
    return(pl_clause_make_from_string(buffer));
}

/******************************************************************************/
/**************************  E N D   O F   F I L E  ***************************/
/******************************************************************************/
