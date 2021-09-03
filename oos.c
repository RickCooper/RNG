
// OOS Interpreter
// Version 1.2.6
// R. Cooper (c) 29/06/15

// Changes (1.0.1):
//   Add protection against possible NULL in oos_dump/2
//   Add oos_buffer_get_contents/2
// Changes (1.0.2):
//   Buffer contents now uses a TimestampedClauseList, allowing buffer decay to be properly implemented
//   Replaced GlobalVars with OosVars
//   Added oos_count_buffer_elements/2
//   Definition of the MessageList struct made private to oos.c (no longer in oos.h)
// Changes (1.1.0):
//   Added support for diagrams (arrows and coordinates for boxes)
//   New functions: oos_arrows_free/1, oos_arrow_create/6
// Changes (1.2.0):
//   Introduce activation values for buffer elements, hence new EXCITE/INHIBIT messages 
//   MessageList definition moved back into oos.h (hence now global, but why?)
//   New functions: oos_match_above_threshold/4
//   Implement correct handling of excess buffer elements when using BUFFER_EXCESS_RANDOM
// Changes (1.2.1):
//   Add support for diagram annotations:
//   New functions: oos_annotations_free/1, oos_annotation_create/7
//   For oos_box_name/2 return the model name if the box can't be found
//   Modify buffer element EXCITE/INHIBIT response (but still not right) 
//   Add defaults for trials_per_subject and subjects_per_experiment in oos_initialise_session/1
// Changes (1.2.2):
//   Bug fix: Call oos_initialise_session/1 in oos_globals_create/0
// Changes (1.2.3):
//   Move random_initialise/0, random_integer/2, random_uniform/2 to lib_maths.c
//   Create oos_model_free/1 from oos_*_free/1
//   Box fix: Call pl_operator_table_destroy/0 in oos_globals_destroy/1
// Changes (1.2.4):
//   Pass trials_per_subject and subjects_per_experiment into oos_initialise_session/3
// Changes (1.2.5):
//   Efficiency tweak: Set trials_per_subject and subjects_per_experiment directly in oos_globals_create/0
// Changes (1.2.6):
//   Set VertexStyle and LineStyle in oos_arrow_create

/******************************************************************************/

#include <stdio.h>
#include <glib.h>
#include <math.h>
#include "oos.h"
#include "lib_math.h"
#include "lib_string.h"

char *oos_class_name[BOX_TYPE_MAX] = {
    "Process", "Buffer"
};

char *oos_message_type_name[MT_MAX] = {
    "Send", "Add", "Delete", "Excite", "Inhibit", "Clear", "Stop"
};

/******************************************************************************/

static void timestamped_clause_list_free(TimestampedClauseList *list)
{
    while (list != NULL) {
        TimestampedClauseList *tail = list->tail;
        pl_clause_free(list->head);
        free(list);
        list = tail;
    }
}

static int timestamped_clause_list_length(TimestampedClauseList *list)
{
    int n = 0;
    while (list != NULL) {
        list = list->tail;
        n++;
    }
    return(n);
}

static TimestampedClauseList *timestamped_clause_list_copy(TimestampedClauseList *clauses)
{
    TimestampedClauseList dummy, *tmp, *current = &dummy;

    for (tmp = clauses; tmp != NULL; tmp = tmp->tail) {
        if ((current->tail = (TimestampedClauseList *)malloc(sizeof(TimestampedClauseList))) == NULL) {
            return(NULL);      // failed malloc
        }
        else {
            current = current->tail;
            current->head = pl_clause_copy(tmp->head);
            current->activation = tmp->activation;
            current->timestamp = tmp->timestamp;
        }
    }
    current->tail = NULL;
    return(dummy.tail);
}

static TimestampedClauseList *timestamped_clause_list_reverse(TimestampedClauseList *list)
{
    TimestampedClauseList *prev = list;

    if (prev != NULL) {
        TimestampedClauseList *next = prev->tail;
        prev->tail = NULL;
        while (next != NULL) {
            TimestampedClauseList *tmp = next->tail;
            next->tail = prev;
            prev = next;
            next = tmp;
        }
    }
    return(prev);
}

