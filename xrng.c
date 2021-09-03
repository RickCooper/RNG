/*******************************************************************************

    File:     xrng.c
    Author:   Rick Cooper
    Created:  22/06/12
    Edited:   22/06/12
    Contents: An X interface to the RNG model

    Public procedures:
        int main(int argc, char **argv)

*******************************************************************************/

#include "xrng.h"
#include "rng_defaults.h"

/******************************************************************************/
/* Section 1: Main -----------------------------------------------------------*/

int main(int argc, char **argv)
{
    OosVars *gv;
    XGlobals xg = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRUE, TRUE, FALSE, CANVAS_GLOBAL, CANVAS_VARY_DECAY, {}, -1, {}, {}};

    gtk_set_locale();
    gtk_init(&argc, &argv);

    xg.params.wm_decay_rate = pars.wm_decay_rate;
    xg.params.wm_update_efficiency = pars.wm_update_efficiency;
    xg.params.selection_temperature = pars.selection_temperature;
    xg.params.switch_rate = pars.switch_rate;
    xg.params.monitoring_method = pars.monitoring_method;
    xg.params.monitoring_efficiency = pars.monitoring_efficiency;
    xg.params.individual_variability = pars.individual_variability;
    xg.params.sample_size = pars.sample_size;
    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else if (!rng_create(gv, &(xg.params))) {
        fprintf(stdout, "ABORTING: Cannot create RNG\n");
    }
    else if (!rng_widgets_create(&xg, gv)) {
        fprintf(stderr, "%s: Aborting - Widget creation failed\n", argv[0]);
    }
    else {
        rng_initialise_subject(gv);
        gtk_widget_show(xg.window);
        gtk_main();
    }

    rng_globals_destroy((RngData *)gv->task_data);
    oos_globals_destroy(gv);

    exit(0);
}

/******************************************************************************/
