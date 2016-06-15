#include "csi.h"
#include <stdio.h>

static inline void print_call(const char *const msg,
                              source_loc_t const * source_loc) {
    printf("%s:%d -- %s\n",
           source_loc->filename,
           source_loc->line_number,
           msg);
}

void __csi_init() {
    printf("__csi_init\n");
}

void __csi_unit_init(const char * const file_name,
                     const instrumentation_counts_t counts) {
    fprintf(stderr, "Initialize unit id %s, %lu basic blocks, %lu callsites, %lu functions, %lu function exits, %lu loads, %lu stores.\n", file_name, counts.num_bb, counts.num_callsite, counts.num_func, counts.num_func_exit, counts.num_load, counts.num_store);
}

void __csi_before_load(const csi_id_t load_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop) {
    print_call("__csi_before_load", __csi_fed_get_load(load_id));
}

void __csi_after_load(const csi_id_t load_id,
                      const void *addr,
                      const int32_t num_bytes,
                      const uint64_t prop) {
    print_call("__csi_after_load", __csi_fed_get_load(load_id));
}

void __csi_before_store(const csi_id_t store_id,
                        const void *addr,
                        const int32_t num_bytes,
                        const uint64_t prop) {
    print_call("__csi_before_store", __csi_fed_get_store(store_id));
}

void __csi_after_store(const csi_id_t store_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop) {
    print_call("__csi_after_store", __csi_fed_get_store(store_id));
}

void __csi_func_entry(const csi_id_t func_id) {
    print_call("__csi_func_entry", __csi_fed_get_func(func_id));
}

void __csi_func_exit(const csi_id_t func_exit_id,
                     const csi_id_t func_id) {
    print_call("__csi_func_exit", __csi_fed_get_func_exit(func_exit_id));
}

void __csi_bb_entry(const csi_id_t bb_id) {
    print_call("__csi_bb_entry", __csi_fed_get_bb(bb_id));
}

void __csi_bb_exit(const csi_id_t bb_id) {
    print_call("__csi_bb_exit", __csi_fed_get_bb(bb_id));
}

void __csi_before_call(const csi_id_t callsite_id, const csi_id_t func_id) {
    print_call("__csi_before_call", __csi_fed_get_callsite(callsite_id));
    if (__csirt_is_callsite_target_unknown(callsite_id, func_id)) {
        printf("  (unknown target)\n");
    } else {
        source_loc_t const * func = __csi_fed_get_func(func_id);
        printf("  target: %s:%d\n", func->filename, func->line_number);
    }
}

void __csi_after_call(const csi_id_t callsite_id, const csi_id_t func_id) {
    print_call("__csi_after_call", __csi_fed_get_callsite(callsite_id));
    if (__csirt_is_callsite_target_unknown(callsite_id, func_id)) {
        printf("  (unknown target)\n");
    } else {
        source_loc_t const * func = __csi_fed_get_func(func_id);
        printf("  target: %s:%d\n", func->filename, func->line_number);
    }
}
