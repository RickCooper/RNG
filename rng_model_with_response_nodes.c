
#include "rng.h"

#define BOX_TASK_SETTING        11
#define BOX_MONITORING          12
#define BOX_APPLY_SET           13
#define BOX_GENERATE_RESPONSE   14
#define BOX_PROPOSE_RESPONSE    15

#define BOX_CURRENT_SET         21
#define BOX_WORKING_MEMORY      22
#define BOX_RESPONSE_BUFFER     23
#define BOX_RESPONSE_NODES      24

#define MATCH_THRESHOLD 0.8

// #define DEBUG
#undef DEBUG

/*******************************************************************************

Note: The most basic model fails to capture TPI data - it fails to switch
direction often enough. In principle, direction switching might be imposed by
monitoring or by switching.

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

Best parameter fits based on genetic algorithm:

BASE (99)   0: 0.339466 [Decay:  404, Temp: 40.520, Monitoring: 0.377, Updating:  1.000]
SD   (99)   0: 0.244245 [Decay:  452, Temp: 17.376, Monitoring: 0.416, Updating:  1.000]
V1   (99)   0: 0.160653 [Decay:  428, Temp: 104.213, Monitoring: 1.000, Updating:  0.898]
V2   (99)   0: 0.545337 [Decay:  431, Temp: 86.068, Monitoring: 0.497, Updating:  1.000]

*******************************************************************************/

#include "rng_flags.h"

// Schemas are: [-4, -3, -2, -1, 0, +1, +2, +3, +4, +5]

#if SWITCH_DIRECTION_BIAS
static double strength1[10] = {0.010, 0.010, 0.010, 0.010, 0.002, 0.168, 0.125, 0.108, 0.110, 0.220};
static double strength2[10] = {0.145, 0.090, 0.085, 0.110, 0.002, 0.010, 0.010, 0.010, 0.010, 0.010};
#else
static double strength[10] = {0.100, 0.090, 0.080, 0.100, 0.000, 0.120, 0.110, 0.100, 0.100, 0.110};
#endif

/******************************************************************************/