static void timetamped_clause_list_swap_elements(TimestampedClauseList *list, int i, int r)
{
    if (i != r) {
        TimestampedClauseList *il = NULL;
        TimestampedClauseList *rl = NULL;
        TimestampedClauseList *this;

        for (this = list; this != NULL; this = this->tail) {
            if (i == 0) {
                il = this;
            }
            if (r == 0) {
                rl = this;
            }
            i--;
            r--;
        }

        if ((il == NULL) || (rl == NULL)) {
            fprintf(stdout, "WARNING: Failed to find element(s) in %s\n", __FUNCTION__);
        }
        else {
            TimestampedClauseList tmp;

            tmp.head = il->head;
            tmp.timestamp = il->timestamp;
            tmp.activation = il->activation;

            il->head = rl->head;
	    rl->head = tmp.head;

            il->timestamp = rl->timestamp;
	    rl->timestamp = tmp.timestamp;

            il->activation = rl->activation;
            rl->activation = tmp.activation;
        }
    }
}

static TimestampedClauseList *timestamped_clause_list_permute(TimestampedClauseList *list)
{
    int i, r, l;

    l = timestamped_clause_list_length(list);

    for (i = 0; i < l; i++) {
        r = random_integer(i, l);
        timetamped_clause_list_swap_elements(list, i, r);
    }
    return(list);
}

static TimestampedClauseList *timestamped_clause_list_delete_nth(TimestampedClauseList *list, int n)
{

    /* Delete the nth element, counting from zero */

    if (list == NULL) {
        return(NULL);
    }
    else if (n == 0) {
        TimestampedClauseList *result;

        result = list->tail;
        pl_clause_free(list->head);
        free(list);
        return(result);
    }
    else {
        TimestampedClauseList *prev = NULL, *this = list;
        while ((this != NULL) && (n-- > 0)) {
            prev = this;
            this = this->tail;
        }
        prev->tail = this->tail;
        pl_clause_free(this->head);
        free(this);
        return(list);
    }
}

static TimestampedClauseList *timestamped_clause_list_prepend_element(TimestampedClauseList *list, ClauseType *element, long now, double activation)
{
    TimestampedClauseList *new = (TimestampedClauseList *) malloc(sizeof(TimestampedClauseList));

    if (new == NULL) {
        return(list);
    }
    else {
        new->head = element;
        new->timestamp = now;
        new->activation = activation;
        new->tail = list;
        return(new);
    }
}

static TimestampedClauseList *timestamped_clause_list_add_element_to_tail(TimestampedClauseList *list, ClauseType *element, long now, double activation)
{
    TimestampedClauseList *node = (TimestampedClauseList *) malloc(sizeof(TimestampedClauseList));

    if (node == NULL) {
        fprintf(stdout, "WARNING: Memory allocation failed when appending element to timestamped list\n");
        return(list);
    }
    else if (element != NULL) {
        node->head = element;
        node->timestamp = now;
        node->activation = activation;
        node->tail = NULL;
        if (list == NULL) {
            list = node;
        }
        else {
            TimestampedClauseList *tmp = list;
            while (tmp->tail != NULL) {
                tmp = tmp->tail;
            }
            tmp->tail = node;
        }
    }
    return(list);
}

/******************************************************************************/

static BoxList *oos_locate_box_ptr(OosVars *gv, int id)
{
    BoxList *tmp;

    for (tmp = gv->components; tmp != NULL; tmp = tmp->next) {
        if (tmp->id == id) {
            return(tmp);
        }
    }
    return(NULL);
}

char *oos_box_name(OosVars *gv, int id)
{
    BoxList *this = oos_locate_box_ptr(gv, id);

    if (this == NULL) {
        return(gv->name);
    }
    else {
        return(this->name);
    }
}

/******************************************************************************/
/* Initialisation functions: **************************************************/

