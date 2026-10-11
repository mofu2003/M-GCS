#ifndef MGCORE_CORE_UTIL_JSON_H
#define MGCORE_CORE_UTIL_JSON_H

// 线程安全的 cJSON 解析入口。
//
// cJSON 的每个解析入口都会写一份进程级全局错误缓冲（`cJSON.c` 里的
// `global_error`，由 `cJSON_GetErrorPtr` 读取）。两个线程同时解析就会竞争这块
// 全局状态，即使两份 JSON 毫无关系。M-GCS 会在 FFI 调用线程和异步命令工作线程
// 上同时解析，所以这里把解析入口串行化。
//
// 只锁"解析"这一步：解析产物（cJSON 对象）彼此独立，后续的
// cJSON_GetObjectItem / cJSON_Print / cJSON_Delete 都不需要这个锁。
// cJSON 1.7.18 没有提供关闭全局错误缓冲的编译开关，`third_party/` 不可修改，
// 因此由本包装负责。项目内不再直接调用 `cJSON_Parse`。

#include <mutex>

#include "cJSON.h"

namespace mg::core::json {

inline cJSON* parse(const char* text) {
    static std::mutex parse_mutex;
    std::lock_guard<std::mutex> lock(parse_mutex);
    return cJSON_Parse(text);
}

}  // namespace mg::core::json

#endif  // MGCORE_CORE_UTIL_JSON_H
