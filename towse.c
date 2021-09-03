/* Generate expected values for  R, RNG and A for different set sizes */

#include "rng.h"
#include "lib_math.h"

#define NUM_TRIALS 100
#define NUM_GROUP  2500
// Repeat the generation NUM_REPS times, averaging over the results
#define NUM_REPS 1000

/******************************************************************************/

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

static void rng_score_subject_data(RngSubjectData *subject)
{
    /* Read a data file and return the various RNG dvs. */
    /* Also return misses (times participant failed to generate a response) */
    /* and hits (responses generated, excluding misses). */

    int f[RESPONSE_SET_SIZE], ff[RESPONSE_SET_SIZE][RESPONSE_SET_SIZE], fff[RESPONSE_SET_SIZE][RESPONSE_SET_SIZE][RESPONSE_SET_SIZE], last[RESPONSE_SET_SIZE];
    int rg_list[MAX_TRIALS];
    double rt, rt_sum, sum, h_s, h_m;
    double num, denom;
    long v0, v1, v2, fr, k;
    int tp_direction, tp_count, n, d, i, j;
    int hits = 0, misses = 0, bigrams = 0;

    for (v1 = 0; v1 < RESPONSE_SET_SIZE; v1++) {
        f[v1] = 0;
        last[v1] = -1;
        for (v2 = 0; v2 < RESPONSE_SET_SIZE; v2++) {
            ff[v1][v2] = 0;
            for (v0 = 0; v0 < 10; v0++) {
                fff[v0][v1][v2] = 0;
            }
        }
    }

    /* Score the data: */

    v1 = -1; v0 = -1; fr = -1; tp_direction = 0; n = 0; i = 0; tp_count = 0; rt_sum = 0.0;

    for (j = 0; j < subject->n; j++) {
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

        /* Count frequencies for triplets: */
        if ((v0 >= 0) && (v1 >= 0) && (v2 >= 0)) {
            fff[v0][v1][v2]++;
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
            v0 = v1;
            v1 = v2;
        }

        if ((v2 >= 0) && (i < subject->n)) {
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
        if (v1 > 0) {
            fff[v1][v2][fr]++;
        }
    }

    /* Calculation of R1: */
    /* We use log base 2, but of the answer is independent of base */
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
    /* We use log base 2, but of the answer is independent of base */
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

    /* Calculation of R3: */
    h_s = 0.0;
    h_m = 0.0;
    bigrams = hits;
    for (v1 = 0; v1 < 10; v1++) {
        for (v2 = 0; v2 < 10; v2++) {
            for (v0 = 0; v0 < 10; v0++) {
                if (fff[v0][v1][v2] > 1) {
                    h_s -= fff[v0][v1][v2]/(double)bigrams * log(fff[v0][v1][v2]/(double)bigrams) / log(2.0);
                }
            }
            if (ff[v1][v2] > 1) {
                h_m -= ff[v1][v2]/(double)bigrams * log(ff[v1][v2]/(double)bigrams) / log(2.0);
            }
        }
    }
    subject->scores.r3 = 100.0 * (1 - h_s / (2.0*h_m));

    /* Calculation of RNG: */
    /* We use log base 2, but of the answer is independent of base */
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

    /* Calculation of RNG2: */
    num = 0.0;
    denom = 0.0;

    for (v1 = 0; v1 < 10; v1++) {
	for (v2 = 0; v2 < 10; v2++) {
	    for (v0 = 0; v0 < 10; v0++) {
                if (fff[v0][v1][v2] > 1) {
                    num = num + fff[v0][v1][v2] * log(fff[v0][v1][v2]) / log(2.0);
                }
	    }
	    if (ff[v1][v2] > 1) {
		denom = denom + ff[v1][v2] * log(ff[v1][v2]) / log(2.0);
	    }
	}
    }
    subject->scores.rng2 = num / denom;

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

    /* Calculation of TPI: */ // 79/198 determined empiricall! CHANGE 1.0 to something else!
    subject->scores.tpi = tp_count / ((79.0/198.0) * (n - 2.0)); // 100.0 * (tp_count / (2.0 * (n - 2.0) / 3.0));

    /* Calculate RG (repetition gap): */
    subject->scores.rg1 = calculate_mean(rg_list, i);
    subject->scores.rg2 = calculate_median(rg_list, i);
}

/******************************************************************************/

static void rng_analyse_group(RngSubjectData subjects[NUM_GROUP], RngGroupData *group)
{
    RngScores sum, ssq;
    int i, k;

    sum.r1 = 0.0;
    sum.r2 = 0.0;
    sum.r3 = 0.0;
    sum.rng = 0.0;
    sum.rng2 = 0.0;
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
    ssq.r3 = 0.0;
    ssq.rng = 0.0;
    ssq.rng2 = 0.0;
    ssq.rr = 0.0;
    ssq.aa = 0.0;
    ssq.oa = 0.0;
    ssq.tpi = 0.0;
    ssq.rg1 = 0.0;
    ssq.rg2 = 0.0;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        ssq.associates[k] = 0.0;
    }

    for (i = 0; i < NUM_GROUP; i++) {
        sum.r1 += subjects[i].scores.r1;
        ssq.r1 += (subjects[i].scores.r1*subjects[i].scores.r1);
        sum.r2 += subjects[i].scores.r2;
        ssq.r2 += (subjects[i].scores.r2*subjects[i].scores.r2);
        sum.r3 += subjects[i].scores.r3;
        ssq.r3 += (subjects[i].scores.r3*subjects[i].scores.r3);
        sum.rng += subjects[i].scores.rng;
        ssq.rng += (subjects[i].scores.rng*subjects[i].scores.rng);
        sum.rng2 += subjects[i].scores.rng2;
        ssq.rng2 += (subjects[i].scores.rng2*subjects[i].scores.rng2);
        sum.rr += subjects[i].scores.rr;
        ssq.rr += (subjects[i].scores.rr*subjects[i].scores.rr);
        sum.aa += subjects[i].scores.aa;
        ssq.aa += (subjects[i].scores.aa*subjects[i].scores.aa);
        sum.oa += subjects[i].scores.oa;
        ssq.oa += (subjects[i].scores.oa*subjects[i].scores.oa);
        sum.tpi += subjects[i].scores.tpi;
        ssq.tpi += (subjects[i].scores.tpi*subjects[i].scores.tpi);
        sum.rg1 += subjects[i].scores.rg1;
        ssq.rg1 += (subjects[i].scores.rg1*subjects[i].scores.rg1);
        sum.rg2 += subjects[i].scores.rg2;
        ssq.rg2 += (subjects[i].scores.rg2*subjects[i].scores.rg2);
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            sum.associates[k] += subjects[i].scores.associates[k];
            ssq.associates[k] += (subjects[i].scores.associates[k]*subjects[i].scores.associates[k]);
        }
    }

    group->n = NUM_GROUP;

    if (group->n > 0) {
        group->mean.r1 = sum.r1 / (double) group->n;
        group->mean.r2 = sum.r2 / (double) group->n;
        group->mean.r3 = sum.r3 / (double) group->n;
        group->mean.rng = sum.rng / (double) group->n;
        group->mean.rng2 = sum.rng2 / (double) group->n;
        group->mean.rr = sum.rr / (double) group->n;
        group->mean.aa = sum.aa / (double) group->n;
        group->mean.oa = sum.oa / (double) group->n;
        group->mean.tpi = sum.tpi / (double) group->n;
        group->mean.rg1 = sum.rg1 / (double) group->n;
        group->mean.rg2 = sum.rg2 / (double) group->n;
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            group->mean.associates[k] = sum.associates[k] / (double) group->n;
        }
    }
    if (group->n > 1) {
        group->sd.r1 = sqrt((ssq.r1 - (sum.r1*sum.r1/ (double) group->n))/((double) group->n - 1));
        group->sd.r2 = sqrt((ssq.r2 - (sum.r2*sum.r2/ (double) group->n))/((double) group->n - 1));
        group->sd.r3 = sqrt((ssq.r3 - (sum.r3*sum.r3/ (double) group->n))/((double) group->n - 1));
        group->sd.rng = sqrt((ssq.rng - (sum.rng*sum.rng/ (double) group->n))/((double) group->n - 1));
        group->sd.rng2 = sqrt((ssq.rng2 - (sum.rng2*sum.rng2/ (double) group->n))/((double) group->n - 1));
        group->sd.rr = sqrt((ssq.rr - (sum.rr*sum.rr/ (double) group->n))/((double) group->n - 1));
        group->sd.aa = sqrt((ssq.aa - (sum.aa*sum.aa/ (double) group->n))/((double) group->n - 1));
        group->sd.oa = sqrt((ssq.oa - (sum.oa*sum.oa/ (double) group->n))/((double) group->n - 1));
        group->sd.tpi = sqrt((ssq.tpi - (sum.tpi*sum.tpi/ (double) group->n))/((double) group->n - 1));
        group->sd.rg1 = sqrt((ssq.rg1 - (sum.rg1*sum.rg1/ (double) group->n))/((double) group->n - 1));
        group->sd.rg2 = sqrt((ssq.rg2 - (sum.rg2*sum.rg2/ (double) group->n))/((double) group->n - 1));
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            group->sd.associates[k] = sqrt((ssq.associates[k] - (sum.associates[k]*sum.associates[k]/ (double) group->n))/((double) group->n - 1));
        }
    }
}

