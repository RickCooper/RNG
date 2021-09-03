
#include "oos.h"

extern int oos_count_buffer_elements(OosVars *gv, int id);

#define MY_BUFFER 23
#define MY_PROCESS 32

/******************************************************************************/

static void my_process_output(OosVars *gv)
{
    if (gv->cycle == 1) {
        int i;
        for (i = 0; i < 100; i++) {
            ClauseType *schema = pl_clause_make_from_string("wme(_).");
            pl_arg_set_to_int(schema, 1, i);
            oos_message_create(gv, MT_ADD, MY_PROCESS, MY_BUFFER, schema);
        }
    }
}

/******************************************************************************/

static void test_initialise(OosVars *gv, BufferDecayProp decay)
{
    oos_messages_free(gv);
    oos_components_free(gv);

    oos_buffer_create(gv, "Test Buffer", MY_BUFFER, decay, 40, BUFFER_CAPACITY_UNLIMITED, 0, BUFFER_EXCESS_IGNORE, BUFFER_ACCESS_RANDOM);
    oos_process_create(gv, "Test Process", MY_PROCESS, my_process_output);

    gv->name = string_copy("Test Model");
    gv->stopped = FALSE;
    gv->cycle = 0;
    gv->block = 0;
}

/******************************************************************************/

void test_run(OosVars *gv)
{
    BufferDecayProp decay;
    int occurs[5][250];
    int j, k;

    for (k = 0; k < 5; k++) {
        for (j = 0; j < 250; j++) {
            occurs[k][j] = 0;
        }
    }

    for (decay = BUFFER_DECAY_FIXED; decay <= BUFFER_DECAY_WEIBULL; decay++) {
        for (k = 0; k < 1000; k++) {
            test_initialise(gv, decay);
            do {
                oos_step(gv);
                j = oos_count_buffer_elements(gv, MY_BUFFER);
                occurs[decay - BUFFER_DECAY_FIXED][gv->cycle-1] += j;
    //            fprintf(stdout, "%d\t%d\n", gv->cycle, j);
            } while ((j > 0) && (gv->cycle < 250));
            fputc('.', stdout); fflush(stdout);
        }
        fputc('\n', stdout);
    }

    for (j = 0; j < 250; j++) {
        fprintf(stdout, "%d\t%f\t%f\t%f\t%f\t%f\n", j, occurs[0][j] / 1000.0, occurs[1][j] / 1000.0, occurs[2][j] / 1000.0, occurs[3][j] / 1000.0, occurs[4][j] / 1000.0);

    }
}

/******************************************************************************/

int main(int argc, char **argv)
{
    OosVars *gv;

    if ((gv = oos_globals_create()) == NULL) {
        fprintf(stdout, "ABORTING: Cannot allocate global variable space\n");
    }
    else {
        test_run(gv);
        oos_globals_destroy(gv);
    }
    exit(1);
}

/******************************************************************************/

