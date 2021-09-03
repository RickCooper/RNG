/* Use gradient descent to find best fitting parameters */

#include "rng.h"
#include "rng_defaults.h"
#include "lib_math.h"

// GENERATION_MAX is only used in the for loop as a termination condition:
#define GENERATION_MAX  100
// The population comprises the previous best fitting point, plus the points
// immediately surrounding it in parameter space. With a 1-d parameter space we
// would have three points. With a 2-d parameter space we would have 9 points.
// Since we have a 5-d parameter space we have 3^5 = 243 points
//#define POPULATION_SIZE 243
// Or, if we fix one parameter, 3^4 = 81
#define POPULATION_SIZE 81

static RngParameters para_pop[POPULATION_SIZE];
static double para_fit[POPULATION_SIZE];

// Select one log file and the corresponding data file:

// #define LOG_FILE "FIT_GD_CTRL.log"
// #define LOG_FILE "FIT_GD_DS.log"
#define LOG_FILE "FIT_GD_2B.log"
//#define LOG_FILE "FIT_GD_GNG.log"

// static RngGroupData *SUBJECT_DATA = &subject_ctl;
// static RngGroupData *SUBJECT_DATA = &subject_ds;
static RngGroupData *SUBJECT_DATA = &subject_2b;
//static RngGroupData *SUBJECT_DATA = &subject_gng;

/******************************************************************************/

double clip(double low, double high, double x)
{
    if (x < low) {
        return(low);
    }
    else if (x > high) {
        return(high);
    }
    else {
        return(x);
    }
}

/******************************************************************************/

static void gd_initialise_parameters(RngParameters *seed)
{
    /* Default values: */
    seed->wm_decay_rate = pars.wm_decay_rate;
    seed->wm_update_efficiency = pars.wm_update_efficiency;
    seed->selection_temperature = pars.selection_temperature;
    seed->switch_rate = pars.switch_rate;
    seed->monitoring_method = pars.monitoring_method;
    seed->monitoring_efficiency = pars.monitoring_efficiency;
    seed->individual_variability = pars.individual_variability;
    seed->sample_size = pars.sample_size * 10;    /* Sample 360 times to get stable measures: */
}

static void gd_generate_population(RngParameters *seed)
{
    int p0, p1, p2, p3, p4;
    int i = 0;

    double wm_decay_rate_step = 2;
    double wm_update_efficiency_step = 0.05;
    double selection_temperature_step = 0.02;
    double switch_rate_step = 0.05;
    double monitoring_efficiency_step = 0.05;

//    for (p0 = -1; p0 < 2; p0++) {
    for (p0 = 0; p0 < 1; p0++) { // WM Decay is fixed at default
        for (p1 = -1; p1 < 2; p1++) {
            for (p2 = -1; p2 < 2; p2++) {
                for (p3 = -1; p3 < 2; p3++) {
                    for (p4 = -1; p4 < 2; p4++) {
                        para_pop[i].wm_decay_rate = clip(0, 100, seed->wm_decay_rate + p0*wm_decay_rate_step);
                        para_pop[i].wm_update_efficiency = clip(0.0, 1.0, seed->wm_update_efficiency + p1*wm_update_efficiency_step);
                        para_pop[i].selection_temperature = clip(0.0, 2.0, seed->selection_temperature + p2*selection_temperature_step);
                        para_pop[i].switch_rate = clip(0.0, 1.0, seed->switch_rate + p3*switch_rate_step);
                        para_pop[i].monitoring_method = seed->monitoring_method;
                        para_pop[i].monitoring_efficiency = clip(0.0, 1.0, seed->monitoring_efficiency + p4*monitoring_efficiency_step);
                        para_pop[i].individual_variability = seed->individual_variability;
                        para_pop[i].sample_size = seed->sample_size;
                        i++;
                    }
                }
            }
        }
    }
}

/******************************************************************************/

static int gd_get_best_fit(int n)
{
    /* Return the index of the best fitting parameters */

    int i, j;

    i = 0;
    for (j = 1; j < n; j++) {
        if (para_fit[j] < para_fit[i]) {
            i = j;
        }
    }
    return(i);
}

static void population_print_statistics(int generation, int i)
{
    FILE *fp;

    fprintf(stdout, "%4d: %f [DR: %3d; SR: %4.2f; ST: %4.2f; ME: %4.2f; UE: %4.2f]\n", generation, para_fit[i], para_pop[i].wm_decay_rate, para_pop[i].switch_rate, para_pop[i].selection_temperature, para_pop[i].monitoring_efficiency, para_pop[i].wm_update_efficiency);

    fp = fopen(LOG_FILE, "a");
    if (fp != NULL) {
        fprintf(fp, "%4d: %f [DR: %3d; SR: %4.2f; ST: %4.2f; ME: %4.2f; UE: %4.2f]\n", generation, para_fit[i], para_pop[i].wm_decay_rate, para_pop[i].switch_rate, para_pop[i].selection_temperature, para_pop[i].monitoring_efficiency, para_pop[i].wm_update_efficiency);
        fclose(fp);
    }
}

static void gd_copy_parameters(RngParameters *seed, int i)
{
    seed->wm_decay_rate = para_pop[i].wm_decay_rate;
    seed->wm_update_efficiency = para_pop[i].wm_update_efficiency;
    seed->selection_temperature = para_pop[i].selection_temperature;
    seed->switch_rate = para_pop[i].switch_rate;
    seed->monitoring_method = para_pop[i].monitoring_method;
    seed->monitoring_efficiency = para_pop[i].monitoring_efficiency;
    seed->individual_variability = para_pop[i].individual_variability;
    seed->sample_size = para_pop[i].sample_size;
}

/******************************************************************************/

static double rng_model_fit(OosVars *gv, RngParameters *pars)
{
    rng_create(gv, pars);
    rng_initialise_subject(gv);
    rng_run(gv);
    rng_analyse_group_data((RngData *)gv->task_data);
    return(rng_data_calculate_fit(&((RngData *)gv->task_data)->group, SUBJECT_DATA));
}

/******************************************************************************/

int main(int argc, char **argv)
{
    OosVars *gv;
    RngParameters seed;
    FILE *fp;
    int generation = 0;
    int i;

    fprintf(stdout, "Running Gradient Descent; Output to %s\n", LOG_FILE);

    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else {
        gd_initialise_parameters(&seed);
        fp = fopen(LOG_FILE, "w"); fclose(fp);
        for (generation = 0; generation < GENERATION_MAX; generation++) {
            gd_generate_population(&seed);
            for (i = 0; i < POPULATION_SIZE; i++) {
                para_fit[i] = rng_model_fit(gv, &para_pop[i]);
fprintf(stdout, "%1d", i % 10); fflush(stdout);
            }
fprintf(stdout, "\n");
            i = gd_get_best_fit(POPULATION_SIZE);

            /* Append this generation's results to the log file: */
            population_print_statistics(generation, i);
            gd_copy_parameters(&seed, i);
	}

        rng_globals_destroy((RngData *)gv->task_data);
        oos_globals_destroy(gv);
    }
    exit(1);
}

/******************************************************************************/
