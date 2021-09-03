#ifndef _rng_h_

#define _rng_h_

#include <stdio.h>
#include <math.h>
#include <glib.h>
#include "lib_string.h"
#include "oos.h"

#undef DEBUG

#define RESPONSE_SET_SIZE 10
#define MAX_SUBJECTS 360
#define MAX_TRIALS  500
#define SCHEMA_SET_SIZE 11

typedef struct rng_scores {
    double r1;
    double r2;
    double r3;
    double rng;
    double rng2;
    double rr;
    double aa;
    double oa;
    double tpi;
    double rg1;
    double rg2;
    double associates[RESPONSE_SET_SIZE];
} RngScores;

typedef struct rng_group_data {
    RngScores mean;
    RngScores sd;
    int n;
} RngGroupData;

typedef struct rng_parameters {
    int    wm_decay_rate;
    double wm_update_efficiency;
    double selection_temperature;
    double switch_rate;
    int    monitoring_method;
    double monitoring_efficiency;
    double individual_variability;
    int    sample_size;
} RngParameters;

typedef struct rng_subject_data {
    int n;
    int response[MAX_TRIALS];
    RngScores scores;
} RngSubjectData;

typedef struct rng_data {
    RngParameters  params;
    RngSubjectData subject[MAX_SUBJECTS];
    RngGroupData   group;
    double         strengths[SCHEMA_SET_SIZE];
} RngData;

extern RngGroupData subject_ctl, subject_ds, subject_2b, subject_gng;

extern void rng_analyse_group_data(RngData *task_data);
extern void rng_print_group_data_analysis(FILE *fp, RngData *task_data);
extern void rng_print_scores(FILE *fp, RngScores *scores);
extern void rng_print_subject_sequence(FILE *fp, RngSubjectData *subject);
extern void rng_analyse_subject_responses(FILE *fp, RngSubjectData *subject, int num_trials);
extern Boolean rng_create(OosVars *gv, RngParameters *pars);
extern void rng_initialise_subject(OosVars *gv);
extern void rng_globals_destroy(RngData *task_data);
extern void rng_run(OosVars *gv);
extern void rng_scores_convert_to_z(RngGroupData *raw_data, RngGroupData *baseline, RngGroupData *z_scores);
extern double rng_data_calculate_fit(RngGroupData *data, RngGroupData *model);

#endif