void oos_messages_free(OosVars *gv)
{
    while (gv->messages != NULL) {
        MessageList *tmp = gv->messages->next;
        pl_clause_free(gv->messages->content);
        g_free(gv->messages);
        gv->messages = tmp;
    }
}

void oos_annotations_free(OosVars *gv)
{
    while (gv->annotations != NULL) {
        AnnotationList *tmp = gv->annotations->next;
        g_free(gv->annotations->text);
        g_free(gv->annotations);
        gv->annotations = tmp;
    }
}

void oos_arrows_free(OosVars *gv)
{
    while (gv->arrows != NULL) {
        ArrowList *tmp = gv->arrows->next;
        g_free(gv->arrows->coordinates);
        g_free(gv->arrows);
        gv->arrows = tmp;
    }
}

void oos_components_free(OosVars *gv)
{
    while (gv->components != NULL) {
        BoxList *tmp = gv->components->next;
        timestamped_clause_list_free(gv->components->content);
        g_free(gv->components->name);
        g_free(gv->components);
        gv->components = tmp;
    }
}

void oos_model_free(OosVars *gv)
{
    oos_components_free(gv);
    oos_annotations_free(gv);
    oos_arrows_free(gv);
}

/*----------------------------------------------------------------------------*/

AnnotationList *oos_annotation_create(OosVars *gv, char *text, double x, double y, int fontsize, double theta, Boolean italic)
{
    // Return pointer to new annotation, or NULL if it isn't created

    AnnotationList *new;

    if ((new = (AnnotationList *)(malloc(sizeof(AnnotationList)))) != NULL) {
        new->next = gv->annotations;
        new->text = string_copy(text);
        new->x = x;
        new->y = y;
        new->fontsize = fontsize;
        new->theta = theta;
        new->italic = italic;
        gv->annotations = new;
    }
    return(new);
}


ArrowList *oos_arrow_create(OosVars *gv, VertexStyle vs, LineStyle ls, ArrowHeadType head, CairoxPoint *coordinates, int points, double width)
{
    // Return pointer to new arrow, or NULL if it isn't created

    ArrowList *new;

    if ((new = (ArrowList *)(malloc(sizeof(ArrowList)))) != NULL) {
        new->next = gv->arrows;
        new->vertex_style = vs;
        new->head = head;
        new->coordinates = coordinates;
        new->points = points;
        new->width = width;
        new->line_style = ls;
        gv->arrows = new;
    }
    return(new);
}

BoxList *oos_process_create(OosVars *gv, char *name, int id, double x, double y, void (*output_function)(OosVars *))
{
    // Return pointer to new box, or NULL if it isn't created

    BoxList *new;

    if ((new = (BoxList *)(malloc(sizeof(BoxList)))) != NULL) {
        new->next = gv->components;
        new->name = string_copy(name);;
        new->id = id;
        new->x = x;
        new->y = y;
        new->bt = BOX_PROCESS;
        new->stopped = FALSE;
        new->output_function = output_function;
        new->content = NULL;
        gv->components = new;
    }
    return(new);
}

BoxList *oos_buffer_create(OosVars *gv, char *name, int id, double x, double y, BufferDecayProp decay, int decay_constant, BufferCapacityProp capacity, int capacity_constant, BufferExcessProp excess_capacity, BufferAccessProp access)
{
    // Return pointer to new box, or NULL if it isn't created

    BoxList *new;

    if ((new = (BoxList *)(malloc(sizeof(BoxList)))) != NULL) {
        new->next = gv->components;
        new->name = string_copy(name);;
        new->id = id;
        new->x = x;
        new->y = y;
        new->bt = BOX_BUFFER;
        new->stopped = FALSE;
        new->output_function = NULL;
        new->decay = decay;
        new->decay_constant = decay_constant;
        new->capacity = capacity;
        new->capacity_constant = capacity_constant;
        new->excess_capacity = excess_capacity;
        new->access = access;
        new->content = NULL;
        gv->components = new;
    }
    return(new);
}

