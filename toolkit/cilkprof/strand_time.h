#ifndef INCLUDED_STRAND_TIME_DOT_H
#define INCLUDED_STRAND_TIME_DOT_H

#include <stdio.h>
#include <time.h>

typedef struct strand_ruler_t {
  struct timespec start;
  struct timespec stop;
} strand_ruler_t;

// Store the current "time" into TIMER.
static inline void gettime(struct timespec *timer) {
  // TB 2014-08-01: This is the "clock_gettime" variant I could get
  // working with -std=c11.  I want to use TIME_MONOTONIC instead, but
  // it does not appear to be supported on my system.
  // timespec_get(timer, TIME_UTC);
  clock_gettime(CLOCK_MONOTONIC, timer);
}

// Get the number of nanoseconds elapsed between STOP and START.
static inline uint64_t elapsed_nsec(const struct timespec *start,
                                    const struct timespec *stop) {
  /* fprintf(stderr, "stop (%lu %lu), start: (%lu %lu)\n", */
  /*         stop->tv_sec, stop->tv_nsec, start->tv_sec, start->tv_nsec); */
  return (uint64_t)(stop->tv_sec - start->tv_sec) * 1000000000ll
    + (stop->tv_nsec - start->tv_nsec);
}

static inline void init_strand_ruler(strand_ruler_t *strand_ruler) {
  return;
}

static inline void start_strand(strand_ruler_t *strand_ruler) {
  gettime(&(strand_ruler->start));
}

static inline void stop_strand(strand_ruler_t *strand_ruler) {
  gettime(&(strand_ruler->stop));
}

static inline uint64_t measure_strand_length(strand_ruler_t *strand_ruler) {
  // End of strand
  stop_strand(strand_ruler);

  return elapsed_nsec(&(strand_ruler->start), &(strand_ruler->stop));
}

static inline void print_work_span(uint64_t work, uint64_t span) {
  fprintf(stderr, "work %fs, span %fs, parallelism %f\n",
          work / (1000000000.0),
          span / (1000000000.0),
          work / (float)span);
}

#endif
