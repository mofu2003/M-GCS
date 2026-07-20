/**
 * platform_filesystem_win32.cc - Win32 implementation of filesystem utilities
 * Win32 文件系统实现 — 模块路径、数据文件查找
 */
#include "core/platform/platform_filesystem.h"

#ifdef _WIN32
#include <windows.h>

namespace mg::core::platform {

std::string get_module_directory() {
    char buf[MAX_PATH] = {};
    HMODULE hm = nullptr;
    // Get the DLL handle from the current function's address
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&get_module_directory), &hm);
    GetModuleFileNameA(hm, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) return path.substr(0, pos + 1);
    return ".\\";
}

std::string find_data_file(const char* filename) {
    std::string dir = get_module_directory();
    if (dir.empty()) return "";
    return dir + filename;
}

} // namespace mg::core::platform
#endif