
#include "rng.h"
#include "lib_math.h"

#define BOX_STRATEGY             11
#define BOX_MONITORING           12
#define BOX_APPLY_SET            13
#define BOX_GENERATE_RESPONSE    14

#define BOX_SCHEMA_NETWORK       21
#define BOX_WORKING_MEMORY       22
#define BOX_RESPONSE_BUFFER      23

// #define DEBUG
#undef DEBUG

/*******************************************************************************

Hypothesis / assumption:

* DVs that pattern together in dual task experiment do so as a result of
sharing a common process (which is tapped by the secondary task).

Implications:

* RNG, TPI and all second order measures are due to process A (greater
emphasis on existing biases in schema selection)

* R and RG are due to process B (failure to update WM)

So, the basic model should capture RNG and TPI through the schema selection
bias (which is assumed to be affected by A), not through a monitoring
subprocess (which is affected by B). That is, schema selection should
impose the clockwise / anti-clockwise idea.

Note that these assumptions also have implications for WCST and ToL, given the
dual-task experiment.

*******************************************************************************/

#if SCHEMA_SET_SIZE == 11

// Schemas are: [Repeat, +1, +2, +3, +4, OppositeA, OppositB, -4, -3, -2, -1]

// Note that with the Botlzmann selection mechanism, the data tell us that:
// * adjacent clockwise must have the highest weight;
// * adjacent counterclockwise must have the next highest weight;
// * opposite must be lower than either of those;


//double strength[SCHEMA_SET_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

double strength[SCHEMA_SET_SIZE] = {
    0.10, 
    1.80,
    1.40, 
    1.35,
    1.40, 
    1.05,
    1.05, 
    1.10,
    1.05,
    1.10, 
    1.45
};

char *slabels[SCHEMA_SET_SIZE] = {
    "repeat",
    "plus_one",
    "plus_two",
    "plus_three",
    "plus_four",
    "oppositeA",
    "oppositeB",
    "minus_four",
    "minus_three",
    "minus_two",
    "minus_one"
};

#endif

#if SCHEMA_SET_SIZE == 12
// Schemas are: [Repeat, +1, +2, +3, +4, +5, Opposite, -5, -4, -3, -2, -1]

static double strength[SCHEMA_SET_SIZE] = {0.900, 1.003, 0.985, 0.980, 0.985, 0.908, 0.990, 0.980, 0.967, 0.966, 0.966, 1.000};

#endif

/******************************************************************************/

#ifdef DEBUG

static int schema_counts[SCHEMA_SET_SIZE];

static void initialise_schema_counts()
{
    int i;

    for (i = 0; i < SCHEMA_SET_SIZE; i++) {
        schema_counts[i] = 0;
    }
}

static void fprint_schema_counts(FILE *fp, OosVars *gv)
{
    if (fp != NULL) {
	int sum = 0, i;

	for (i = 0; i < SCHEMA_SET_SIZE; i++) {
	    sum += schema_counts[i];
	}
	fprintf(fp, "Empirical distribution with %d samples with %d subjects:\n", sum, gv->block);
	if (sum > 0) {
	    for (i = 0; i < SCHEMA_SET_SIZE; i++) {
		fprintf(fp, "%2d: %6.4f\n", i, schema_counts[i] / (double) sum);
	    }
	}
	else {
	    fprintf(fp, "NONE\n");
	}
	fprintf(fp, "\n");
    }
}
#endif

/******************************************************************************/

static int select_from_probability_distribution(double weights[SCHEMA_SET_SIZE], double temperature)
{
    // Select an element between 1 and SCHEMA_SET_SIZE at random given the weights associated
    // with those elements.

    // Note: We no longer use a temperature parameter!

    double limit;
    double weighted_sum;
    int s;

    weighted_sum = 0;
    for (s = 0; s < SCHEMA_SET_SIZE; s++) {
        weighted_sum += exp(log(weights[s])/temperature);
    }

    limit = random_uniform(0.0, weighted_sum);
    s = -1;
    do {
        s++;
        limit -= exp(log(weights[s])/temperature);
    } while ((limit > 0) && (s < SCHEMA_SET_SIZE));
    return(s);
}