void oos_buffer_create_element(OosVars *gv, int box_id, char *element, double activation)
{
    BoxList *this = oos_locate_box_ptr(gv, box_id);

    if (this == NULL) {
        fprintf(stdout, "WARNING: Cannot locate buffer %d in oos_buffer_create_element\n", box_id);
    }
    else {
        this->content = timestamped_clause_list_prepend_element(this->content,  pl_clause_make_from_string(element), gv->cycle, activation);
    }
}

/*----------------------------------------------------------------------------*/

OosVars *oos_globals_create()
{
    OosVars *gv;

    if ((gv = (OosVars *)malloc(sizeof(OosVars))) != NULL) {
        gv->cycle = 0;
        gv->block = 0;
        gv->name = NULL;
        gv->stopped = FALSE;
        gv->task_data = NULL;
        gv->components = NULL;
        gv->annotations = NULL;
        gv->arrows = NULL;
        gv->messages = NULL;
        gv->trials_per_subject = 1;
        gv->subjects_per_experiment = 1;
    }
    random_initialise();
    return(gv);
}

void oos_globals_destroy(OosVars *gv)
{
    if (gv != NULL) {
        g_free(gv->name);
        oos_model_free(gv);
        oos_messages_free(gv);
        free(gv);
    }
    pl_operator_table_destroy();
}

/*----------------------------------------------------------------------------*/

ClauseType *unify_terms(ClauseType *template, ClauseType *term)
{
    ClauseType *result = NULL;

    /* This implementation of unification is incomplete - it doesn't deal */
    /* correctly with variables that occur more than once in either term. */

    if ((pl_clause_type(term) == VAR) && (pl_clause_type(template) != NULL_TERM)) {
        /* We're matching against a variable: return TRUE unless the thing  */
        /* where matching is a NULL_TERM (which shouldn't happen)           */
        result = pl_clause_copy(template);
    }
    else {
        switch (pl_clause_type(template)) {
            case NULL_TERM: {
                break;
            }
            case EMPTY_LIST: {
                if (pl_clause_type(term) == EMPTY_LIST) {
                    result = pl_clause_copy(term);
                }
                break;
            }
            case INT_NUMBER: {
                if ((pl_clause_type(term) == INT_NUMBER) && (pl_integer(template) == pl_integer(term))) {
                    result = pl_clause_copy(term);
                }
                break;
            }
            case REAL_NUMBER: {
                if ((pl_clause_type(term) == REAL_NUMBER) && (pl_double(template) == pl_double(term))) {
                    result = pl_clause_copy(term);
                }
                break;
            }
            case STRING: {
                if ((pl_clause_type(term) == STRING) && (strcmp(pl_functor(template), pl_functor(term)) == 0)) {
                    result = pl_clause_copy(term);
                }
                break;
            }            
            case VAR: {
                result = pl_clause_copy(term);
                break;
            }
            case COMPLEX: {
                if (pl_arity(template) != pl_arity(term)) {
                    result = NULL;
                }
                else if (strcmp(pl_functor(template), pl_functor(term)) != 0) {
                    result = NULL;
                }
                else {
                    int i = pl_arity(term);
                    ClauseType *tmp;

                    result = pl_clause_copy(term);

                    /* Attempt to unify every argument... */
                    while ((i > 0) && ((tmp = unify_terms(pl_arg_get(template, i), pl_arg_get(term, i))) != NULL)) {
                        pl_arg_set(result, i, tmp);
                        i--;
                    }

                    /* And if we fail discard "result": */
                    if (i != 0) {
                        pl_clause_free(result);
                        result = NULL;
                    }
                }
                break;
            }
        }
    }
    return(result);
}

static Boolean terms_unify(ClauseType *term1, ClauseType *term2)
{
    // Check that terms unify, but don't actually unify them!

    ClauseType *result;

    if ((result = unify_terms(term1, term2)) == NULL) {
        return(FALSE);
    }
    else {
        pl_clause_free(result);
        return(TRUE);
    }
}

