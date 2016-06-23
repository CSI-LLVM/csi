#include <stdio.h>
#include <stdlib.h>
#include "csi.h"
#include "rdtsc.h"

namespace {
    uint64_t global_time_used = 0;
    uint64_t count = 0;
    uint64_t last_time;
    bool init = false;

    void destroy() {
        fprintf(stderr, "global time used: %lu, count: %lu\n", global_time_used, count);
    }

    void measure_time() {
        uint64_t  now = rdtsc();
        if (!init) { init = true; last_time = now; }
        global_time_used += now - last_time;
        count++;
        last_time = rdtsc();
    }
}

extern "C" {
    // void __csi_init(csi_info_t info) {
    void __csi_init(uint32_t num_modules) {
        atexit(destroy);
    }

    void __csi_func_entry(void *function, void *parentReturnAddr, char *funcName) {
        measure_time();
    }

    void __csi_func_exit() {
        measure_time();
    }
}
