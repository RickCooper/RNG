
#include "rng.h"

RngGroupData subject_ctl = {{0.96236, 45.63603, 84.88786, 0.30043, 0.21936, 0.01361, 0.25861, 0.13139, 0.73342, 9.71256, 8.80556, {0.01361, 0.14778, 0.11639, 0.10556, 0.11333, 0.13139, 0.09056, 0.08778, 0.08278, 0.11083}},
                            {0.56746,  4.15996,  8.17690, 0.06793, 0.12009, 0.02380, 0.14275, 0.07043, 0.19622, 0.17397, 0.68949, {0.02380, 0.11053, 0.06316, 0.04693, 0.06370, 0.07043, 0.05171, 0.02587, 0.04425, 0.07338}},
                            36};
RngGroupData subject_ds =  {{2.04830, 42.68736, 75.92826, 0.40954, 0.35190, 0.00417, 0.32814, 0.13619, 0.60445, 9.53367, 8.31944, {0.00417, 0.18187, 0.08231, 0.08922, 0.11897, 0.13619, 0.09618, 0.06919, 0.07563, 0.14628}},
                            {1.91225,  4.47866, 11.38800, 0.15078, 0.11178, 0.00967, 0.25167, 0.08499, 0.24919, 0.43896, 0.92700, {0.00967, 0.17605, 0.05434, 0.05611, 0.08588, 0.08499, 0.05821, 0.03894, 0.04510, 0.19695}},
                            36};
RngGroupData subject_2b =  {{1.97855, 42.89944, 70.52411, 0.46126, 0.44508, 0.00167, 0.42389, 0.09667, 0.44572, 9.56080, 8.20833, {0.00167, 0.25778, 0.09972, 0.08389, 0.08806, 0.09667, 0.06917, 0.06944, 0.06750, 0.16611}},
                            {1.61988,  3.76967, 12.05060, 0.16597, 0.22423, 0.00378, 0.27947, 0.10187, 0.26863, 0.30339, 1.00267, {0.00378, 0.22231, 0.06934, 0.06147, 0.07855, 0.10187, 0.05729, 0.06197, 0.06767, 0.21381}},
                            36};
RngGroupData subject_gng = {{1.19562, 43.56381, 76.99210, 0.38757, 0.33856, 0.00529, 0.33448, 0.13035, 0.58248, 9.63612, 8.72222, {0.00529, 0.20196, 0.08504, 0.08838, 0.10119, 0.13035, 0.09321, 0.07950, 0.08257, 0.13252}},
                            {1.01681,  3.92845, 10.88506, 0.15775, 0.20874, 0.01444, 0.25996, 0.09619, 0.26543, 0.29080, 0.79682, {0.01444, 0.21106, 0.05855, 0.06405, 0.08283, 0.09619, 0.05605, 0.04966, 0.05453, 0.13862}},
                            36};

/*----------------------------------------------------------------------------*/

double calculate_median(int list[], int l)
{
    int j, k, tmp;

    // Ripple sort the list:

    for (j = 0; j < l; j++) {
        for (k = j+1; k < l; k++) {
            if (list[j] > list[k]) {
                tmp = list[j];
                list[j] = list[k];
                list[k] = tmp;
            }
        }
    }

    if ((l % 2) == 1) {
        /* l is odd */
        return(list[(l - 1) / 2]);
    }
    else {
        /* l is even */
        return((list[l / 2] + list[l / 2 - 1]) * 0.5);
    }
}

double calculate_mean(int list[], int i)
{
    int sum = 0, n;

    for (n = 0; n < i; n++) {
        sum += list[n];
    }
    return(sum / (double) i);
}

