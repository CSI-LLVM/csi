#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

namespace {
long times_accessed_by_size[4];

void destroy() {
    fprintf(stderr, "bytes accessed: %ld\n", times_accessed_by_size[0]);
    fprintf(stderr, "shorts accessed: %ld\n", times_accessed_by_size[1]);
    fprintf(stderr, "ints accessed: %ld\n", times_accessed_by_size[2]);
    fprintf(stderr, "longs accessed: %ld\n", times_accessed_by_size[3]);
}
}

extern "C" {

// void __csi_init(csi_info_t info) {
void __csi_init(uint32_t num_modules){
    atexit(destroy);
}

void __csi_before_load(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
    times_accessed_by_size[__builtin_ctz(num_bytes)]++;
}

void __csi_before_store(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
    times_accessed_by_size[__builtin_ctz(num_bytes)]++;
}

} // extern "C"
