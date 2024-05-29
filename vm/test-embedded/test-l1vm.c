// test-l1vm.c
//
// This is a demo program of how to use the L1VM as a shared library program

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "l1vm-embedded.h"

int main (void) {
    long long int data_size, i, pwrap = 0;
    unsigned char *data_ptr = NULL;
    int ret;
    unsigned char hexstr[3];

    printf("starting L1VM hello world program...\n");

    ret = run_program ("hello");
    if (ret != 0)
    {
        printf ("error running program! exit!\n");
        cleanup ();
        exit (1);
    }

    data_size = get_global_data_size ();
    data_ptr = get_global_data ();

    printf ("\n\nmemdump global data:\n");
    for (i = 0; i < data_size; i++)
    {
        sprintf ((char *) hexstr, "%X ", data_ptr[i]);

        if (strlen ((const char *) hexstr) < 3)
        {
            printf ("0");
        }

        printf ("%s", hexstr);

        pwrap++;
        if (pwrap == 16)
        {
            printf ("\n");
            pwrap = 0;
        }
    }
    printf ("\n\n");

    // call the L1VM memory cleanup function:
    cleanup ();
    exit (0);
}
