#include "csi.h"
#include <stdio.h>
#include <stdlib.h>
#include <set>

namespace {

std::set<csi_id_t> *load_ids = nullptr;
std::set<csi_id_t> *store_ids = nullptr;
csi_id_t total_num_loads = 0;
csi_id_t total_num_stores = 0;

void report() {
    fprintf(stderr, "\n============= Memory Tracer report =============\n");
    fprintf(stderr, "Executed %lu/%ld loads.\n", 
            load_ids->size(), total_num_loads);
    fprintf(stderr, "Executed %lu/%ld stores.\n", 
            store_ids->size(), total_num_stores);
    for(csi_id_t id = 0; id < total_num_loads; id++) {
        fprintf(stderr, "Load ID %ld at %s:%d was executed %zu times.\n",
                id, __csi_fed_get_load(id)->filename, 
                __csi_fed_get_load(id)->line_number, load_ids->count(id)); 
    }
    for(csi_id_t id = 0; id < total_num_stores; id++) {
        fprintf(stderr, "Store ID %ld at %s:%d was executed %zu times.\n",
                id, __csi_fed_get_store(id)->filename, 
                __csi_fed_get_store(id)->line_number, store_ids->count(id)); 
    }
    fprintf(stderr, "\n");

    delete load_ids;
    delete store_ids;
}

void init() {
    load_ids = new std::set<csi_id_t>();
    store_ids = new std::set<csi_id_t>();
    atexit(report);
}
}

extern "C" {

void __csi_init() {
    fprintf(stderr, "Initializing the tool.\n");
    init();
}

void __csi_unit_init(const char * const file_name,
                     const instrumentation_counts_t counts) {
    total_num_loads += counts.num_load;
    total_num_stores += counts.num_store;
}

void __csi_before_load(const csi_id_t load_id,
                       const void *addr,
                       const int32_t num_bytes,
                       const uint64_t prop){
    fprintf(stderr, "loading address %p, %d bytes (%s:%d)\n",
            addr, num_bytes,
            __csi_fed_get_load(load_id)->filename,
            __csi_fed_get_load(load_id)->line_number);
    load_ids->insert(load_id);
}

void __csi_before_store(const csi_id_t store_id,
                        const void *addr,
                        const int32_t num_bytes,
                        const uint64_t prop) {
    fprintf(stderr, "storing address %p, %d bytes (%s:%d)\n",
            addr, num_bytes,
            __csi_fed_get_store(store_id)->filename,
            __csi_fed_get_store(store_id)->line_number);
    store_ids->insert(store_id);
}

} // extern "C"
