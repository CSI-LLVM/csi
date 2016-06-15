#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

namespace {

void destroy() {
    fprintf(stderr, "Destroy tool\n");
}

}

extern "C" {

void __csi_init() {
    fprintf(stderr, "Initialize tool.\n");
    atexit(destroy);
}

void __csi_unit_init(const char * const file_name,
                     const instrumentation_counts_t counts) {
    fprintf(stderr, "Initialize unit id %s, %lu basic blocks, %lu callsites, %lu functions, %lu function exits, %lu loads, %lu stores.\n", file_name, counts.num_bb, counts.num_callsite, counts.num_func, counts.num_func_exit, counts.num_load, counts.num_store);
}

void __csi_before_load(const csi_id_t load_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop){
    fprintf(stderr, "Before load %lu %p (%d bytes) prop=%lu (%s:%d)\n",
            load_id, addr, num_bytes, prop,
            __csi_fed_get_load(load_id)->filename,
            __csi_fed_get_load(load_id)->line_number);
}

void __csi_after_load(const csi_id_t load_id,
                      const void *addr,
                      const int32_t num_bytes,
                      const uint64_t prop){
    fprintf(stderr, "After load %lu %p (%d bytes) prop=%lu (%s:%d)\n",
            load_id, addr, num_bytes, prop,
            __csi_fed_get_load(load_id)->filename,
            __csi_fed_get_load(load_id)->line_number);
}

void __csi_before_store(const csi_id_t store_id,
                        const void *addr,
                        const int32_t num_bytes,
                        const uint64_t prop) {
    fprintf(stderr, "Before store %lu %p (%d bytes) prop=%lu (%s:%d)\n",
            store_id, addr, num_bytes, prop,
            __csi_fed_get_store(store_id)->filename,
            __csi_fed_get_store(store_id)->line_number);
}

void __csi_after_store(const csi_id_t store_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop) {
    fprintf(stderr, "After store %lu %p (%d bytes) prop=%lu (%s:%d)\n",
            store_id, addr, num_bytes, prop,
            __csi_fed_get_store(store_id)->filename,
            __csi_fed_get_store(store_id)->line_number);
}

void __csi_func_entry(const csi_id_t func_id) {
    fprintf(stderr, "Func entry %lu (%s:%d)\n",
            func_id,
            __csi_fed_get_func(func_id)->filename,
            __csi_fed_get_func(func_id)->line_number);
}

void __csi_func_exit(const csi_id_t func_exit_id,
                     const csi_id_t func_id) {
    fprintf(stderr, "Func exit %lu from %lu (%s:%d)\n",
            func_exit_id, func_id,
            __csi_fed_get_func_exit(func_exit_id)->filename,
            __csi_fed_get_func_exit(func_exit_id)->line_number);
}

void __csi_bb_entry(const csi_id_t bb_id) {
    fprintf(stderr, "Basic block entry %lu (%s:%d)\n", bb_id,
            __csi_fed_get_bb(bb_id)->filename,
            __csi_fed_get_bb(bb_id)->line_number);
}

void __csi_bb_exit(const csi_id_t bb_id) {
    fprintf(stderr, "Basic block exit %lu (%s:%d)\n", bb_id,
            __csi_fed_get_bb(bb_id)->filename,
            __csi_fed_get_bb(bb_id)->line_number);
}

void __csi_before_callsite(const csi_id_t callsite_id, const csi_id_t func_id) {
    fprintf(stderr, "Before callsite %lu (%s:%d) to ", callsite_id,
            __csi_fed_get_callsite(callsite_id)->filename,
            __csi_fed_get_callsite(callsite_id)->line_number);
    if (__csirt_is_callsite_target_unknown(callsite_id, func_id)) {
        fprintf(stderr, "unknown function.\n");
    } else {
        fprintf(stderr, "%lu (%s:%d)\n", func_id,
                __csi_fed_get_func(func_id)->filename,
                __csi_fed_get_func(func_id)->line_number);
    }
}


} // extern "C"

// Local Variables:
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: nil
// End:
