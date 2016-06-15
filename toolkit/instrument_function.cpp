#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

void foo(int i);

bool should_instrument = false;

extern "C" {
    // TODO(ddoucet): this doesn't work with recursive functions. Need a stack
    // of function pointers to push on func_entry and pop on func_exit. Then
    // should_instrument is whether the top element is foo

    void __csi_before_load(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
        if (should_instrument)
            fprintf(stderr, "Before load %p (%d bytes) unused %d, unused2 %d, unused3 %d, read_before_write_in_bb %d\n", addr, num_bytes, unused, unused2, unused3, read_before_write_in_bb);
    }
    void __csi_after_load(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
        if (should_instrument)
            fprintf(stderr, "After load %p (%d bytes) unused %d, unused2 %d, unused3 %d, read_before_write_in_bb %d\n", addr, num_bytes, unused, unused2, unused3, read_before_write_in_bb);
    }
    void __csi_before_store(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
        if (should_instrument)
            fprintf(stderr, "Before store %p (%d bytes) unused %d, unused2 %d, unused3 %d, read_before_write_in_bb %d\n", addr, num_bytes, unused, unused2, unused3, read_before_write_in_bb);
    }
    void __csi_after_store(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
        if (should_instrument)
            fprintf(stderr, "After store %p (%d bytes) unused %d, unused2 %d, unused3 %d, read_before_write_in_bb %d\n", addr, num_bytes, unused, unused2, unused3, read_before_write_in_bb);
    }

    void __csi_func_entry(void *function, void *parentReturnAddr, char *funcName) {
        if (function == foo)
            should_instrument = true;
    }

    void __csi_func_exit() {
        should_instrument = false;
    }

} // extern "C"
