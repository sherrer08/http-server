#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <stdlib.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                 \
    do {                                                            \
        checks_run++;                                               \
        if (!(cond)) {                                              \
            checks_failed++;                                        \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",            \
                    __FILE__, __LINE__, #cond);                     \
        }                                                           \
    } while (0)

/* Prints the summary and gives the exit code; use as: return CHECK_SUMMARY(); */
#define CHECK_SUMMARY()                                             \
    (printf("%d checks, %d failed\n", checks_run, checks_failed),   \
     checks_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS)

#endif