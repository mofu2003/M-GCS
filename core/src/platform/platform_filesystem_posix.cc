/**
 * platform_filesystem_posix.cc - POSIX implementation of filesystem utilities
 * POSIX 文件系统实现 — Linux/macOS 模块路径与数据文件查找
 */
#include "core/platform/platform_filesystem.h"

#if !defined(_WIN32) && !defined(__ANDROID__)
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace mg::core::platform {

std::string get_module_directory() {
    char buf[PATH_MAX] = {};
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::string path(buf);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) return path.substr(0, pos + 1);
    }
#else
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) return path.substr(0, pos + 1);
    }
#endif
    return "./";
}

std::string find_data_file(const char* filename) {
    std::string dir = get_module_directory();
    if (dir.empty()) return "";
    return dir + filename;
}

} // namespace mg::core::platform
#endif