#ifdef DEBUG
static int schema_counts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void fprint_schema_counts(FILE *fp, OosVars *gv)
{
    if (fp != NULL) {
	int sum = 0, i;

	for (i = 0; i < 10; i++) {
	    sum += schema_counts[i];
	}
	fprintf(fp, "Empirical distribution with %d samples with %d subjects:\n", sum, gv->block);
	if (sum > 0) {
	    for (i = 0; i < 10; i++) {
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

static int select_from_probability_distribution(double weights[10], double temperature)
{
    // Select an element between 1 and 10 at random given the weights associated
    // with those elements, and the temperature, using a Boltzmann distribution.

    // I'm confused about this. The Boltzmann equation would suggest exp(-E/T),
    // but exp(E*T) is what actually works. With this, when temperature is low
    // stochasticity is high (all options are equally likely), so we are more
    // likely to choose any option (like low load). When temperature is high,
    // stochasticity is low and so we are more predictable (high weight options
    // dominate) and more likely to show inbuilt biases (like high load).

    double limit;
    double weighted_sum;
    int s;

    weighted_sum = 0;
    for (s = 0; s < 10; s++) {
        weighted_sum += exp(weights[s]*temperature);
    }

    limit = weighted_sum * random_uniform(0.0, 1.0);
    s = -1;
    do {
        s++;
        limit -= exp(weights[s]*temperature);
    } while ((limit > 0) && (s < 10));
    return(s);
}

static ClauseType *select_weighted_schema(OosVars *gv)
{
    RngData *task_data = (RngData *)(gv->task_data);
    int selection;

#if SWITCH_DIRECTION_BIAS
    static Boolean clockwise = TRUE;

    if (clockwise) {
        selection = select_from_probability_distribution(strength1, task_data->params.temperature);
        clockwise = FALSE;
    }
    else {
        selection = select_from_probability_distribution(strength2, task_data->params.temperature);
	clockwise = TRUE;
    }
#else
    selection = select_from_probability_distribution(strength, task_data->params.temperature);
#endif

#ifdef DEBUG
    // If DEBUG is defined then keep track of how many times each schema is
    // selected, so we can compare this to the expected distribution based on
    // the calculated probabilities:
    schema_counts[selection]++;
#endif

    switch (selection) {
	case 0: { return(pl_clause_make_from_string("minus_four.")); break; }
	case 1: { return(pl_clause_make_from_string("minus_three.")); break; }
	case 2: { return(pl_clause_make_from_string("minus_two.")); break; }
	case 3: { return(pl_clause_make_from_string("minus_one.")); break; }
	case 4: { return(pl_clause_make_from_string("repeat.")); break; }
	case 5: { return(pl_clause_make_from_string("plus_one.")); break; }
	case 6: { return(pl_clause_make_from_string("plus_two.")); break; }
	case 7: { return(pl_clause_make_from_string("plus_three.")); break; }
	case 8: { return(pl_clause_make_from_string("plus_four.")); break; }
	case 9: { return(pl_clause_make_from_string("opposite.")); break; }
	default: { return(pl_clause_make_from_string("error.")); break; }
    }
}

static void task_setting_output(OosVars *gv)
{
    ClauseType *current_set, *schema, *response;

    response = pl_clause_make_from_string("response(_,_).");
    current_set = pl_clause_make_from_string("_.");

    /* If there is a previous response in working memory, but no current set */
    /* (schema), then select a new schema at random subject to individual weights */
    if (oos_match(gv, BOX_WORKING_MEMORY, response) && !oos_match(gv, BOX_CURRENT_SET, current_set)) {
	schema = pl_clause_make_from_string("schema(_).");
	pl_arg_set(schema, 1, select_weighted_schema(gv));
	oos_message_create(gv, MT_ADD, BOX_TASK_SETTING, BOX_CURRENT_SET, schema);
    }
    pl_clause_free(current_set);
    pl_clause_free(response);
}

/*----------------------------------------------------------------------------*/

static Boolean check_random(OosVars *gv, long r)
{
    ClauseType *template;

    /* Return TRUE if this item appears to be random */

    /* Build the template based on the proposed response: */
    template = pl_clause_make_from_string("response(_,_).");
    pl_arg_set_to_int(template, 1, r);
    /* If it matches WM, then it isn't random: */
    if (oos_match(gv, BOX_WORKING_MEMORY, template)) {
	/* Free the template and return the result: */
	pl_clause_free(template);
	return(FALSE);
    }
    else {
	Boolean result = TRUE;

	pl_clause_free(template);

#if REJECT_SMART_V1 | REJECT_SMART_V2

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

		g1 = (r + 10 - p1) % 10;
		g2 = (p1 + 10 - p2) % 10;
#if REJECT_SMART_V2
		if ((g1 > 5) && (g2 > 5)) {
		    result = FALSE;
		}
		else if ((g1 < 5) && (g2 < 5)) {
		    result = FALSE;
		}
#endif
		if (g1 == g2) {
		    result = FALSE;
		}
	    }
	}
#endif
	return(result);
    }
}

static void monitoring_output(OosVars *gv)
{
    RngData *task_data = (RngData *)(gv->task_data);

    if (task_data->params.monitoring_efficiency > random_uniform(0.0, 1.0)) {
	ClauseType *proposed;
	long r;

	proposed = pl_clause_make_from_string("response(_,_).");
	if (oos_match(gv, BOX_RESPONSE_BUFFER, proposed)) {
	    if (pl_is_integer(pl_arg_get(proposed, 1), &r) && !check_random(gv, r)) {
		/* The thing we need to inhibit is of the form "response(_)", */
		/* so adjust the arity of the thing we matched which had the  */
		/* form "response(_,_)" 				      */
		pl_arity_adjust(proposed, 1);
		oos_message_create(gv, MT_CLEAR, BOX_MONITORING, BOX_CURRENT_SET, NULL);
		oos_message_create(gv, MT_INHIBIT, BOX_MONITORING, BOX_RESPONSE_NODES, proposed);
		// Inhibit / clear the item from BOX_RESPONSE_NODES (fix from NS):
		oos_message_create(gv, MT_CLEAR, BOX_MONITORING, BOX_RESPONSE_BUFFER, NULL);
	    }
	    else {
		pl_clause_free(proposed);
	    }
	}
	else {
	    pl_clause_free(proposed);
	}
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

    if (functor_comp(schema, "schema", 1)) {
	char *s = pl_functor(pl_arg_get(schema, 1));

	if (strcmp(s, "minus_four") == 0) {
	    return((last + 6) % 10);
	}
	else if (strcmp(s, "minus_three") == 0) {
	    return((last + 7) % 10);
	}
	else if (strcmp(s, "minus_two") == 0) {
	    return((last + 8) % 10);
	}
	else if (strcmp(s, "minus_one") == 0) {
	    return((last + 9) % 10);
	}
	else if (strcmp(s, "repeat") == 0) {
	    return(last);
	}
	else if (strcmp(s, "plus_one") == 0) {
	    return((last + 1) % 10);
	}
	else if (strcmp(s, "plus_two") == 0) {
	    return((last + 2) % 10);
	}
	else if (strcmp(s, "plus_three") == 0) {
	    return((last + 3) % 10);
	}
	else if (strcmp(s, "plus_four") == 0) {
	    return((last + 4) % 10);
	}
	else if (strcmp(s, "opposite") == 0) {
	    return((last + 5) % 10);
	}
    }
    return(-1);
}

static void apply_set_output(OosVars *gv)
{
    ClauseType *seed, *current_set, *previous, *content, *template;

    template = pl_clause_make_from_string("response(_).");
    seed = pl_clause_make_from_string("response(_,_).");
    previous = pl_clause_make_from_string("_.");
    current_set = pl_clause_make_from_string("schema(_).");

    /* If there is no response already lined up for production, and none of   */
    /* the possible responses are above threshold, then ...		      */
    if (!oos_match(gv, BOX_RESPONSE_BUFFER, previous) && !oos_match_above_threshold(gv, BOX_RESPONSE_NODES, template, MATCH_THRESHOLD)) {
	/* If we have a previous response to use as a seed, then ...	      */
	if (oos_match(gv, BOX_WORKING_MEMORY, seed)) {
	    /* If there is also a current schema, then ...		      */
	    if (oos_match(gv, BOX_CURRENT_SET, current_set)) {
		/* Apply the current schema to the seed and excite the result */
		content = pl_clause_make_from_string("response(_).");
		pl_arg_set_to_int(content, 1, apply_schema(current_set, seed));
		oos_message_create(gv, MT_EXCITE, BOX_APPLY_SET, BOX_RESPONSE_NODES, content);
	    }
	}
	/* Otherwise, with no previous response to use as a seed, excite a   */
	/* response node at random:					      */
	else {
	    content = pl_clause_make_from_string("response(_).");
	    pl_arg_set_to_int(content, 1, random_integer(0, 10));
	    oos_message_create(gv, MT_EXCITE, BOX_APPLY_SET, BOX_RESPONSE_NODES, content);
	}
    }
    pl_clause_free(seed);
    pl_clause_free(template);
    pl_clause_free(previous);
    pl_clause_free(current_set);
}

/*----------------------------------------------------------------------------*/

static void propose_response_output(OosVars *gv)
{
    ClauseType *template;

    template = pl_clause_make_from_string("response(_).");
    if (oos_match_above_threshold(gv, BOX_RESPONSE_NODES, template, MATCH_THRESHOLD)) {

	pl_arity_adjust(template, 2);
	/* Note: Arg 2 of template will be "_" - we want it to be a var while */
	/* we check for membership in Response Buffer			      */

	if (!oos_match(gv, BOX_RESPONSE_BUFFER, template)) {
	    pl_arg_set_to_int(template, 2, gv->cycle);
	    oos_message_create(gv, MT_ADD, BOX_PROPOSE_RESPONSE, BOX_RESPONSE_BUFFER, template);
	}
	else {
	    /* The response has already been proposed - do nothing:	      */
	    pl_clause_free(template);
	}
    }
    else {
	pl_clause_free(template);
    }
}

static void generate_response_output(OosVars *gv)
{
    ClauseType *template, *node;
    RngData *task_data = (RngData *)(gv->task_data);
    RngSubjectData *subject;
    long r, t2;

    subject = &(task_data->trial[gv->block]);

    template = pl_clause_make_from_string("response(_,_).");
    if (oos_match(gv, BOX_RESPONSE_BUFFER, template)) {
	if (pl_is_integer(pl_arg_get(template, 2), &t2) && (gv->cycle > t2+3)) {

	    /* Inhibit a response node when the response is proposed (NEW): */
	    node = pl_clause_copy(template);
	    pl_arity_adjust(node, 1);
	    oos_message_create(gv, MT_INHIBIT, BOX_GENERATE_RESPONSE, BOX_RESPONSE_NODES, node);

	    /* Update WM on some well-defined percentage of trails: */
	    if (task_data->params.wm_update_efficiency > random_uniform(0.0, 1.0)) {
		oos_message_create(gv, MT_ADD, BOX_GENERATE_RESPONSE, BOX_WORKING_MEMORY, pl_clause_copy(template));
	    }
	    /* Produce the response - by adding it the current subject's */
	    /* response list: */
	    if (pl_is_integer(pl_arg_get(template, 1), &r)) {
		subject->response[(subject->n)++] = (int) r;
//                fprintf(stdout, "CYCLE %4d\tRESPONSE %3d: %d\n", gv->cycle, subject->n, (int) r);
	    }
	    oos_message_create(gv, MT_CLEAR, BOX_GENERATE_RESPONSE, BOX_RESPONSE_BUFFER, NULL);
	}
    }
    pl_clause_free(template);
    if (subject->n >= gv->trials_per_subject) {
	oos_message_create(gv, MT_STOP, BOX_GENERATE_RESPONSE, 0, NULL);
    }
}

/******************************************************************************/

CairoxPoint *coordinate_list_create(int points)
{
    CairoxPoint *coords = (CairoxPoint *)malloc(sizeof(CairoxPoint)*points);
    return(coords);
}

static void coordinate_list_set(CairoxPoint *coordinates, int index, double x, double y)
{
    coordinates[index-1].x = x;
    coordinates[index-1].y = y;
}

Boolean rng_create(OosVars *gv, RngParameters *pars)
{
    RngData *task_data;
    CairoxPoint *coordinates;

    oos_messages_free(gv);
    oos_components_free(gv);
    g_free(gv->task_data);

    gv->name = string_copy("RNG");
    gv->cycle = 0;
    gv->block = 0;
    gv->stopped = FALSE;

    oos_process_create(gv, "Task Setting", BOX_TASK_SETTING, 0.2, 0.1, task_setting_output);
    oos_process_create(gv, "Monitoring", BOX_MONITORING, 0.8, 0.1, monitoring_output);
    oos_process_create(gv, "Apply Set", BOX_APPLY_SET, 0.2, 0.7, apply_set_output);
    oos_process_create(gv, "Generate Response", BOX_GENERATE_RESPONSE, 0.8, 0.7, generate_response_output);
    oos_process_create(gv, "Propose Response", BOX_PROPOSE_RESPONSE, 0.5, 0.9, propose_response_output);

    oos_buffer_create(gv, "Current Set", BOX_CURRENT_SET, 0.2, 0.4, BUFFER_DECAY_NONE, 0, BUFFER_CAPACITY_UNLIMITED, 0, BUFFER_EXCESS_IGNORE, BUFFER_ACCESS_RANDOM);
    oos_buffer_create(gv, "Working Memory", BOX_WORKING_MEMORY, 0.8, 0.4, BUFFER_DECAY_LINEAR, pars->wm_decay_rate, BUFFER_CAPACITY_UNLIMITED, pars->wm_decay_rate, BUFFER_EXCESS_IGNORE, BUFFER_ACCESS_LIFO);
    oos_buffer_create(gv, "Response Buffer", BOX_RESPONSE_BUFFER, 0.5, 0.7, BUFFER_DECAY_NONE, 0, BUFFER_CAPACITY_LIMITED, 1, BUFFER_EXCESS_RANDOM, BUFFER_ACCESS_RANDOM);

    oos_buffer_create(gv, "Response Nodes", BOX_RESPONSE_NODES, 0.2, 0.9, BUFFER_DECAY_NONE, 0, BUFFER_CAPACITY_LIMITED, 10, BUFFER_EXCESS_RANDOM, BUFFER_ACCESS_RANDOM);

    // Task Setting sends to Current Set
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.19, 0.145);
    coordinate_list_set(coordinates, 2, 0.19, 0.355);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);
    // Task Setting reads from Current Set
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.20, 0.145);
    coordinate_list_set(coordinates, 2, 0.20, 0.355);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);
    // Task Setting reads from Working Memory
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.21, 0.145);
    coordinate_list_set(coordinates, 2, 0.21, 0.230);
    coordinate_list_set(coordinates, 3, 0.794, 0.230);
    coordinate_list_set(coordinates, 4, 0.794, 0.355);
    oos_arrow_create(gv, FALSE, AH_BLUNT, coordinates, 4, 1.0);

    // Monitoring sends to Current Set
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.794, 0.145);
    coordinate_list_set(coordinates, 2, 0.794, 0.270);
    coordinate_list_set(coordinates, 3, 0.21, 0.270);
    coordinate_list_set(coordinates, 4, 0.21, 0.355);
    oos_arrow_create(gv, FALSE, AH_SHARP, coordinates, 4, 1.0);
    // Monitoring sends to Response Nodes
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.794, 0.145);
    coordinate_list_set(coordinates, 2, 0.794, 0.270);
    coordinate_list_set(coordinates, 3, 0.495, 0.270);
    coordinate_list_set(coordinates, 4, 0.495, 0.655);
    oos_arrow_create(gv, FALSE, AH_SHARP, coordinates, 4, 1.0);
    // Monitoring reads from Response Nodes
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.806, 0.145);
    coordinate_list_set(coordinates, 2, 0.806, 0.285);
    coordinate_list_set(coordinates, 3, 0.505, 0.285);
    coordinate_list_set(coordinates, 4, 0.505, 0.655);
    oos_arrow_create(gv, FALSE, AH_BLUNT, coordinates, 4, 1.0);
    // Monitoring reads from Working Memory
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.806, 0.145);
    coordinate_list_set(coordinates, 2, 0.806, 0.355);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);

    // Apply Set reads from Current Set
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.20, 0.655);
    coordinate_list_set(coordinates, 2, 0.20, 0.445);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);
    // Apply Set reads from Working Memory
    coordinates = coordinate_list_create(4);
    coordinate_list_set(coordinates, 1, 0.20, 0.655);
    coordinate_list_set(coordinates, 2, 0.20, 0.55);
    coordinate_list_set(coordinates, 3, 0.794, 0.55);
    coordinate_list_set(coordinates, 4, 0.794, 0.445);
    oos_arrow_create(gv, FALSE, AH_BLUNT, coordinates, 4, 1.0);
    // Generate Response sends to Working Memory
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.806, 0.655);
    coordinate_list_set(coordinates, 2, 0.806, 0.445);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);
    // Apply Set reads from Response Buffer
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.265, 0.70);
    coordinate_list_set(coordinates, 2, 0.435, 0.70);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);
    // Apply Set seads to Response Nodes
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.20, 0.745);
    coordinate_list_set(coordinates, 2, 0.20, 0.855);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);

    // Generate Response sends to Response Buffer (0.735, 0.79 -> 0.565, 0.79)
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.735, 0.69);
    coordinate_list_set(coordinates, 2, 0.565, 0.69);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);
    // Generate Response reads from Response Buffer (0.735, 0.81 -> 0.565, 0.81)
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.735, 0.71);
    coordinate_list_set(coordinates, 2, 0.565, 0.71);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);
    // Generate Response sends to Output 
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.865, 0.70);
    coordinate_list_set(coordinates, 2, 0.965, 0.70);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);

    // Propose Response reads from Response Nodes
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.435, 0.90);
    coordinate_list_set(coordinates, 2, 0.265, 0.90);
    oos_arrow_create(gv, TRUE, AH_BLUNT, coordinates, 2, 1.0);
    // Propose Response sends to Response Buffer
    coordinates = coordinate_list_create(2);
    coordinate_list_set(coordinates, 1, 0.5, 0.855);
    coordinate_list_set(coordinates, 2, 0.5, 0.745);
    oos_arrow_create(gv, TRUE, AH_SHARP, coordinates, 2, 1.0);

    // Annotations: meta-component labels
    oos_annotation_create(gv, "Supervisory", 0.06, 0.25, 14, 0.0, TRUE);
    oos_annotation_create(gv, "System", 0.06, 0.28, 14, 0.0, TRUE);
    oos_annotation_create(gv, "Contention", 0.06, 0.74, 14, 0.0, TRUE);
    oos_annotation_create(gv, "Scheduling", 0.06, 0.77, 14, 0.0, TRUE);
    // Annotations: arrow labels
    oos_annotation_create(gv, "Switch", 0.420, 0.257, 12, 0.0, FALSE);
    oos_annotation_create(gv, "Inhibit", 0.485, 0.490, 12, 90.0, FALSE);
    oos_annotation_create(gv, "Update", 0.815, 0.540, 12, 90.0, FALSE);

    // Annotation: A dotted line between CS and SS (hackery!):
    int k;
    for (k = 0; k < 100; k++) {
        coordinates = coordinate_list_create(2);
        coordinate_list_set(coordinates, 1, 0.05 + 0.90 * k / 100.0, 0.61);
        coordinate_list_set(coordinates, 2, 0.05 + 0.90 * (k + 0.5) / 100.0, 0.61);
        oos_arrow_create(gv, TRUE, AH_NONE, coordinates, 2, 1.0);
    }

    if ((task_data = (RngData *)malloc(sizeof(RngData))) != NULL) {
	int i, j;
	for (i = 0; i < MAX_SUBJECTS; i++) {
	    task_data->trial[i].n = 0;
	    for (j = 0; j < MAX_TRIALS; j++) {
		task_data->trial[i].response[j] = -1;
	    }
	}
	task_data->group.n = 0;
	task_data->params.wm_decay_rate = pars->wm_decay_rate;
	task_data->params.temperature = pars->temperature;
	task_data->params.monitoring_efficiency = pars->monitoring_efficiency;
	task_data->params.wm_update_efficiency = pars->wm_update_efficiency;
	gv->task_data = (void *)task_data;
    }
    return(gv->task_data != NULL);
}

