#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main ()
{
    long long int limit = 100000000;
    //long long int limit = 50;
    unsigned char *primes, a;
    long long int i = 0, j = 0, k = 0, n = 0;
    clock_t start, end;
    double time_used;

    primes = (unsigned char *) calloc (limit, 1);
    if (primes == NULL)
    {
        printf ("error: can't allocate primes array!\n");
        exit (1);
    }

    start = clock ();

    i = 2;
    search_primes:
    n = i * i;
    if (n < limit)
    {
        a = primes[i];
        if (a == 0)
        {
            j = i;
            search:
            k = i * j;
            if (k < limit)
            {
                primes[k] = 1;
                j++;
                goto search;
            }
        }
        i++;
        goto search_primes;
    }

    end = clock ();
    time_used = ((double) (end - start) / CLOCKS_PER_SEC);
    printf ("time: %lf seconds\n\n", time_used);

    // print primes
    for (i = 2; i < limit; i++)
    {
        if (primes[i] == 0)
        {
            printf ("%lli\n", i);
        }
    }

    free (primes);
    exit (0);
}
