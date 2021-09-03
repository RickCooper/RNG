/*******************************************************************************

    File:       pl_operators.c
    Contents:   Routines for scanning/tokenising Prolog.
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
        void pl_operator_table_initialise()
        void pl_operator_table_destroy()
        int pl_operator_declare(ClauseType *dec)
        OperatorType *lookup_operator(Token *token, OpClass op_class)
        OperatorType *lookup_operator_in_term(Token *token, OpClass op_class)

*******************************************************************************/

/******** Include files: ******************************************************/

#include "pl.h"
#include <glib.h>

#define PATHNAME_LENGTH 256

#ifdef MALLOC_CHECK
#include "../lib/zmalloc.h"
#endif

/******** Local type defs: ****************************************************/

typedef struct operator_table {
    OperatorType           head;
    struct operator_table *tail;
} OperatorTable;

/******** Local variables: ****************************************************/

static OperatorTable *operator_list = NULL;

/******************************************************************************/
/******** Routines for Manipulating Operators: ********************************/
/******************************************************************************/

/*------- Classify a TypeOfOperator: -----------------------------------------*/

static OpClass operator_class(TypeOfOperator op_type)
{
    switch (op_type) {
        case FX: case FY: {
            return(PREFIX);
        }
        case XFX: case XFY: case YFX: {
            return(INFIX);
        }
        case XF: case YF: {
            return(POSTFIX);
        }
    }
    return(INFIX);
}

/*------- Convert a string into a TypeOfOperator: ----------------------------*/

static int pl_is_optype(ClauseType *string, TypeOfOperator *optype)
{
    char *atom;

    if (!pl_is_atom(string, &atom)) {
        return(0);
    }
    else if (strcmp(atom, "fx") == 0) {
        *optype = FX; return(1);
    }
    else if (strcmp(atom, "fy") == 0) {
        *optype = FY; return(1);
    }
    else if (strcmp(atom, "xf") == 0) {
        *optype = XF; return(1);
    }
    else if (strcmp(atom, "yf") == 0) {
        *optype = YF; return(1);
    }
    else if (strcmp(atom, "xfx") == 0) {
        *optype = XFX; return(1);
    }
    else if (strcmp(atom, "xfy") == 0) {
        *optype = XFY; return(1);
    }
    else if (strcmp(atom, "yfx") == 0) {
        *optype = YFX; return(1);
    }
    else {
        return(0);
    }
}

/*------- Add an operator to the Operator Table: -----------------------------*/

static int add_operator(int precedence, TypeOfOperator op_t, const char *op_n)
{
    OperatorTable *p;

    if ((p = (OperatorTable *)malloc(sizeof(OperatorTable))) == NULL) {
        return(FALSE); // error: failed_malloc
    }
    else if ((p->head.op_name = string_new(strlen(op_n)+1)) == NULL) {
        free(p);
        return(FALSE); // error: failed_malloc
    }
    else {
        p->head.op_precedence = precedence;
        p->head.op_type = op_t;
        strcpy(p->head.op_name, op_n);
        p->tail = operator_list;
        operator_list = p;
        return(TRUE);
    }
}

/*------- Declare an operator (and other things?): ---------------------------*/

int pl_operator_declare(ClauseType *dec)
{
    if (functor_comp(dec, ":-", 1)) {
        ClauseType *op = pl_arg_get(dec, 1);
        if (functor_comp(op, "op", 3)) {
            long precedence = 0;
            TypeOfOperator optype = FX;
            char *name = NULL;

            pl_is_integer(pl_arg_get(op, 1), &precedence);
            pl_is_optype(pl_arg_get(op, 2), &optype);
            pl_is_atom(pl_arg_get(op, 3), &name);

            return(add_operator((int) precedence, optype, name));
        }
        else {
            return(FALSE); // error: unrecognised declaration
        }
    }
    else {
        return(FALSE); // error: unrecognised declaration
    }
}

/*------- Destroy the Operator Table: ----------------------------------------*/

void pl_operator_table_destroy()
{
    while (operator_list != NULL) {
        OperatorTable *tmp = operator_list;

        operator_list = tmp->tail;
        free(tmp->head.op_name);
        free(tmp);
    }
}

/*------- Initialise the Operator Table: -------------------------------------*/

