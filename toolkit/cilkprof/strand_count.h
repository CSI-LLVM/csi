#ifndef INCLUDED_STRAND_COUNT_DOT_H
#define INCLUDED_STRAND_COUNT_DOT_H

#include <stdbool.h>

typedef struct strand_ruler_t {
  uint64_t started;
} strand_ruler_t;

static inline void init_strand_ruler(strand_ruler_t *strand_ruler) {
  strand_ruler->started = 0;
}

static inline void start_strand(strand_ruler_t *strand_ruler) {
  ++strand_ruler->started;
}

static inline void stop_strand(strand_ruler_t *strand_ruler) {
  --strand_ruler->started;
}

static inline uint64_t measure_strand_length(strand_ruler_t *strand_ruler) {
  // End of strand
  stop_strand(strand_ruler);
  assert(strand_ruler->started == 0);
  return 1;
}

static inline void print_work_span(uint64_t work, uint64_t span) {
  fprintf(stderr, "work %lu strands, span %lu strands, parallelism %f\n",
          work, span, work / (float)span);
}

#endif
