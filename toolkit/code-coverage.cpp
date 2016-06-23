#include "csi.h"
#include "bitset.h"
#include "vector.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>

namespace {

Bitset *bitset = NULL;
uint64_t total_num_basic_blocks = 0;

void report() {
    uint64_t num_covered = bitset->count();
    printf("Code coverage summary:\n");
    printf("%lu/%lu (%.02f%%) basic blocks executed.\n", num_covered, total_num_basic_blocks,
           ((float)num_covered/total_num_basic_blocks)*100);

    for (csi_id_t id = 0; id < total_num_basic_blocks; id++) {
        if (bitset->get(id) == false) {
            printf("Basic block ID %ld at %s:%d was not executed.\n", id,
                         __csi_get_bb_source_loc(id)->filename,
                         __csi_get_bb_source_loc(id)->line_number);
        }
    }
}
}

extern "C" {

void __csi_init() {
    atexit(report);
    bitset = new Bitset();
}

void __csi_unit_init(const char * const file_name,
                     const instrumentation_counts_t counts) {
    total_num_basic_blocks += counts.num_bb;
    bitset->expand(total_num_basic_blocks);
}

void __csi_bb_entry(const csi_id_t bb_id) {
    bitset->set(bb_id);
}

} // extern "C"
