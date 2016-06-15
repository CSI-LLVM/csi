#include <stdio.h>
#include <stdint.h>

extern "C" {
void __csi_init(const char * const name,
                const uint32_t num_units) {
    printf("init: %s. num units: %u\n", name, num_units);
}

void __csi_unit_init(const char * const file_name,
                     const uint64_t num_basic_blocks) {
    printf("unit init: %s. num bb: %lu\n", file_name, num_basic_blocks);
}

void __csi_bb_entry(const uint64_t bb_id) {
    uint64_t unit = bb_id >> 32,
        offset = bb_id & ((1ULL << 32) - 1);
    printf("enter BB %lu:%lu\n", unit, offset);
}

void __csi_bb_exit(const uint64_t bb_id) {
    printf("exit BB\n");
}

}
