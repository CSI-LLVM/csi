#include "csi.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {

extern void mcount();

void __csi_func_entry(void *function, void *parentReturnAddr, char *funcName) {
    mcount();
}

} // extern "C"
