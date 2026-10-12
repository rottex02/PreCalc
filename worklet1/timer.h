#ifndef TIMER_H
#define TIMER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>

typedef struct {
    struct timespec start;
    struct timespec end;
    double elapsed_ms;
} Timer;

void   timer_start(Timer *t);
void   timer_stop(Timer *t);
double timer_elapsed_ms(const Timer *t);

#endif /* TIMER_H */