void oos_dump(OosVars *gv, Boolean state)
{
    // Debugging predicate: dump the model (and its state?)

    BoxList *tmp;

    fprintf(stdout, "%s: BLOCK %d; CYCLE %d\n", gv->name, gv->block, gv->cycle);
    if ((state) && (gv->messages)) {
        MessageList *ml;
        for (ml = gv->messages; ml != NULL; ml = ml->next) {
            fprintf(stdout, "  MESSAGE (");
            if ((tmp = oos_locate_box_ptr(gv, ml->source)) != NULL) {
                fprintf(stdout, "%s --> ", tmp->name ? tmp->name : "Unnamed");
            }
            else {
                fprintf(stdout, "Unknown --> ");
            }
            if ((tmp = oos_locate_box_ptr(gv, ml->target)) != NULL) {
                fprintf(stdout, "%s; %s): ", tmp->name ? tmp->name : "Unnamed", oos_message_type_name[ml->mt]);
            }
            else {
                fprintf(stdout, "%s; %s): ", "Unknown", oos_message_type_name[ml->mt]);
            }
            if (ml->content != NULL) {
                fprint_clause(stdout, ml->content);
            }
            if (ml->next != NULL) {
                fprintf(stdout, ", ");
            }
            fprintf(stdout, "\n");
        }
    }
    for (tmp = gv->components; tmp != NULL; tmp = tmp->next) {
        fprintf(stdout, "  COMPONENT: ");
        fprintf(stdout, "%s ", tmp->name ? tmp->name : "Unnamed");
        fprintf(stdout, "(%s)", oos_class_name[(int) tmp->bt]);
        if ((state) && (tmp->content != NULL)) {
            TimestampedClauseList *cl;
            fprintf(stdout, ": [");
            for (cl = tmp->content; cl != NULL; cl = cl->tail) {
                fprint_clause(stdout, cl->head);
                fprintf(stdout, "(%ld, %4.2f)", cl->timestamp, cl->activation);
                if (cl->tail != NULL) {
                    fprintf(stdout, ", ");
                }
            }
            fprintf(stdout, "]");
	}
        fprintf(stdout, "\n");
    }
}

/******************************************************************************/
/* Processing functions: ******************************************************/

Boolean oos_match_above_threshold(OosVars *gv, int id, ClauseType *template, double threshold)
{
    TimestampedClauseList *cl, *buffer_contents = NULL;
    ClauseType *result;
    BoxList *this = NULL;
    Boolean match = FALSE;

    /* Locate the buffer (for its content and access property): */
    this = oos_locate_box_ptr(gv, id);

    /* Get the buffer's content, suitably reordered: */
    if (this != NULL) {
        if (this->access == BUFFER_ACCESS_LIFO) {
            buffer_contents = timestamped_clause_list_copy(this->content);
        }
        else if (this->access == BUFFER_ACCESS_FIFO) {
            buffer_contents = timestamped_clause_list_reverse(timestamped_clause_list_copy(this->content));
        }
        else { // BUFFER_ACCESS_RANDOM
            buffer_contents = timestamped_clause_list_permute(timestamped_clause_list_copy(this->content));
        }
    }

    /* Attempt the match, unifying if it succeeds: */
    cl = buffer_contents;
    while ((cl != NULL) && (!match)) {
        if ((cl->activation >= threshold) && ((result = unify_terms(template, cl->head)) != NULL)) {
            pl_clause_swap(template, result);
            pl_clause_free(result);
            match = TRUE;
        }
        cl = cl->tail;
    }

    /* Free temporary copy and return the result: */
    timestamped_clause_list_free(buffer_contents);
    return(match);
}

Boolean oos_match(OosVars *gv, int id, ClauseType *template)
{
    return(oos_match_above_threshold(gv, id, template, 0.0));
}

void oos_message_create(OosVars *gv, MessageType mt, int source, int target, ClauseType *content)
{
    MessageList *new;

    if ((new = (MessageList *)malloc(sizeof(MessageList))) != NULL) {
        new->source = source;
        new->target = target;
        new->mt = mt;
        new->content = content;
        new->next = gv->messages;
        gv->messages = new;
    }
}

