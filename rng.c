
#include "rng.h"
#include "rng_defaults.h"

/******************************************************************************/

int main(int argc, char **argv)
{
    OosVars *gv;

    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else {
        rng_create(gv, &pars);
        rng_initialise_subject(gv);
        rng_run(gv);
        rng_analyse_group_data((RngData *)gv->task_data);
        rng_print_group_data_analysis(stdout, (RngData *)gv->task_data);
        rng_globals_destroy((RngData *)gv->task_data);
        oos_globals_destroy(gv);
    }
    exit(1);
}

/******************************************************************************/
