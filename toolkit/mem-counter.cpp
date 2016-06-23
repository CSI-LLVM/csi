#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

namespace {
long times_accessed_by_size[4];

void report() {
    fprintf(stderr, "bytes accessed: %ld\n", times_accessed_by_size[0]);
    fprintf(stderr, "shorts accessed: %ld\n", times_accessed_by_size[1]);
    fprintf(stderr, "ints accessed: %ld\n", times_accessed_by_size[2]);
    fprintf(stderr, "longs accessed: %ld\n", times_accessed_by_size[3]);
}
}

extern "C" {

void __csi_init() {
    atexit(report);
}

void __csi_before_load(const csi_id_t load_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop) {
    times_accessed_by_size[__builtin_ctz(num_bytes)]++;
}

void __csi_before_store(const csi_id_t store_id,
                        const void *addr,
                        const int32_t num_bytes,
                        const uint64_t prop) {
    times_accessed_by_size[__builtin_ctz(num_bytes)]++;
}

} // extern "C"
