#include "csi.h"
#include <stdio.h>

void __csi_before_callsite(uint64_t csi_id, uint64_t func_id) {
    printf("Before callsite at %s:%d to ", __csirt_get_filename(csi_id),
               __csirt_get_line_number(csi_id));
    if (__csirt_is_callsite_target_unknown(csi_id, func_id)) {
        printf("unknown target\n");
    } else {
        printf("target at %s:%d\n", __csirt_get_filename(func_id),
               __csirt_get_line_number(func_id));
    }
}