/******************************************************************************/

static void rng_average_group_data(RngGroupData *group, RngGroupData *groups)
{
    RngScores sum;
    int r, k;

    group->n = 0;

    sum.r1 = 0.0;
    sum.r2 = 0.0;
    sum.r3 = 0.0;
    sum.rng = 0.0;
    sum.rng2 = 0.0;
    sum.rr = 0.0;
    sum.aa = 0.0;
    sum.oa = 0.0;
    sum.tpi = 0.0;
    sum.rg1 = 0.0;
    sum.rg2 = 0.0;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        sum.associates[k] = 0.0;
    }

    for (r = 0; r < NUM_REPS; r++) {
        sum.r1 += groups[r].mean.r1;
        sum.r2 += groups[r].mean.r2;
        sum.r3 += groups[r].mean.r3;
        sum.rng += groups[r].mean.rng;
        sum.rng2 += groups[r].mean.rng2;
        sum.rr += groups[r].mean.rr;
        sum.aa += groups[r].mean.aa;
        sum.oa += groups[r].mean.oa;
        sum.tpi += groups[r].mean.tpi;
        sum.rg1 += groups[r].mean.rg1;
        sum.rg2 += groups[r].mean.rg2;
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            sum.associates[k] += groups[r].mean.associates[k];
        }
        group->n += groups[r].n;
    }

    group->mean.r1 = sum.r1 / (double) NUM_REPS;
    group->mean.r2 = sum.r2 / (double) NUM_REPS;
    group->mean.r3 = sum.r3 / (double) NUM_REPS;
    group->mean.rng = sum.rng / (double) NUM_REPS;
    group->mean.rng2 = sum.rng2 / (double) NUM_REPS;
    group->mean.rr = sum.rr / (double) NUM_REPS;
    group->mean.aa = sum.aa / (double) NUM_REPS;
    group->mean.oa = sum.oa / (double) NUM_REPS;
    group->mean.tpi = sum.tpi / (double) NUM_REPS;
    group->mean.rg1 = sum.rg1 / (double) NUM_REPS;
    group->mean.rg2 = sum.rg2 / (double) NUM_REPS;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        group->mean.associates[k] = sum.associates[k] / (double) NUM_REPS;
    }

    /* Same again for SD: */

    sum.r1 = 0.0;
    sum.r2 = 0.0;
    sum.r3 = 0.0;
    sum.rng = 0.0;
    sum.rng2 = 0.0;
    sum.rr = 0.0;
    sum.aa = 0.0;
    sum.oa = 0.0;
    sum.tpi = 0.0;
    sum.rg1 = 0.0;
    sum.rg2 = 0.0;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        sum.associates[k] = 0.0;
    }

    for (r = 0; r < NUM_REPS; r++) {
        sum.r1 += groups[r].sd.r1;
        sum.r2 += groups[r].sd.r2;
        sum.r3 += groups[r].sd.r3;
        sum.rng += groups[r].sd.rng;
        sum.rng2 += groups[r].sd.rng2;
        sum.rr += groups[r].sd.rr;
        sum.aa += groups[r].sd.aa;
        sum.oa += groups[r].sd.oa;
        sum.tpi += groups[r].sd.tpi;
        sum.rg1 += groups[r].sd.rg1;
        sum.rg2 += groups[r].sd.rg2;
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            sum.associates[k] += groups[r].sd.associates[k];
        }
        group->n += groups[r].n;
    }

    group->sd.r1 = sum.r1 / (double) NUM_REPS;
    group->sd.r2 = sum.r2 / (double) NUM_REPS;
    group->sd.r3 = sum.r3 / (double) NUM_REPS;
    group->sd.rng = sum.rng / (double) NUM_REPS;
    group->sd.rng2 = sum.rng2 / (double) NUM_REPS;
    group->sd.rr = sum.rr / (double) NUM_REPS;
    group->sd.aa = sum.aa / (double) NUM_REPS;
    group->sd.oa = sum.oa / (double) NUM_REPS;
    group->sd.tpi = sum.tpi / (double) NUM_REPS;
    group->sd.rg1 = sum.rg1 / (double) NUM_REPS;
    group->sd.rg2 = sum.rg2 / (double) NUM_REPS;
    for (k = 0; k < RESPONSE_SET_SIZE; k++) {
        group->sd.associates[k] = sum.associates[k] / (double) NUM_REPS;
    }
}

