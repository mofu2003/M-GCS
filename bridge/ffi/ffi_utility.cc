#define MGCORE_EXPORTS
/**
 * ffi_utility.cc - Section 8: Utility
 *
 * 工具函数：返回字符串的内存释放。
 *
 * mg_free_string
 */
#include "ffi_internal.h"

MGCORE_API void mg_free_string(char* str) {
    if (str) free(str);
}
