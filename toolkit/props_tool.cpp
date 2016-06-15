#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {
int a, b, c, d;

void print() {
    printf("%d %d %d %d\n", a, b, c, d);
}

// void __csi_init(csi_info_t info) {
void __csi_init(uint32_t num_modules) {
    atexit(print);
}

// void __csi_before_load(void *addr, int num_bytes, csi_acc_prop_t prop) {
void __csi_before_load(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
    if (!read_before_write_in_bb)
        a++;
}

// void __csi_before_store(void *addr, int num_bytes, csi_acc_prop_t prop) {
void __csi_before_store(void *addr, int num_bytes, unsigned unused, bool unused2, bool unused3, bool read_before_write_in_bb) {
    c++;
}

} // extern "C"

