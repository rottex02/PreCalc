#include "timer.h"

void timer_start(Timer *t) {
    if (!t) return;
    t->elapsed_ms = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

void timer_stop(Timer *t) {
    if (!t) return;
    clock_gettime(CLOCK_MONOTONIC, &t->end);
    double sec = (double)(t->end.tv_sec - t->start.tv_sec);
    double nsec = (double)(t->end.tv_nsec - t->start.tv_nsec);
    t->elapsed_ms = (sec * 1000.0) + (nsec / 1000000.0);
}

double timer_elapsed_ms(const Timer *t) {
    return (t ? t->elapsed_ms : 0.0);
}

