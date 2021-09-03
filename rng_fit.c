/* Use a genetic algorithm to find best fitting parameters */

#include "rng.h"
#include "rng_defaults.h"
#include "lib_math.h"

#define GENERATION_MAX  100
#define POPULATION_SIZE 60

static RngParameters para_pop[POPULATION_SIZE];
static double para_fit[POPULATION_SIZE];
static int generation = 0;

#define MIN_WM_DECAY  1
#define MAX_WM_DECAY 40
#define MIN_TEMP 0.0
#define MAX_TEMP 2.0

#define LOG_FILE "FIT.log"

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

static void generate_new_individual_within_range(int i)
{
    para_pop[i].wm_decay_rate = random_integer(MIN_WM_DECAY, MAX_WM_DECAY);
    para_pop[i].selection_temperature = random_uniform(MIN_TEMP, MAX_TEMP);
    para_pop[i].wm_update_efficiency = random_uniform(0.1, 1.0);
    para_pop[i].switch_rate = random_uniform(0.1, 1.0);
    para_pop[i].monitoring_method = pars.monitoring_method; // Default
    para_pop[i].monitoring_efficiency = random_uniform(0.1, 1.0);
    para_pop[i].individual_variability = 0.5; // Default
    para_pop[i].sample_size = 36;
}

static void ga_generate_seed_population()
{
    int i;

    for (i = 0; i < POPULATION_SIZE; i++) {
        generate_new_individual_within_range(i);
    }
}

static int parameters_match(int i, int j)
{
    if (para_pop[i].wm_decay_rate != para_pop[j].wm_decay_rate) {
        return(0);
    }
    else if (para_pop[i].monitoring_efficiency != para_pop[j].monitoring_efficiency) {
        return(0);
    }
    else if (para_pop[i].switch_rate != para_pop[j].switch_rate) {
        return(0);
    }
    else if (para_pop[i].selection_temperature != para_pop[j].selection_temperature) {
        return(0);
    }
    else if (para_pop[i].wm_update_efficiency != para_pop[j].wm_update_efficiency) {
        return(0);
    }
    else {
        return(1);
    }
}

static void ga_generate_next_generation()
{
// Take best 25% of current population
// cross these to get next 25%
// add 25% mutatations from first group
// finish with 25% totally new

    int i, j;
    int l[3];

    l[0] = POPULATION_SIZE * 0.25;
    l[1] = POPULATION_SIZE * 0.50;
    l[2] = POPULATION_SIZE * 0.75;

    for (i = l[0]; i < l[1]; i++) {
        // 25% from originals crossed
        int m = random_integer(0, l[0]);
        int n = random_integer(0, l[0]);

        para_pop[i].wm_decay_rate = para_pop[m].wm_decay_rate;
        para_pop[i].switch_rate = para_pop[n].switch_rate;
        para_pop[i].monitoring_efficiency = para_pop[m].monitoring_efficiency;
        para_pop[i].wm_update_efficiency = para_pop[n].wm_update_efficiency;
        para_pop[i].selection_temperature = para_pop[m].selection_temperature;
        para_pop[i].individual_variability = para_pop[n].individual_variability;
        para_pop[i].sample_size = para_pop[n].sample_size;
    }

    for (i = l[1]; i < l[2]; i++) {
        // 25% mutatations from original good cases
        para_pop[i].wm_decay_rate = clip(MIN_WM_DECAY, MAX_WM_DECAY, random_normal(para_pop[i-l[1]].wm_decay_rate, 5.0));
        para_pop[i].switch_rate = clip(0.0, 1.0, random_normal(para_pop[i-l[1]].switch_rate, 0.2));
        para_pop[i].monitoring_efficiency = clip(0.0, 1.0, random_normal(para_pop[i-l[1]].monitoring_efficiency, 0.2));
        para_pop[i].wm_update_efficiency = clip(0.0, 1.0, random_normal(para_pop[i-l[1]].wm_update_efficiency, 0.2));
        para_pop[i].selection_temperature = clip(0.0, 1.0, random_normal(para_pop[i-l[1]].selection_temperature, 0.2));
        para_pop[i].individual_variability = para_pop[0].individual_variability;
        para_pop[i].sample_size = para_pop[0].sample_size;
    }

    for (i = l[2]; i < POPULATION_SIZE; i++) {
        generate_new_individual_within_range(i);
    }

    /* Now run through the full set of individuals and mutate duplicates: */
    for (i = 0; i < POPULATION_SIZE; i++) {
        for (j = i+1; j < POPULATION_SIZE; j++) {
            if (parameters_match(i, j)) {
                para_pop[j].wm_decay_rate = clip(MIN_WM_DECAY, MAX_WM_DECAY, random_normal(para_pop[j].wm_decay_rate, 5.0));
                para_pop[j].switch_rate = clip(0.0, 1.0, random_normal(para_pop[j].switch_rate, 0.1));
                para_pop[j].monitoring_efficiency = clip(0.0, 1.0, random_normal(para_pop[j].monitoring_efficiency, 0.1));
                para_pop[j].wm_update_efficiency = clip(0.0, 1.0, random_normal(para_pop[j].wm_update_efficiency, 0.1));
                para_pop[j].selection_temperature = clip(0.0, 1.0, random_normal(para_pop[j].selection_temperature, 0.1));
            }
        }
    }
}

