#ifndef __TIME_OP_H__
#define __TIME_OP_H__

#include <time.h>
#include <sys/time.h>

#define CREATE_TIMEVAL(ms) (struct timeval) {.tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000}

#define timecmp(a, b, CMP) \
  (((a)->tv_sec == (b)->tv_sec) ? \
   ((a)->tv_usec CMP (b)->tv_usec) : \
   ((a)->tv_sec CMP (b)->tv_sec))

struct timeval timestamp();

struct timeval timevalAdd (struct timeval *t1, struct timeval *t2);

int verifyTimeout(struct timeval limit);

#endif