static void rng_score_subject_data(RngSubjectData *subject, int trials)
{
    /* Read a data file and return the various RNG dvs. */
    /* Also return misses (times participant failed to generate a response) */
    /* and hits (responses generated, excluding misses). */

    int f[RESPONSE_SET_SIZE], ff[RESPONSE_SET_SIZE][RESPONSE_SET_SIZE], last[RESPONSE_SET_SIZE];
    int rg_list[MAX_TRIALS];
    double rt, rt_sum, sum, h_s, h_m;
    double num, denom;
    long v1, v2, fr, k;
    int tp_direction, tp_count, n, d, i, j;
    int hits = 0, misses = 0;

    for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
        f[v1] = 0;
        last[v1] = -1;
        for (v2 = 0; v2 < RESPONSE_SET_SIZE; v2++) {
            ff[v1][v2] = 0;
        }
    }

    /* Score the data: */

    v1 = -1; fr = -1; tp_direction = 0; n = 0; i = 0; tp_count = 0; rt_sum = 0.0;

    for (j = 0; j < trials; j++) {
        v2 = subject->response[j];
        rt = 0.0; // TO DO: Handle RT?

        rt_sum += rt;
        /* Record the first valid response, for wrap-around: */
        if ((v2 >= 0) && (fr == -1)) {
            fr = v2;
        }

        /* Score hits and misses, and count frequencies for hits: */
        if (v2 < 0) {
            misses++;
        }
        else {
            hits++;
            f[v2]++;
        }

        /* Count frequencies for pairs: */
        if ((v1 >= 0) && (v2 >= 0)) {
            ff[v1][v2]++;
        }

        /* Score turning points: */
        d = (v2 < v1) ? (v2 + RESPONSE_SET_SIZE - v1) : (v2 - v1);
        if ((v1 >= 0) && (d != 0)) {
            if ((tp_direction == 0) && (d != 5)) {
                /* First response ... just set the initial direction: */
                tp_direction = (d > 5) ? -1 : 1;
            }
            else if ((tp_direction > 0) && (d > 5)) {
                /* Sequence was going clockwise, but is no longer: */
                tp_direction = -1;
                tp_count++;
            }
            else if ((tp_direction < 0) && (d < 5)) {
                /* Sequence was going counter-clockwise, but is no longer: */
                tp_direction = +1;
                tp_count++;
            }
        }

        if (v2 >= 0) {
            /* If v2 is -1 we ignore this in ff; */
            v1 = v2;
        }

        if ((v2 >= 0) && (i < trials)) {
            if (last[v2] == -1) {
                last[v2] = n;
            }
            else {
                rg_list[i++] = n - last[v2];
                last[v2] = n;
            }
        }

        n++;
    }

    /* Add in the wrap-around (as per Towse, but contra Baddeley): */
    if ((fr >= 0) && (v2 >= 0)) {
        ff[v2][fr]++;
    }

    /* Calculation of R1: */
    sum = 0.0;
    for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
        if (f[v1] > 1) {
            sum += f[v1] * log(f[v1]) / log(2.0);
        }
    }
    h_s = log((double) hits) / log(2.0) - (double) sum / (double) hits;
    h_m = log((double) RESPONSE_SET_SIZE) / log(2.0);

    subject->scores.r1 = 100.0 * (1 - h_s / h_m);

    /* Calculation of R2: */
    h_s = 0.0;
    h_m = 0.0;
    for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
        for (v2 = 0; v2 < RESPONSE_SET_SIZE; v2++) {
            if (ff[v1][v2] > 1) {
                h_s -= ff[v1][v2]/(double)hits * log(ff[v1][v2]/(double)hits) / log(2.0);
            }
        }
        if (f[v1] > 1) {
            h_m -= f[v1]/(double)hits * log(f[v1]/(double)hits) / log(2.0);
        }
    }

    subject->scores.r2 = 100.0 * (1 - h_s / (2.0*h_m));

    /* Calculation of RNG: */
    num = 0.0;
    denom = 0.0;

    for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
        for (v2 = 0; v2 < RESPONSE_SET_SIZE; v2++) {
            if (ff[v1][v2] > 1) {
                num = num + ff[v1][v2] * log(ff[v1][v2]) / log(2.0);
            }
        }
        if (f[v1] > 1) {
            denom = denom + f[v1] * log(f[v1]) / log(2.0);
        }
    }
    subject->scores.rng = num / denom;

    /* Calculation of associates (RR, OA, AA, etc.): */

    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        num = 0.0;
        denom = 0.0;
        for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
            num = num + ff[v1][(v1 + k) % RESPONSE_SET_SIZE];
            denom = denom + f[v1];
        }
        subject->scores.associates[k] = num / denom;
    }

    /* Calculation of RR: */
    subject->scores.rr = subject->scores.associates[0];

    /* Calculation of AA: */
    subject->scores.aa = subject->scores.associates[1] + subject->scores.associates[9];

    /* Calculation of OA: */
    subject->scores.oa = subject->scores.associates[5];

    /* Calculation of TPI: */
    subject->scores.tpi = tp_count / (0.4 * (n - 2.0));

    /* Calculate RG (repetition gap): */
    subject->scores.rg1 = calculate_mean(rg_list, i);
    subject->scores.rg2 = calculate_median(rg_list, i);
}

/*----------------------------------------------------------------------------*/

void rng_print_subject_sequence(FILE *fp, RngSubjectData *subject)
{
    if (fp != NULL) {
        int i;

        fprintf(fp, "SEQUENCE: ");
        for (i = 0; i < subject->n; i++) {
            fprintf(fp, "%d ", subject->response[i]);
        }
        fprintf(fp, "\n");
    }
}

static void fprint_rng_scores(FILE *fp, RngScores *score)
{
    if (fp != NULL) {
        int k;

        fprintf(fp, "%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f", score->r1, score->r2, score->rng, score->rr, score->aa, score->oa, score->tpi, score->rg1, score->rg2);
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            fprintf(fp, "\t%5.3f", score->associates[k]);
        }
        fprintf(fp, "\n");
    }
}