/******************************************************************************/

static void ga_generate_population(int generation)
{
    if (generation == 0) {
        ga_generate_seed_population();
    }
    else {
        ga_generate_next_generation();
    }
}

/******************************************************************************/

static void ga_sort_by_fit()
{
    /* A quick to code but dirty ripple sort procedure... */

    int i, j;
    double f;
    RngParameters p;

    for (i = 0; i < POPULATION_SIZE; i++) {
        for (j = i+1; j < POPULATION_SIZE; j++) {
            if (para_fit[i] > para_fit[j]) {
                /* Swap i and j: */
            
                f = para_fit[i];
                para_fit[i] = para_fit[j];
                para_fit[j] = f;

                p.wm_decay_rate = para_pop[i].wm_decay_rate;
                para_pop[i].wm_decay_rate = para_pop[j].wm_decay_rate;
                para_pop[j].wm_decay_rate = p.wm_decay_rate;

                p.switch_rate = para_pop[i].switch_rate;
                para_pop[i].switch_rate = para_pop[j].switch_rate;
                para_pop[j].switch_rate = p.switch_rate;

                p.monitoring_efficiency = para_pop[i].monitoring_efficiency;
                para_pop[i].monitoring_efficiency = para_pop[j].monitoring_efficiency;
                para_pop[j].monitoring_efficiency = p.monitoring_efficiency;

                p.wm_update_efficiency = para_pop[i].wm_update_efficiency;
                para_pop[i].wm_update_efficiency = para_pop[j].wm_update_efficiency;
                para_pop[j].wm_update_efficiency = p.wm_update_efficiency;

                p.selection_temperature = para_pop[i].selection_temperature;
                para_pop[i].selection_temperature = para_pop[j].selection_temperature;
                para_pop[j].selection_temperature = p.selection_temperature;
            }
        }
    }
}

static void ga_print_statistics(int generation)
{
    FILE *fp;
    int i;

    fprintf(stdout, "%4d: %f [DR: %3d; SE: %4.2f; ST: %4.2f; ME: %4.2f; UE: %4.2f]\n", generation, para_fit[0], para_pop[0].wm_decay_rate, para_pop[0].switch_rate, para_pop[0].selection_temperature, para_pop[0].monitoring_efficiency, para_pop[0].wm_update_efficiency);

    fp = fopen(LOG_FILE, "a");
    if (fp != NULL) {
// Print the best 3 individuals:
        for (i = 0; i < 3; i++) {
            fprintf(fp, "%4d %3d: %f", generation, i, para_fit[i]);
            fprintf(fp, " [Decay: %4d, Switch: %5.3f, Monitoring: %5.3f, Updating: %6.3f]\n", para_pop[i].wm_decay_rate, para_pop[i].switch_rate, para_pop[i].monitoring_efficiency, para_pop[i].wm_update_efficiency);
	}
// Print all individuals
//        fprintf(fp, "Generation %3d [%s] [Decay; Temperature; Monitoring; Updating]: %f\n", generation, RT_FLAGS, para_fit[0]);
//        for (i = 0; i < POPULATION_SIZE; i++) {
//            fprintf(fp, "%3d: %f [%4d, %5.3f, %5.3f, %6.3f]\n", i, para_fit[i], para_pop[i].wm_decay_rate,ppara_pop[i].switch_rate,  ara_pop[i].monitoring_efficiency, para_pop[i].wm_update_efficiency);
//        }
        fclose(fp);
    }
}

/******************************************************************************/

static double rng_model_fit(OosVars *gv, RngParameters *pars)
{
    rng_create(gv, pars);
    rng_initialise_subject(gv);
    rng_run(gv);
    rng_analyse_group_data((RngData *)gv->task_data);
    return(rng_data_calculate_fit(&((RngData *)gv->task_data)->group, &subject_ds));
}

/******************************************************************************/

int main(int argc, char **argv)
{
    OosVars *gv;
    FILE *fp;
    int i;

    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else {
        fp = fopen(LOG_FILE, "w"); fclose(fp);
        for (generation = 0; generation < GENERATION_MAX; generation++) {
            ga_generate_population(generation);
            for (i = 0; i < POPULATION_SIZE; i++) {
                para_fit[i] = rng_model_fit(gv, &para_pop[i]);

//                fprintf(stdout, "%4d %3d: %f", generation, i, para_fit[i]);
//                fprintf(stdout, " [Decay: %4d, Monitoring: %5.3f, Updating: %6.3f]\n", para_pop[i].wm_decay_rate, para_pop[i].monitoring_efficiency, para_pop[i].wm_update_efficiency);
            }
            ga_sort_by_fit();

            /* Append this generation's results to the log file: */
            ga_print_statistics(generation);
	}

        rng_globals_destroy((RngData *)gv->task_data);
        oos_globals_destroy(gv);
    }
    exit(1);
}

/******************************************************************************/