static ClauseType *select_weighted_schema(OosVars *gv)
{
    RngData *task_data = (RngData *)(gv->task_data);
    char buffer[16];
    int selection;

    selection = select_from_probability_distribution(task_data->strengths, task_data->params.selection_temperature);

#ifdef DEBUG
    // If DEBUG is defined then keep track of how many times each schema is
    // selected, so we can compare this to the expected distribution based on
    // the calculated probabilities:
    schema_counts[selection]++;
#endif

    if (selection < SCHEMA_SET_SIZE) {
        g_snprintf(buffer, 16, "%s.", slabels[selection]);
    }
    else {
        strncpy(buffer, "error.", 16);
    }
    return(pl_clause_make_from_string(buffer));
}

static void strategy_output(OosVars *gv)
{
    ClauseType *current_set, *response;
    RngData *task_data = (RngData *)(gv->task_data);

    response = pl_clause_make_from_string("response(_,_).");
    current_set = pl_clause_make_from_string("schema(_,selected).");

    /* If there is a previous response in working memory, but no current set */
    /* (schema), then select a new schema at random subject to individual weights */
    if (oos_match(gv, BOX_WORKING_MEMORY, response) && !oos_match(gv, BOX_SCHEMA_NETWORK, current_set)) {
	pl_arg_set(current_set, 1, select_weighted_schema(gv));
        pl_arg_set(current_set, 2, pl_clause_make_from_string("unselected."));
        oos_message_create(gv, MT_DELETE, BOX_STRATEGY, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
        pl_arg_set(current_set, 2, pl_clause_make_from_string("selected."));
        oos_message_create(gv, MT_ADD, BOX_STRATEGY, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
    }
    pl_clause_free(current_set);
    pl_clause_free(response);

    if (task_data->params.switch_rate > random_uniform(0.0, 1.0)) {
        response = pl_clause_make_from_string("response(_,_).");
        current_set = pl_clause_make_from_string("schema(_,selected).");
        pl_arg_set_to_int(response, 2, gv->cycle-1);

        if (oos_match(gv, BOX_WORKING_MEMORY, response) && oos_match(gv, BOX_SCHEMA_NETWORK, current_set)) {
            oos_message_create(gv, MT_DELETE, BOX_STRATEGY, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
            pl_arg_set(current_set, 2, pl_clause_make_from_string("unselected."));
            oos_message_create(gv, MT_ADD, BOX_STRATEGY, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
        }
        pl_clause_free(current_set);
        pl_clause_free(response);
    }
}

/*----------------------------------------------------------------------------*/

static Boolean check_random(OosVars *gv, long r)
{
    ClauseType *template;
    long pr;

    /* Return TRUE if this item appears to be random */

    /* Build the template based on the proposed response: */
    template = pl_clause_make_from_string("response(_,_).");

    /* If it matches the most recent response in WM, then it is an      */
    /* intentional repeat ... consider it random!                       */
    if ((oos_match(gv, BOX_WORKING_MEMORY, template)) && pl_is_integer(pl_arg_get(template, 1), &pr) && (pr == r)) {
	/* Free the template and return the result: */
	pl_clause_free(template);
	return(TRUE);
    }
    else {
        /* Otherwise, fill in the template with the putative response. If it matches WM it isn't random: */
	pl_clause_free(template);
        template = pl_clause_make_from_string("response(_,_).");
        pl_arg_set_to_int(template, 1, r);
        if (oos_match(gv, BOX_WORKING_MEMORY, template)) {
            /* Free the template and return the result: */
            pl_clause_free(template);
            return(FALSE);
        }
        else {
            RngData *task_data = (RngData *)(gv->task_data);
            Boolean result = TRUE;

            pl_clause_free(template);

            if (task_data->params.monitoring_method > 0) {
                /* Smarter monitoring: Check for local sequences */

                TimestampedClauseList *contents;
                long p1, p2;
                int g1, g2;

                contents = oos_buffer_get_contents(gv, BOX_WORKING_MEMORY);

                if ((contents != NULL) && (contents->tail != NULL)) {
                    if (pl_is_integer(pl_arg_get(contents->head, 1), &p1) && pl_is_integer(pl_arg_get(contents->tail->head, 1), &p2)) {

                        // "r" is the putative response, p1 is the previous response, p2 is the
                        // response before that. g1 is the gap between the current and p1.
                        // g2 is the gap between p1 and p2. We might reject if the gap is
                        // the same (so non random), or we might reject if we've gone in the same
                        // direction twice in succession.

                        g1 = (r + RESPONSE_SET_SIZE - p1) % RESPONSE_SET_SIZE;
                        g2 = (p1 + RESPONSE_SET_SIZE - p2) % RESPONSE_SET_SIZE;

                        if (task_data->params.monitoring_method == 2) {
                            /* Extra smart monitoring: Check direction */
                            if ((g1 > 5) && (g2 > 5)) {
                                result = FALSE;
                            }
                            else if ((g1 < 5) && (g2 < 5)) {
                                result = FALSE;
                            }
                        }
                        if (g1 == g2) {
                            result = FALSE;
                        }
                    }
                }
            }
            return(result);
        }
    }
}

static void monitoring_output(OosVars *gv)
{
    RngData *task_data = (RngData *)(gv->task_data);

    if (task_data->params.monitoring_efficiency > random_uniform(0.0, 1.0)) {
	ClauseType *proposed, *current_set;
	long r;

	proposed = pl_clause_make_from_string("response(_,_).");
	if (oos_match(gv, BOX_RESPONSE_BUFFER, proposed)) {
	    if (pl_is_integer(pl_arg_get(proposed, 1), &r) && !check_random(gv, r)) {
                // Don't generate the response - it is insufficiently random
                oos_message_create(gv, MT_DELETE, BOX_MONITORING, BOX_RESPONSE_BUFFER, pl_clause_copy(proposed));
                // Also deselect the selected schema to foce selection of a new schema
                current_set = pl_clause_make_from_string("schema(_,selected).");
                if (oos_match(gv, BOX_SCHEMA_NETWORK, current_set)) {
                    oos_message_create(gv, MT_DELETE, BOX_MONITORING, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
                    pl_arg_set(current_set, 2, pl_clause_make_from_string("unselected."));
                    oos_message_create(gv, MT_ADD, BOX_MONITORING, BOX_SCHEMA_NETWORK, pl_clause_copy(current_set));
		}
                pl_clause_free(current_set);
	    }
	}
        pl_clause_free(proposed);
    }
}

/*----------------------------------------------------------------------------*/

static int apply_schema(ClauseType *schema, ClauseType *seed)
{
    long last;

    if (!functor_comp(seed, "response", 2)) {
	return(-1);
    }
    else if (!pl_is_integer(pl_arg_get(seed, 1), &last)) {
	return(-1);
    }

    if (functor_comp(schema, "schema", 2)) {
	char *s = pl_functor(pl_arg_get(schema, 1));

#if SCHEMA_SET_SIZE == 11
	if (strcmp(s, "minus_four") == 0) {
	    return((last + 6) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_three") == 0) {
	    return((last + 7) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_two") == 0) {
	    return((last + 8) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_one") == 0) {
	    return((last + 9) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "repeat") == 0) {
	    return(last);
	}
	else if (strcmp(s, "plus_one") == 0) {
	    return((last + 1) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_two") == 0) {
	    return((last + 2) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_three") == 0) {
	    return((last + 3) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_four") == 0) {
	    return((last + 4) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "oppositeA") == 0) {
	    return((last + 5) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "oppositeB") == 0) {
	    return((last + 5) % RESPONSE_SET_SIZE);
	}
#endif
#if SCHEMA_SET_SIZE == 12
	if (strcmp(s, "minus_five") == 0) {
	    return((last + 7) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_four") == 0) {
	    return((last + 8) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_three") == 0) {
	    return((last + 9) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_two") == 0) {
	    return((last + 10) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "minus_one") == 0) {
	    return((last + 11) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "repeat") == 0) {
	    return(last);
	}
	else if (strcmp(s, "plus_one") == 0) {
	    return((last + 1) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_two") == 0) {
	    return((last + 2) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_three") == 0) {
	    return((last + 3) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_four") == 0) {
	    return((last + 4) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "plus_five") == 0) {
	    return((last + 5) % RESPONSE_SET_SIZE);
	}
	else if (strcmp(s, "opposite") == 0) {
	    return((last + 6) % RESPONSE_SET_SIZE);
	}

#endif

    }
    return(-1);
}

static void apply_set_output(OosVars *gv)
{
    ClauseType *seed, *current_set, *previous, *content;

    seed = pl_clause_make_from_string("response(_,_).");
    previous = pl_clause_make_from_string("_.");
    current_set = pl_clause_make_from_string("schema(_,selected).");

    if (!oos_match(gv, BOX_RESPONSE_BUFFER, previous)) {
        if (oos_match(gv, BOX_WORKING_MEMORY, seed)) {
            if (oos_match(gv, BOX_SCHEMA_NETWORK, current_set)) {
                content = pl_clause_make_from_string("response(_,_).");
                pl_arg_set_to_int(content, 1, apply_schema(current_set, seed));
                pl_arg_set_to_int(content, 2, gv->cycle);
                oos_message_create(gv, MT_ADD, BOX_APPLY_SET, BOX_RESPONSE_BUFFER, content);
            }
        }
        else {
            content = pl_clause_make_from_string("response(_,_).");
            pl_arg_set_to_int(content, 1, random_integer(0, RESPONSE_SET_SIZE));
            pl_arg_set_to_int(content, 2, gv->cycle);
            oos_message_create(gv, MT_ADD, BOX_APPLY_SET, BOX_RESPONSE_BUFFER, content);
        }
    }
    pl_clause_free(seed);
    pl_clause_free(previous);
    pl_clause_free(current_set);
}

/*----------------------------------------------------------------------------*/

static void generate_response_output(OosVars *gv)
{
    ClauseType *template;
    RngData *task_data = (RngData *)(gv->task_data);
    RngSubjectData *subject;
    long r, t2;

    subject = &(task_data->subject[gv->block]);

    template = pl_clause_make_from_string("response(_,_).");
    if (oos_match(gv, BOX_RESPONSE_BUFFER, template)) {
        if (pl_is_integer(pl_arg_get(template, 2), &t2) && (gv->cycle == t2+2)) {
            oos_message_create(gv, MT_DELETE, BOX_GENERATE_RESPONSE, BOX_RESPONSE_BUFFER, pl_clause_copy(template));
            pl_arg_set_to_int(template, 2, gv->cycle);
            /* Update WM on some well-defined percentage of trails: */
            if (task_data->params.wm_update_efficiency > random_uniform(0.0, 1.0)) {
                oos_message_create(gv, MT_ADD, BOX_GENERATE_RESPONSE, BOX_WORKING_MEMORY, pl_clause_copy(template));
            }
            /* Produce the response, by adding it the current subject's */
            /* response list: */
            if (pl_is_integer(pl_arg_get(template, 1), &r)) {
                subject->response[(subject->n)++] = (int) r;
            }
        }
    }
    pl_clause_free(template);
    if (subject->n >= gv->trials_per_subject) {
        oos_message_create(gv, MT_STOP, BOX_GENERATE_RESPONSE, 0, NULL);
    }
}

/******************************************************************************/

static CairoxPoint *coordinate_list_create(int points)
{
    CairoxPoint *coords = (CairoxPoint *)malloc(sizeof(CairoxPoint)*points);
    return(coords);
}

static void coordinate_list_set(CairoxPoint *coordinates, int index, double x, double y)
{
    coordinates[index-1].x = x;
    coordinates[index-1].y = y;
}

#define Y_SCALE  1.25

Boolean rng_create(OosVars *gv, RngParameters *pars)
{
    RngData *task_data;
    CairoxPoint *coordinates;

    oos_model_free(gv);
    oos_messages_free(gv);

    g_free(gv->task_data);

    gv->name = string_copy("RNG");
    gv->cycle = 0;
    gv->block = 0;
    gv->stopped = FALSE;

    oos_process_create(gv, "Strategy Generation", BOX_STRATEGY, 0.2, 0.1*Y_SCALE, strategy_output);
    oos_process_create(gv, "Monitoring", BOX_MONITORING, 0.8, 0.1*Y_SCALE, monitoring_output);
    oos_process_create(gv, "Apply Set", BOX_APPLY_SET, 0.2, 0.7*Y_SCALE, apply_set_output);
    oos_process_create(gv, "Generate Response", BOX_GENERATE_RESPONSE, 0.8, 0.7*Y_SCALE, generate_response_output);

    oos_buffer_create(gv, "Schema Network", BOX_SCHEMA_NETWORK, 0.2, 0.4*Y_SCALE, 
                                            BUFFER_DECAY_NONE, 0, 
                                            BUFFER_CAPACITY_UNLIMITED, 0, 
					    BUFFER_EXCESS_IGNORE, 
					    BUFFER_ACCESS_RANDOM);
    oos_buffer_create(gv, "Working Memory", BOX_WORKING_MEMORY, 0.8, 0.4*Y_SCALE, 
//                                            BUFFER_DECAY_EXPONENTIAL, pars->wm_decay_rate,
//                                            BUFFER_DECAY_WEIBULL, pars->wm_decay_rate, 
                                            BUFFER_DECAY_QUADRATIC, pars->wm_decay_rate, 
                                            BUFFER_CAPACITY_UNLIMITED, 0, 
					    BUFFER_EXCESS_IGNORE, 
					    BUFFER_ACCESS_LIFO);
    oos_buffer_create(gv, "Response Buffer", BOX_RESPONSE_BUFFER, 0.5, 0.7*Y_SCALE, 
                                            BUFFER_DECAY_NONE, 0, 
					    BUFFER_CAPACITY_LIMITED, 1, 
					    BUFFER_EXCESS_RANDOM, 
					    BUFFER_ACCESS_RANDOM);

    // Strategy sends to Schema Network
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.190, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.190, 0.360*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_SHARP, coordinates, 2, 1.0);
    // Strategy reads from Schema Network
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.200, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.200, 0.360*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_BLUNT, coordinates, 2, 1.0);
    // Strategy reads from Working Memory
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.200, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.200, 0.230*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.795, 0.230*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.795, 0.360*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_BLUNT, coordinates, 4, 1.0);
//    // Strategy reads from Response Buffer
//    coordinates = coordinate_list_create(4);
//    coordinate_list_set(coordinates, 1, 0.200, 0.140*Y_SCALE);
//    coordinate_list_set(coordinates, 2, 0.200, 0.230*Y_SCALE);
//    coordinate_list_set(coordinates, 3, 0.490, 0.230*Y_SCALE);
//    coordinate_list_set(coordinates, 4, 0.490, 0.660*Y_SCALE);
//    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_BLUNT, coordinates, 4, 1.0);

    // Monitoring sends to Schema Network
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.795, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.795, 0.270*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.210, 0.270*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.210, 0.360*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_SHARP, coordinates, 4, 1.0);
    // Monitoring sends to Response Buffer
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.795, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.795, 0.270*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.500, 0.270*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.500, 0.660*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_SHARP, coordinates, 4, 1.0);
    // Monitoring reads from Response Buffer
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.805, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.805, 0.285*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.510, 0.285*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.510, 0.660*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_BLUNT, coordinates, 4, 1.0);
    // Monitoring reads from Working Memory
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.805, 0.140*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.805, 0.360*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_BLUNT, coordinates, 2, 1.0);

    // Apply Set reads from Schema Network
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.20, 0.660*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.20, 0.440*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_BLUNT, coordinates, 2, 1.0);
    // Apply Set reads from Working Memory
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.20, 0.660*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.20, 0.550*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.795, 0.550*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.795, 0.440*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_SOLID, AH_BLUNT, coordinates, 4, 1.0);
    // Generate Response sends to Working Memory
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.805, 0.660*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.805, 0.440*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_SHARP, coordinates, 2, 1.0);
    // Apply Set reads from Response Buffer
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.260, 0.69*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.440, 0.69*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_BLUNT, coordinates, 2, 1.0);
    // Apply Set seads to Response Buffer
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.260, 0.71*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.440, 0.71*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_SHARP, coordinates, 2, 1.0);

    // Generate Response sends to Response Buffer (0.735, 0.79 -> 0.565, 0.79)
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.740, 0.71*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.560, 0.71*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_SHARP, coordinates, 2, 1.0);
    // Generate Response reads from Response Buffer (0.735, 0.81 -> 0.565, 0.81)
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.740, 0.69*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.560, 0.69*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_BLUNT, coordinates, 2, 1.0);
    // Generate Response sends to Output 
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.860, 0.70*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.960, 0.70*Y_SCALE);
    oos_arrow_create(gv, VS_SHARP, LS_SOLID, AH_SHARP, coordinates, 2, 1.0);

    // Annotations: meta-component labels
    oos_annotation_create(gv, "Supervisory", 0.06, 0.25*Y_SCALE, 14, 0.0, TRUE);
    oos_annotation_create(gv, "System", 0.06, 0.28*Y_SCALE, 14, 0.0, TRUE);
    oos_annotation_create(gv, "Contention", 0.06, 0.68*Y_SCALE, 14, 0.0, TRUE);
    oos_annotation_create(gv, "Scheduling", 0.06, 0.71*Y_SCALE, 14, 0.0, TRUE);
    // Annotations: arrow labels
    oos_annotation_create(gv, "Switch", 0.380, 0.270*Y_SCALE, 12, 0.0, FALSE);
    oos_annotation_create(gv, "Inhibit", 0.500, 0.490*Y_SCALE, 12, 90.0, FALSE);
    oos_annotation_create(gv, "Update", 0.805, 0.540*Y_SCALE, 12, 90.0, FALSE);
    oos_annotation_create(gv, "Task Setting", 0.190, 0.240*Y_SCALE, 12, 90.0, FALSE);

    // Annotation: A dotted line between CS and SS (just for visual clarity):
//    int k;
//    for (k = 0; k < 100; k++) {
//        coordinates = coordinate_list_create(2);
//        coordinate_list_set(coordinates, 1, 0.05 + 0.90 * k / 100.0, 0.61*Y_SCALE);
//        coordinate_list_set(coordinates, 2, 0.05 + 0.90 * (k + 0.5) / 100.0, 0.61*Y_SCALE);
//        oos_arrow_create(gv, VS_CURVED, LS_DASHED, H_NONE, coordinates, 2, 1.0);
//    }
//    int k;

    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.03, 0.31*Y_SCALE);
    coordinate_list_set(coordinates, 2, 0.35, 0.31*Y_SCALE);
    coordinate_list_set(coordinates, 3, 0.35, 0.61*Y_SCALE);
    coordinate_list_set(coordinates, 4, 0.95, 0.61*Y_SCALE);
    oos_arrow_create(gv, VS_CURVED, LS_DASHED, AH_NONE, coordinates, 4, 1.0);

    if ((task_data = (RngData *)malloc(sizeof(RngData))) != NULL) {
	int i, j;
	for (i = 0; i < MAX_SUBJECTS; i++) {
	    task_data->subject[i].n = 0;
	    for (j = 0; j < MAX_TRIALS; j++) {
		task_data->subject[i].response[j] = -1;
	    }
	}
	task_data->group.n = 0;
	task_data->params.wm_decay_rate = pars->wm_decay_rate;
	task_data->params.wm_update_efficiency = pars->wm_update_efficiency;
	task_data->params.selection_temperature = pars->selection_temperature;
	task_data->params.switch_rate = pars->switch_rate;
	task_data->params.monitoring_method = pars->monitoring_method;
	task_data->params.monitoring_efficiency = pars->monitoring_efficiency;
	task_data->params.individual_variability = pars->individual_variability;
	task_data->params.sample_size = pars->sample_size;
	gv->task_data = (void *)task_data;
    }

    oos_initialise_session(gv, 100, task_data->params.sample_size);

    return(gv->task_data != NULL);
}

void rng_initialise_subject(OosVars *gv)
{
    RngData *task_data;
    char buffer[64];
    int i;

    task_data = (RngData *)gv->task_data;

    /* Initialise subject-specific schema strengths (with individual noise) */

    for (i = 0; i < SCHEMA_SET_SIZE; i++) {
        double w = random_normal(1.0, task_data->params.individual_variability);
        task_data->strengths[i] = (w <=0 ? 0.001 : strength[i] * w);

        g_snprintf(buffer, 64, "schema(%s,unselected).", slabels[i]);
        oos_buffer_create_element(gv, BOX_SCHEMA_NETWORK, buffer, 1.0);
    }
}

void rng_globals_destroy(RngData *task_data)
{
    g_free(task_data);
}

#if DEBUG
static void rng_print_parameters(FILE *fp, RngData *task_data)
{
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "WM Decay: %d\n", task_data->params.wm_decay_rate);
    fprintf(fp, "WM Update efficiency: %5.3f\n", task_data->params.wm_update_efficiency);
    fprintf(fp, "Switch efficiency: %5.3f\n", task_data->params.switch_rate);
    fprintf(fp, "Monitoring efficiency: %5.3f\n", task_data->params.monitoring_efficiency);
    fprintf(fp, "Monitoring method: %d\n", task_data->params.monitoring_method);
}
#endif

/******************************************************************************/

void rng_run(OosVars *gv)
{
    RngData *task_data;
    RngSubjectData *subject;
    FILE *fp = NULL;

#ifdef DEBUG
    initialise_schema_counts();
#endif
    task_data = (RngData *)gv->task_data;
#ifdef DEBUG
    rng_print_parameters(stdout, task_data);
#endif
    while (gv->block < gv->subjects_per_experiment) {
	oos_initialise_trial(gv);
	rng_initialise_subject(gv);
	while (oos_step(gv)) {
#ifdef DEBUG
	    oos_dump(gv, TRUE);
#endif
	}
        subject = &(task_data->subject[gv->block]);
	rng_analyse_subject_responses(fp, subject, gv->trials_per_subject);
	oos_step_block(gv);
        task_data->group.n = gv->block;
    }
#ifdef DEBUG
    fprint_schema_counts(fp, gv);
#endif

    if ((fp != NULL) && (fp != stdout)) {
        fclose(fp);
    }
}

/******************************************************************************/