void rng_print_scores(FILE *fp, RngScores *scores)
{
    if (fp != NULL) {
        fprintf(fp, "  R1\t R2\t RNG\t  RR\t  AA\t   OA\t   TPI\t  RG1\t  RG2\tAssociates\n");
        fprint_rng_scores(fp, scores);
        fprintf(fp, "\n");
    }
}

void rng_analyse_subject_responses(FILE *fp, RngSubjectData *subject, int num_trials)
{
    rng_print_subject_sequence(fp, subject);
    rng_score_subject_data(subject, num_trials);
    rng_print_scores(fp, &(subject->scores));
}

void rng_analyse_group_data(RngData *task_data)
{
    RngScores sum, ssq;
    int i, k;

    sum.r1 = 0.0;
    sum.r2 = 0.0;
    sum.rng = 0.0;
    sum.rr = 0.0;
    sum.aa = 0.0;
    sum.oa = 0.0;
    sum.tpi = 0.0;
    sum.rg1 = 0.0;
    sum.rg2 = 0.0;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        sum.associates[k] = 0.0;
    }

    ssq.r1 = 0.0;
    ssq.r2 = 0.0;
    ssq.rng = 0.0;
    ssq.rr = 0.0;
    ssq.aa = 0.0;
    ssq.oa = 0.0;
    ssq.tpi = 0.0;
    ssq.rg1 = 0.0;
    ssq.rg2 = 0.0;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        ssq.associates[k] = 0.0;
    }

// fprintf(stdout, "PID\tR1\tRR\tAA\tOA\n");
    for (i = 0; i < task_data->group.n; i++) {
        RngSubjectData *subject = &(task_data->subject[i]);

        sum.r1 += subject->scores.r1;
        ssq.r1 += (subject->scores.r1*subject->scores.r1);
        sum.r2 += subject->scores.r2;
        ssq.r2 += (subject->scores.r2*subject->scores.r2);
        sum.rng += subject->scores.rng;
        ssq.rng += (subject->scores.rng*subject->scores.rng);
        sum.rr += subject->scores.rr;
        ssq.rr += (subject->scores.rr*subject->scores.rr);
        sum.aa += subject->scores.aa;
        ssq.aa += (subject->scores.aa*subject->scores.aa);
        sum.oa += subject->scores.oa;
        ssq.oa += (subject->scores.oa*subject->scores.oa);
        sum.tpi += subject->scores.tpi;
        ssq.tpi += (subject->scores.tpi*subject->scores.tpi);
        sum.rg1 += subject->scores.rg1;
        ssq.rg1 += (subject->scores.rg1*subject->scores.rg1);
        sum.rg2 += subject->scores.rg2;
        ssq.rg2 += (subject->scores.rg2*subject->scores.rg2);
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            sum.associates[k] += subject->scores.associates[k];
            ssq.associates[k] += (subject->scores.associates[k]*subject->scores.associates[k]);
        }