/******************************************************************************/

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

        fprintf(fp, "%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f\t%7.5f", score->r1, score->r2, score->r3, score->rng, score->rng2, score->rr, score->aa, score->oa, score->tpi, score->rg1, score->rg2);
        for (k = 0; k < RESPONSE_SET_SIZE; k++) {
            fprintf(fp, "\t%7.5f", score->associates[k]);
        }
        fprintf(fp, "\n");
    }
}

void rng_print_scores(FILE *fp, RngScores *scores)
{
    if (fp != NULL) {
        fprintf(fp, "  R1\t R2\t R3\t RNG\t RNG2\t  RR\t  AA\t   OA\t   TPI\t  RG1\t  RG2\tAssociates\n");
        fprint_rng_scores(fp, scores);
        fprintf(fp, "\n");
    }
}

void rng_print_statistics(FILE *fp, RngScores *means, RngScores *sds)
{
    fprintf(fp, "RG (Mean) = %6.4f (%6.4f); RG (Median) = %6.4f (%6.4f)\n", means->rg1, sds->rg1, means->rg2, sds->rg2);
}

/******************************************************************************/

int main(int argc, char *argv[])
{
    RngSubjectData subject[NUM_GROUP];
    RngGroupData groups[NUM_REPS], group;
    int i, j, r;

    random_initialise();
    for (r = 0; r < NUM_REPS; r++) {
        for (j = 0; j < NUM_GROUP; j++) {
            for (i = 0; i < NUM_TRIALS; i++) {
                subject[j].response[i] = random_integer(0, RESPONSE_SET_SIZE);
            }
            subject[j].n = NUM_TRIALS;
            rng_score_subject_data(&subject[j]);
        }
        rng_analyse_group(&subject[0], &groups[r]);
        fprintf(stdout, "."); fflush(stdout);
    }
    fprintf(stdout, "\n");

    rng_average_group_data(&group, groups);

    fprintf(stdout, "Group means (N = %d; length = %d; response set = %d):\n", group.n, NUM_TRIALS, RESPONSE_SET_SIZE);
    rng_print_scores(stdout, &(group.mean));
    fprintf(stdout, "Group SDs (N = %d; length = %d; response set = %d):\n", group.n, NUM_TRIALS, RESPONSE_SET_SIZE);
    rng_print_scores(stdout, &(group.sd));
//    rng_print_statistics(stdout, &(group.mean), &(group.sd));

    exit(0);
}
