/* Scan the parameter space randomly saving the DVs after each run for later screening */

#include "rng.h"
#include "rng_defaults.h"
#include "lib_math.h"

#define LOG_FILE "FIT_SCAN.log"

/******************************************************************************/

static void skip_to_eoln(FILE *fp)
{
    int c;

    while (((c = getc(fp)) != EOF) && (c != '\n')) {
        c = getc(fp);
    }
    ungetc(c, fp);
}
    
/******************************************************************************/

static int read_data_line(FILE *fp, RngParameters *params, RngGroupData *results)
{
    return(fscanf(fp, "%d\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\n", &params->wm_decay_rate, &params->wm_update_efficiency, &params->selection_temperature, &params->switch_rate, &params->monitoring_efficiency, &results->mean.r1, &results->mean.rng, &results->mean.rr, &results->mean.aa, &results->mean.oa, &results->mean.tpi) == 11);
}

/******************************************************************************/

static void params_copy(RngParameters *params, RngParameters *p2)
{
    p2->wm_decay_rate = params->wm_decay_rate;
    p2->wm_update_efficiency = params->wm_update_efficiency;
    p2->selection_temperature = params->selection_temperature;
    p2->switch_rate = params->switch_rate;
    p2->monitoring_method = params->monitoring_method;
    p2->monitoring_efficiency = params->monitoring_efficiency;
    p2->individual_variability = params->individual_variability;
    p2->sample_size = params->sample_size;
}

static void fprint_params(FILE *fp, RngParameters *p2)
{
    fprintf(fp, "D: %d; U: %5.3f; T: %5.3f; S: %5.3f; M: %5.3f\n",  p2->wm_decay_rate, p2->wm_update_efficiency, p2->selection_temperature, p2->switch_rate, p2->monitoring_efficiency);
}

static void extract_best_fit(char *label, RngGroupData *data)
{
    FILE *fp;

    /* Open the log file and skip the headings on the first line: */
    if ((fp = fopen(LOG_FILE, "r")) == NULL) {
        fprintf(stdout, "Open failed: %s is not readable\n", LOG_FILE);
    }
    else {
        double f = DBL_MAX, f2;
        RngParameters params, p2;
        RngGroupData results;
    
        skip_to_eoln(fp);
        while (read_data_line(fp, &params, &results)) {
            f2 = rng_data_calculate_fit(&results, data);
            if (f2 < f) {
                params_copy(&params, &p2);
                f = f2;
            }
        }
        fclose(fp);
        fprintf(stdout, "Best fit to %s condition is %5.3f with ", label, f);
        fprint_params(stdout, &p2);
    }
}

int main(int argc, char **argv)
{

    extract_best_fit("Control", &subject_ctl);
    extract_best_fit("DS", &subject_ds);
    extract_best_fit("2B", &subject_2b);
    extract_best_fit("GnG", &subject_gng);

    exit(1);
}

/******************************************************************************/
