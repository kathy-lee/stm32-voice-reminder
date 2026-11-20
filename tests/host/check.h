/* Minimal check macro shared by the host tests. */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int failures;

#define CHECK_INT(want, got)                                                  \
    do {                                                                      \
        long w_ = (want), g_ = (got);                                         \
        if (w_ != g_) {                                                       \
            printf("  FAIL %s:%d: %s: want %ld, got %ld\n", __func__,         \
                   __LINE__, #got, w_, g_);                                   \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#endif
