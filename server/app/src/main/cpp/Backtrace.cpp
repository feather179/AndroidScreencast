
#include "Backtrace.h"

#include <iostream>
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <android/log.h>

#define LOG_TAG "Backtrace"

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context *context, void *arg) {
    char symbol[1024];
    char filename[1024];
    int *count_ptr = reinterpret_cast<int *>(arg);

    auto pc = (uintptr_t) _Unwind_GetIP(context);
    if (pc == 0) {
        return _URC_NO_REASON;
    }

    Dl_info dl_info;
    if (dladdr((void *) pc, &dl_info) && dl_info.dli_sname) {
        int status;
        const char *realname = abi::__cxa_demangle(dl_info.dli_sname, nullptr, nullptr, &status);
        if (realname) {
            snprintf(symbol, sizeof(symbol), "pc:%p %s",
                     (void *) (pc - (uintptr_t) dl_info.dli_fbase), realname);
            free((void *) realname);
        } else {
            snprintf(symbol, sizeof(symbol), "pc:%p %s",
                     (void *) (pc - (uintptr_t) dl_info.dli_fbase), dl_info.dli_sname);
        }
    } else {
        snprintf(symbol, sizeof(symbol), "pc:%p", (void *) (pc - (uintptr_t) dl_info.dli_fbase));
    }

    if (dl_info.dli_fname) {
        snprintf(filename, sizeof(filename), "%s", dl_info.dli_fname);
    } else {
        snprintf(filename, sizeof(filename), "unknown");
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "#%02d %s: %s", *count_ptr, filename, symbol);

    *count_ptr += 1;

    return _URC_NO_REASON;
}

void dump_backtrace() {
    int count = 0;
    _Unwind_Backtrace(unwindCallback, &count);
}
