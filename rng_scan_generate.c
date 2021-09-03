/* Scan the parameter space randomly saving the DVs after each run for later screening */

#include "rng.h"
#include "rng_defaults.h"
#include "lib_math.h"

// GENERATION_MAX is only used in the for loop as a termination condition:
#define GENERATION_MAX  10000

#define LOG_FILE "FIT_SCAN.log"

/******************************************************************************/

static void rng_run_and_analyse(OosVars *gv, RngParameters *pars)
{
    rng_create(gv, pars);
    rng_initialise_subject(gv);
    rng_run(gv);
    rng_analyse_group_data((RngData *)gv->task_data);
}

static void parameters_sample(RngParameters *seed)
{
    /* Default values: */
    seed->wm_decay_rate = (int) random_uniform(1, 20);
    seed->wm_update_efficiency = random_uniform(0.0, 1.0);
    seed->selection_temperature = random_uniform(0.0, 2.0);
    seed->switch_rate = random_uniform(0.0, 1.0);
    seed->monitoring_method = pars.monitoring_method;
    seed->monitoring_efficiency = random_uniform(0.0, 1.0);
    seed->individual_variability = pars.individual_variability;
    seed->sample_size = pars.sample_size * 10;
}

static void results_save(OosVars *gv, RngParameters *pars)
{
    FILE *fp;
    RngData *data = (RngData *)gv->task_data;

    if ((fp = fopen(LOG_FILE, "a")) != NULL) {
        /* Parameters / IVS: */
        fprintf(fp, "%4d\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t", pars->wm_decay_rate, pars->wm_update_efficiency, pars->selection_temperature, pars->switch_rate, pars->monitoring_efficiency);
        /* Results / DVS: */
        fprintf(fp, "%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\n", data->group.mean.r1, data->group.mean.rng, data->group.mean.rr, data->group.mean.aa, data->group.mean.oa, data->group.mean.tpi);
        fclose(fp);
    }
}

/******************************************************************************/

int main(int argc, char **argv)
{
    OosVars *gv;
    RngParameters pars;
    FILE *fp;
    int generation;

    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else {

        /* Create the log file with headings on the first line: */
        if ((fp = fopen(LOG_FILE, "r")) == NULL) {
            fp = fopen(LOG_FILE, "w");
            /* Parameters / IVS: */
            fprintf(fp, "WMD\tWMU\tTemp\tSwR\tME\t");
            /* Results / DVS: */
            fprintf(fp, "R\tRNG\tRR\tAA\tOA\tTPI\n");
        }
        fclose(fp);

        for (generation = 0; generation < GENERATION_MAX; generation++) {
            parameters_sample(&pars);
            rng_run_and_analyse(gv, &pars);
            results_save(gv, &pars);
            fprintf(stdout, "."); fflush(stdout);
	}
        fprintf(stdout, "\n"); fflush(stdout);

        rng_globals_destroy((RngData *)gv->task_data);
        oos_globals_destroy(gv);
    }
    exit(1);
}

/******************************************************************************/