void rng_initialise_subject(OosVars *gv)
{
    int i;

    for (i = 0; i < 10; i++) {
	char element[64];
	g_snprintf(element, 64, "response(%d).", i);
	oos_buffer_create_element(gv, BOX_RESPONSE_NODES, element, 0.1);
    }
}

void rng_globals_destroy(RngData *task_data)
{
    g_free(task_data);
}

/******************************************************************************/

void rng_run(OosVars *gv)
{
    RngData *task_data;
    RngSubjectData *subject;
    FILE *fp = NULL;

    task_data = (RngData *)gv->task_data;
    oos_initialise_session(gv, 100, 36);
    while (gv->block < gv->subjects_per_experiment) {
	oos_initialise_trial(gv);
	rng_initialise_subject(gv);    
	while (oos_step(gv)) {
#ifdef DEBUG
	    oos_dump(gv, TRUE);
#endif
	}
        subject = &(task_data->trial[gv->block]);
	rng_analyse_subject_responses(fp, subject, gv->trials_per_subject);
        task_data->group.n = gv->block;
	oos_step_block(gv);
    }
#ifdef DEBUG
    fprint_schema_counts(fp, gv);
#endif

    if ((fp != NULL) && (fp != stdout)) {
        fclose(fp);
    }
}

/******************************************************************************/
