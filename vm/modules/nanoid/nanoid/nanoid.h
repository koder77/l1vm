#ifndef __NANOID_H__
#define __NANOID_H__

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// default alphabet list
// updated to nanoid 2.0
static char alphs[] = {
    '-', '_',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '\0'
};

// custom alphabet pool and length
char* custom(char alphs[], int size);

// custom length
char* generate(int size);

// default character pool and 21 in length
char* simple();

// safe implementation of custom() which uses /dev/urandom
char* safe_custom(char alphs[], int size);

// safe implementation of generate()
char* safe_generate(int size);

// safe implementation of simple()
char* safe_simple();

char* custom(char alphs[], int size) {
    int alph_size = strlen(alphs) - 1;
    char *id = (char *) malloc(sizeof(char) * 3);

    for( int i = 0; i < size; i++ ) {
        int random_num;
        do {
            random_num = rand();
        } while (random_num >= (RAND_MAX - RAND_MAX % alph_size));

        random_num %= alph_size;
        id[i] = alphs[random_num];
    }

    return id;
}

char* generate(int size) {
    return custom(alphs, size);
}

char* simple() {
    return generate(21);
}


char* safe_custom(char alphs[], int size) {
    char *buffer;
    char *rand_buf;
    unsigned int sum;
    FILE *rand_src;

    buffer = (char *) malloc(size);
    rand_buf = (char *) malloc(size);
    rand_src = fopen("/dev/urandom", "rb");

    if (rand_src == NULL)
        return NULL;

    fread(buffer, size, 1, rand_src);

    for (int i = 0; i < size; ++i) {
        sum += buffer[i];
    }

    free(buffer);

    srand(sum);

    for (int j = 0; j < size; ++j) {
        unsigned int random_num;

        do {
            random_num = rand();
        } while (random_num >= (RAND_MAX - RAND_MAX % strlen(alphs)));

        random_num %= strlen(alphs);
        rand_buf[j] = alphs[random_num];
    }

    return rand_buf;
}

char* safe_generate(int size) {
    return safe_custom(alphs, size);
}

char* safe_simple() {
    return safe_generate(21);
}

#endif // __NANOID_H__
