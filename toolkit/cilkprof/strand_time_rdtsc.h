#ifndef INCLUDED_STRAND_TIME_DOT_H
#define INCLUDED_STRAND_TIME_DOT_H

#include <stdio.h>
#include <time.h>

typedef struct strand_ruler_t {
  uint64_t start;
  uint64_t stop;

  /* uint32_t start_lo; */
  /* uint32_t start_hi; */
  /* uint32_t stop_lo; */
  /* uint32_t stop_hi; */
  
} strand_ruler_t;


static __attribute__((always_inline))
unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return (unsigned long long)lo + (((unsigned long long)hi) << 32);
  /* return (   ((unsigned long long)lo) */
  /*         | (((unsigned long long)hi)<<32)); */
}

// Store the current "time" into TIMER.
static inline void gettime(uint64_t *timer) {
  *timer = rdtsc();
}

// Get the number of nanoseconds elapsed between STOP and START.
static inline uint64_t elapsed_cycles(const uint64_t *start,
                                      const uint64_t *stop) {
  return *stop - *start;
}

static inline void init_strand_ruler(strand_ruler_t *strand_ruler) {
  return;
}

static inline void start_strand(strand_ruler_t *strand_ruler) {
  gettime(&(strand_ruler->start));
  /* __asm__ __volatile__ ("rdtsc" : "=a"(strand_ruler->start_lo), "=d"(strand_ruler->start_hi)); */
}

static inline void stop_strand(strand_ruler_t *strand_ruler) {
  gettime(&(strand_ruler->stop));
  /* __asm__ __volatile__ ("rdtsc" : "=a"(strand_ruler->stop_lo), "=d"(strand_ruler->stop_hi)); */
}

static inline uint64_t measure_strand_length(strand_ruler_t *strand_ruler) {
  // End of strand
  stop_strand(strand_ruler);
  return elapsed_cycles(&(strand_ruler->start), &(strand_ruler->stop));

  /* return ((uint64_t)(strand_ruler->stop_lo) + ((uint64_t)(strand_ruler->stop_hi) << 32)) */
  /*     - ((uint64_t)(strand_ruler->start_lo) + ((uint64_t)(strand_ruler->start_hi) << 32)); */
}

static inline void print_work_span(uint64_t work, uint64_t span) {
  fprintf(stderr, "work %f Gcycles, span %f Gcycles, parallelism %f\n",
          work / (1000000000.0),
          span / (1000000000.0),
          work / (float)span);
}

#endif