// fprintf(stdout, "%d\t%f\t%f\t%f\t%f\n", i+1, subject->scores.r1, subject->scores.rr, subject->scores.aa, subject->scores.oa);
    }
    if (task_data->group.n > 0) {
        task_data->group.mean.r1 = sum.r1 / (double) task_data->group.n;
        task_data->group.mean.r2 = sum.r2 / (double) task_data->group.n;
        task_data->group.mean.rng = sum.rng / (double) task_data->group.n;
        task_data->group.mean.rr = sum.rr / (double) task_data->group.n;
        task_data->group.mean.aa = sum.aa / (double) task_data->group.n;
        task_data->group.mean.oa = sum.oa / (double) task_data->group.n;
        task_data->group.mean.tpi = sum.tpi / (double) task_data->group.n;
        task_data->group.mean.rg1 = sum.rg1 / (double) task_data->group.n;
        task_data->group.mean.rg2 = sum.rg2 / (double) task_data->group.n;
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            task_data->group.mean.associates[k] = sum.associates[k] / (double) task_data->group.n;
        }
    }
    if (task_data->group.n > 1) {
        task_data->group.sd.r1 = sqrt((ssq.r1 - (sum.r1*sum.r1/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.r2 = sqrt((ssq.r2 - (sum.r2*sum.r2/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.rng = sqrt((ssq.rng - (sum.rng*sum.rng/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.rr = sqrt((ssq.rr - (sum.rr*sum.rr/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.aa = sqrt((ssq.aa - (sum.aa*sum.aa/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.oa = sqrt((ssq.oa - (sum.oa*sum.oa/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.tpi = sqrt((ssq.tpi - (sum.tpi*sum.tpi/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.rg1 = sqrt((ssq.rg1 - (sum.rg1*sum.rg1/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        task_data->group.sd.rg2 = sqrt((ssq.rg2 - (sum.rg2*sum.rg2/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            task_data->group.sd.associates[k] = sqrt((ssq.associates[k]- (sum.associates[k]*sum.associates[k]/ (double) task_data->group.n))/((double) task_data->group.n - 1));
        }
    }

// fprintf(stdout, "--\t%f\t%f\t%f\t%f\n", task_data->group.mean.r1, task_data->group.mean.rr, task_data->group.mean.aa, task_data->group.mean.oa);
// fprintf(stdout, "--\t%f\t%f\t%f\t%f\n", task_data->group.sd.r1, task_data->group.sd.rr, task_data->group.sd.aa, task_data->group.sd.oa);

}

void rng_print_group_data_analysis(FILE *fp, RngData *task_data)
{
    if (fp != NULL) {
        RngGroupData z_scores;
        fprintf(fp, "  R\t RNG\t TPI\t  RG\tAssociates\n");
        fprint_rng_scores(fp, &(task_data->group.mean));
        rng_scores_convert_to_z(&(task_data->group), &subject_ctl, &z_scores);
        fprint_rng_scores(fp, &(z_scores.mean));
        fprintf(fp, "\n");
    }
}

/******************************************************************************/

void rng_scores_convert_to_z(RngGroupData *raw_data, RngGroupData *baseline, RngGroupData *z_scores)
{
    int k;
    
    z_scores->mean.r1 = (raw_data->mean.r1 - baseline->mean.r1) / baseline->sd.r1;
    z_scores->sd.r1 = raw_data->sd.r1 / baseline->sd.r1;
    z_scores->mean.r2 = (raw_data->mean.r2 - baseline->mean.r2) / baseline->sd.r2;
    z_scores->sd.r2 = raw_data->sd.r2 / baseline->sd.r2;
    z_scores->mean.rng = (raw_data->mean.rng - baseline->mean.rng) / baseline->sd.rng;
    z_scores->sd.rng = raw_data->sd.rng / baseline->sd.rng;
    z_scores->mean.rr = (raw_data->mean.rr - baseline->mean.rr) / baseline->sd.rr;
    z_scores->sd.rr = raw_data->sd.rr / baseline->sd.rr;
    z_scores->mean.aa = (raw_data->mean.aa - baseline->mean.aa) / baseline->sd.aa;
    z_scores->sd.aa = raw_data->sd.aa / baseline->sd.aa;
    z_scores->mean.oa = (raw_data->mean.oa - baseline->mean.oa) / baseline->sd.oa;
    z_scores->sd.oa = raw_data->sd.oa / baseline->sd.oa;
    z_scores->mean.tpi = (raw_data->mean.tpi - baseline->mean.tpi) / baseline->sd.tpi;
    z_scores->sd.tpi = raw_data->sd.tpi / baseline->sd.tpi;
    z_scores->mean.rg1 = (raw_data->mean.rg1 - baseline->mean.rg1) / baseline->sd.rg1;
    z_scores->sd.rg1 = raw_data->sd.rg1 / baseline->sd.rg1;
    z_scores->mean.rg2 = (raw_data->mean.rg2 - baseline->mean.rg2) / baseline->sd.rg2;
    z_scores->sd.rg2 = raw_data->sd.rg2 / baseline->sd.rg2;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        z_scores->mean.associates[k] = (raw_data->mean.associates[k] - baseline->mean.associates[k]) / baseline->sd.associates[k];
        z_scores->sd.associates[k] = raw_data->sd.associates[k] / baseline->sd.associates[k];
    }
    z_scores->n = raw_data->n;
}

double rng_data_calculate_fit(RngGroupData *model, RngGroupData *data)
{
    double fit = 0.0;

    fit = MAX(fit, fabs((data->mean.r1 - model->mean.r1) / data->sd.r1));
//    fit = MAX(fit, fabs((data->mean.r2 - model->mean.r2) / data->sd.r2));
    fit = MAX(fit, fabs((data->mean.rng - model->mean.rng) / data->sd.rng));
    fit = MAX(fit, fabs((data->mean.tpi - model->mean.tpi) / data->sd.tpi));
//    fit = MAX(fit, fabs((data->mean.rg1 - model->mean.rg1) / data->sd.rg1));
//    fit = MAX(fit, fabs((data->mean.rg2 - model->mean.rg2) / data->sd.rg2));

    fit = MAX(fit, fabs((data->mean.oa - model->mean.oa) / data->sd.oa));
    fit = MAX(fit, fabs((data->mean.aa - model->mean.aa) / data->sd.aa));
    fit = MAX(fit, fabs((data->mean.rr - model->mean.rr) / data->sd.rr));

    return(fit);
}