static void generate_messages(OosVars *gv)
{
    BoxList *tmp;

    for (tmp = gv->components; tmp != NULL; tmp = tmp->next) {
        if (!tmp->stopped && (tmp->bt == BOX_PROCESS) && (tmp->output_function != NULL)) {
            tmp->output_function(gv);
        }
    }
}

/*----------------------------------------------------------------------------*/

static void oos_buffer_apply_clear_messages(OosVars *gv, BoxList *this)
{
    MessageList *tmp;

    for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
        if ((tmp->target == this->id) && (tmp->mt == MT_CLEAR)) {
            timestamped_clause_list_free(this->content);
            this->content = NULL;
        }
    }
}

static void oos_buffer_apply_delete_messages(OosVars *gv, BoxList *this)
{
    TimestampedClauseList *cl;
    MessageList *tmp;
    ClauseType *match;

    for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
        if ((tmp->target == this->id) && (tmp->mt == MT_DELETE)) {
            if (this->content != NULL) {
                if ((match = unify_terms(this->content->head, tmp->content)) != NULL) {
                    TimestampedClauseList *tmp = this->content->tail;
                    pl_clause_free(this->content->head);
                    free(this->content);
                    this->content = tmp;
                    pl_clause_free(match);
                }
                else {
                    for (cl = this->content; cl->tail != NULL; cl = cl->tail) {
                        if ((match = unify_terms(cl->tail->head, tmp->content)) != NULL) {
                            TimestampedClauseList *rest = cl->tail->tail;
                            pl_clause_free(cl->tail->head);
                            free(cl->tail);
                            cl->tail = rest;
                            pl_clause_free(match);
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void oos_buffer_apply_add_messages(OosVars *gv, BoxList *this)
{
    MessageList *tmp;

    for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
        if ((tmp->target == this->id) && (tmp->mt == MT_ADD)) {
            if ((this->capacity == BUFFER_CAPACITY_LIMITED) && !(timestamped_clause_list_length(this->content) < this->capacity_constant)) {
                if (this->excess_capacity == BUFFER_EXCESS_RANDOM) {
                    int r = random_integer(0, timestamped_clause_list_length(this->content));
                    this->content = timestamped_clause_list_delete_nth(this->content, r);
                    // Now prepend the new element:
                    this->content = timestamped_clause_list_prepend_element(this->content, pl_clause_copy(tmp->content), gv->cycle, 1.0);
                }
                else if (this->excess_capacity == BUFFER_EXCESS_OLDEST) {
                    // Delete oldest (last) element
                    if ((this->content != NULL) && (this->content->tail == NULL)) {
                        // There's just one element - delete it:
                        pl_clause_free(this->content->head);
                        free(this->content);
                        this->content = NULL;
                    }
                    else if (this->content != NULL) {
                        // There are several elements - go to the second last:
                        TimestampedClauseList *tmp = this->content;
                        while (tmp->tail->tail != NULL) {
                            tmp = tmp->tail;
                        }
                        pl_clause_free(tmp->tail->head);
                        free(tmp->tail);
                        tmp->tail = NULL;
                    }
                    // Now prepend the new element:
                    this->content = timestamped_clause_list_prepend_element(this->content, pl_clause_copy(tmp->content), gv->cycle, 1.0);
                }
                else if (this->excess_capacity == BUFFER_EXCESS_YOUNGEST) {
                    // Delete the youngest (first) element:
                    if (this->content != NULL) {
                        TimestampedClauseList *tmp = this->content;
                        this->content = this->content->tail;
                        pl_clause_free(tmp->head);
                        free(tmp);
                    }
                    // Now prepend the new element:
                    this->content = timestamped_clause_list_prepend_element(this->content, pl_clause_copy(tmp->content), gv->cycle, 1.0);
                }
            }
            else {
// fprintf(stdout, "%4d: Adding ", gv->cycle); fprint_clause(stdout, tmp->content); fprintf(stdout, " to %s\n", this->name);
                // No need to worry about capacity - just prepend the new element:
                this->content = timestamped_clause_list_prepend_element(this->content, pl_clause_copy(tmp->content), gv->cycle, 1.0);
            }
        }
    }
}

static Boolean message_excites_target(MessageList *message, long target_id, ClauseType *target_content)
{
    if (message->mt != MT_EXCITE) {
        return(FALSE);
    }
    else if (message->target != target_id) {
        return(FALSE);
    }
    else {
        return(terms_unify(message->content, target_content));
    }
}

static Boolean message_inhibits_target(MessageList *message, long target_id, ClauseType *target_content)
{
    if (message->mt != MT_INHIBIT) {
        return(FALSE);
    }
    else if (message->target != target_id) {
        return(FALSE);
    }
    else {
        return(terms_unify(message->content, target_content));
    }
}

static void oos_buffer_apply_activate_messages(OosVars *gv, BoxList *this)
{
    if ((this != NULL) && (this->bt == BOX_BUFFER)) {
        TimestampedClauseList *element;

        for (element = this->content; element != NULL; element = element->tail) {
            MessageList *tmp;
            int count = 0;

            for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
                if (message_inhibits_target(tmp, this->id, element->head)) {
                    count--;
                }
                else if (message_excites_target(tmp, this->id, element->head)) {
                    count++;
                }
            }

            if (count > 0) {
TODO(2, "Excite properly");
                element->activation = 1.0; // *= 2.1;
                element->timestamp = gv->cycle;
            }
            else if (count < 0) {
TODO(2, "Inhibit properly");
                element->activation = 0.1; // *= 0.1;
                element->timestamp = gv->cycle;
            }
        }
    }
}

static Boolean element_survives_decay(OosVars *gv, ClauseType *element, long timestamp, BufferDecayProp decay, int decay_constant)
{
    switch (decay) {
        case BUFFER_DECAY_EXPONENTIAL: {
            double threshold = 1.0 / exp(log(2.0)/(double)decay_constant);
            double p = random_uniform(0.0, 1.0);
            return(p < threshold);
            break;
        }
        case BUFFER_DECAY_WEIBULL: {
            double dt = (gv->cycle - timestamp);
            if (dt > 0) {
                double threshold = 1.0 / pow(exp(1.0/(double)decay_constant), log(dt+1));
                double p = random_uniform(0.0, 1.0);
                return(p < threshold);
            }
            else {
                return(TRUE);
            }
            break;
        }
        case BUFFER_DECAY_LINEAR: {
            double dt = (gv->cycle - timestamp);
            if (dt < decay_constant) {
                double threshold = 1.0 / (double) (decay_constant - dt);
                double p = random_uniform(0.0, 1.0);
                return(p > threshold);
            }
            else {
                return(FALSE);
            }
            break;
        }
        case BUFFER_DECAY_QUADRATIC: {
            double dt = (gv->cycle - timestamp);
            if (dt < decay_constant) {
                double k = 2.0;
                double threshold = pow(1.0 / (double) (decay_constant - dt), k);
                double p = random_uniform(0.0, 1.0);
                return(p > threshold);
            }
            else {
                return(FALSE);
            }
            break;
        }
        case BUFFER_DECAY_FIXED: {
            double dt = (gv->cycle - timestamp);
            return(dt < (decay_constant - 1));
            break;
        }
        case BUFFER_DECAY_NONE: {
            return(TRUE);
            break;
        }
    }
    fprintf(stdout, "WARNING: Buffer decay function not handled in switch statement\n");
    return(FALSE);
}

static void oos_buffer_apply_decay(OosVars *gv, BoxList *this, BufferDecayProp decay, int decay_constant)
{
    if ((this->content != NULL) && (decay != BUFFER_DECAY_NONE)) {
        TimestampedClauseList *before, *after, *tmp;

        before = this->content;
        after = NULL;
        
        while (before != NULL) {
            if (element_survives_decay(gv, before->head, before->timestamp, decay, decay_constant)) {
                after = timestamped_clause_list_add_element_to_tail(after, before->head, before->timestamp, before->activation);
            }
            else {
                pl_clause_free(before->head);
            }
            tmp = before->tail;
            free(before);
            before = tmp;
        }
        this->content = after;
    }
}

static void oos_component_process_stop_messages(OosVars *gv, BoxList *this)
{
    MessageList *tmp;

    for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
        if ((tmp->target == this->id) && (tmp->mt == MT_STOP)) {
            this->stopped = TRUE;
        }
    }
}

static void update_component_state(OosVars *gv, BoxList *this)
{
    oos_component_process_stop_messages(gv, this);
    if (!this->stopped && (this->bt == BOX_BUFFER)) {
        // Process all the messages bound for the buffer:
        oos_buffer_apply_clear_messages(gv, this);
        oos_buffer_apply_delete_messages(gv, this);
        oos_buffer_apply_add_messages(gv, this);
        oos_buffer_apply_activate_messages(gv, this);
        // Now apply decay: 
        oos_buffer_apply_decay(gv, this, this->decay, this->decay_constant);
    }
}

static void update_states(OosVars *gv)
{
    BoxList *tmp;

    for (tmp = gv->components; tmp != NULL; tmp = tmp->next) {
        update_component_state(gv, tmp);
    }
}

static void oos_model_process_stop_messages(OosVars *gv)
{
    MessageList *tmp;

    for (tmp = gv->messages; tmp != NULL; tmp = tmp->next) {
        if ((tmp->target == 0) && (tmp->mt == MT_STOP)) {
            gv->stopped = TRUE;
        }
    }
}

static void oos_component_initialise_state(OosVars *gv, BoxList *this)
{
    if (this->content != NULL) {
        timestamped_clause_list_free(this->content);
        this->content = NULL;
    }
}

static void oos_component_initialise_states(OosVars *gv)
{
    BoxList *tmp;

    for (tmp = gv->components; tmp != NULL; tmp = tmp->next) {
        oos_component_initialise_state(gv, tmp);
    }
}

TimestampedClauseList  *oos_buffer_get_contents(OosVars *gv, int id)
{
    BoxList *this = oos_locate_box_ptr(gv, id);

    return(this ? this->content : NULL);
}

/*----------------------------------------------------------------------------*/

Boolean oos_step(OosVars *gv)
{
    oos_messages_free(gv);
    gv->cycle++;
    generate_messages(gv);
    oos_model_process_stop_messages(gv);
    if (!gv->stopped) {
        update_states(gv);
    }
    return(!gv->stopped);
}

void oos_step_block(OosVars *gv)
{
    gv->block++;
    gv->stopped = FALSE;
    oos_component_initialise_states(gv);
}

void oos_initialise_trial(OosVars *gv)
{
    gv->cycle = 0;
    gv->stopped = FALSE;
}

void oos_initialise_session(OosVars *gv, int trials_per_subject, int subjects_per_experiment)
{
    gv->cycle = 0;
    gv->block = 0;
    gv->stopped = FALSE;

    gv->trials_per_subject = trials_per_subject;
    gv->subjects_per_experiment = subjects_per_experiment;
}

/******************************************************************************/
/* EXTRANEOUS FUNCTIONS (SHOULD BE DEFINED ELSEWHERE) *************************/

void gtkx_warn(int warn, char *buffer)
{
    fprintf(stdout, "WARNING %d: %s\n", warn, buffer);
}

void pl_error_syntax(char *error_message)
{
    fprintf(stdout, "SYNTAX ERROR: %s\n", error_message);
}

/******************************************************************************/

int oos_count_buffer_elements(OosVars *gv, int id)
{
    BoxList *this = oos_locate_box_ptr(gv, id);

    if (this != NULL) {
        return(timestamped_clause_list_length(this->content));
    }
    else {
        return(-1);
    }
}

/******************************************************************************/
