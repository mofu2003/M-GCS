#define MGCORE_EXPORTS
/**
 * ffi_utility.cc - Section 8: Utility
 *
 * 工具函数：内存释放、版本号查询。
 *
 * mg_free_string, mg_get_version
 */
#include "ffi_internal.h"

MGCORE_API void mg_free_string(char* str) {
    if (str) free(str);
}

MGCORE_API char* mg_get_version(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d.%d.%d",
             MGCORE_VERSION_MAJOR, MGCORE_VERSION_MINOR, MGCORE_VERSION_PATCH);
    return to_cstr(buf);
}
