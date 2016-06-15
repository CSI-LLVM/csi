#include "../csi.h"

void destroy() {
    cilk_tool_destroy();
}

void __csi_init(csi_info_t info) {
    atexit(destroy);
    cilk_tool_init();
}

void __csi_func_entry(void *function, void *parentReturnAddr, char *funcName) {
    cilk_tool_c_function_enter(/*prop*/ 0, function, parentReturnAddr);
}

void __csi_func_exit() {
    // We pass 0 as rip because the cilkprof doesn't seem to need it. If that
    // changed, we'd have to create a stack and push the rip values from
    // func_entry, then use those and pop them off in this function.
    cilk_tool_c_function_leave(/*rip*/ 0);
}
