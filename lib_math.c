
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

void random_initialise()
{
    srandom(time(NULL));
}

double random_normal(double mean, double sd)
{
    /* This generates a random integer with given mean and standard deviation */

    double r1 = random() / (double) RAND_MAX;
    double r2 = random() / (double) RAND_MAX;

    return(mean + sd * sqrt(-2 * log(r1)) * cos(2.0 * M_PI * r2));
}

int random_integer(int min, int max)
{
    // Return a random integer >= min, < max
    return((int) (min + random() * ((max - min) / ((double) RAND_MAX+1))));
}

double random_uniform(double min, double max)
{
    // Return a random double >= min, <= max
    return(min + random() * ((max - min) / (double) RAND_MAX));
}