void pl_operator_table_initialise()
{
    pl_operator_table_destroy();

    /* Standard Prolog operators: */

    add_operator(1200, XFX, ":-");
    add_operator(1200, XFX, "-->");
    add_operator(1200, FX,  ":-");
    add_operator(1200, FX,  "?-");
    add_operator(1150, FX,  "mode");
    add_operator(1150, FX,  "public");
    add_operator(1150, FX,  "dynamic");
    add_operator(1150, FX,  "multifile");
    add_operator(1150, FX,  "module_transparent");
    add_operator(1150, FX,  "discontiguous");
    add_operator(1150, FX,  "volitile");
    add_operator(1150, FX,  "initialization");
    add_operator(1100, XFY, ";");
    add_operator(1050, XFY, "->");
    add_operator(1000, XFY, ",");
    add_operator(900,  FY,  "not");
    add_operator(900,  FY,  "\\+");
    add_operator(900,  FY,  "spy");
    add_operator(900,  FY,  "nospy");
    add_operator(700,  XFX, "=");
    add_operator(700,  XFX, "is");
    add_operator(700,  XFX, "=..");
    add_operator(700,  XFX, "==");
    add_operator(700,  XFX, "\\==");
    add_operator(700,  XFX, "@<");
    add_operator(700,  XFX, "@>");
    add_operator(700,  XFX, "@=<");
    add_operator(700,  XFX, "@>=");
    add_operator(700,  XFX, "=:=");
    add_operator(700,  XFX, "=\\=");
    add_operator(700,  XFX, "\\=");
    add_operator(700,  XFX, "\\=@=");
    add_operator(700,  XFX, "<");
    add_operator(700,  XFX, ">");
    add_operator(700,  XFX, "=<");
    add_operator(700,  XFX, ">=");
    add_operator(500,  FX,  "+");
    add_operator(500,  FX,  "-");
    add_operator(500,  YFX, "+");
    add_operator(500,  YFX, "-");
    add_operator(500,  YFX, "/\\");
    add_operator(500,  YFX, "\\/");
    add_operator(500,  YFX, "xor");
    add_operator(400,  YFX, "*");
    add_operator(400,  YFX, "/");
    add_operator(400,  YFX, "//");
    add_operator(400,  YFX, "<<");
    add_operator(400,  YFX, ">>");
    add_operator(400,  YFX, "mod");
    add_operator(400,  YFX, "rem");
    add_operator(200,  XFX, "**");
    add_operator(200,  XFY, "^");
}

/*------- Look for a token in the Operator Table -----------------------------*/

/*
 * Is the given token an operator of the specified type (PREFIX/INFIX/POSTFIX)?
 * If so, return a pointer to the operator in the operator table.
 */

OperatorType *lookup_operator(Token *token, OpClass op_class)
{
    if (token->token_type == PL_SYM) {
        OperatorTable *i = operator_list;

        while (i != NULL) {
            if ((operator_class(i->head.op_type) == op_class) &&
                (strcmp(token->token_name, i->head.op_name) == 0)) {
                    break;
            }
            i = i->tail;
        }
        if (i == NULL) {
            return(NULL);
        }
        else {
            return(&(i->head));
        }
    }
    else {
        return(NULL);
    }
}

/*
 * An (prefix) operator might also occur in the middle of a term disguised as a
 * functor. Handle this case by testing if the equivalent symbol is in fact an
 * operator.
 */

OperatorType *lookup_operator_in_term(Token *token, OpClass op_class)
{
    if (token->token_type == PL_FUNCTOR) {
        OperatorType *result;
        token->token_type = PL_SYM;
        if ((result = lookup_operator(token, op_class)) != NULL) {
            return(result);
        }
        else {
            token->token_type = PL_FUNCTOR;
            return(NULL);
        }
    }
    else {
        return(lookup_operator(token, op_class));
    }
}

/******************************************************************************/
/* Declare all operators in the declarations list and return a counter. When  */
/* used in conjunction with pl_operator_declarations_pop, this allows         */
/* temporary declaration of operators. The push function returns the number   */
/* of declarations pushed, while the pop function pops a given number, hence  */
/* returning the operator declarations to their previous state.               */

int pl_operator_declarations_push(ClauseList *declarations)
{
    int count = 0;

    while (declarations != NULL) {
        if (pl_operator_declare(declarations->head)) {
            count++;
        }
        declarations = declarations->tail;
    }
    return(count);
}

void pl_operator_declarations_pop(int count)
{
    while ((count > 0) && (operator_list != NULL)) {
        OperatorTable *p = operator_list->tail;
        free(operator_list->head.op_name);
        free(operator_list);
        operator_list = p;
        count--;
    }
}

/******************************************************************************/
/**************************  E N D   O F   F I L E  ***************************/
/******************************************************************************/
