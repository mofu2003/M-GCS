/**
 * platform_filesystem.h - 跨平台文件系统工具
 *
 * Provides platform-abstracted file operations used by the core engine.
 * No UI dependencies. Used by bridge layer and core modules.
 *
 * v0.18: Extracted from ffi_connection.cc (find_sensor_db_path).
 */
#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace mg::core::platform {

/// 获取当前可执行文件/DLL 所在目录，返回带尾部分隔符的路径，失败返回空字符串
/// Used to locate data files (USBBoardInfo.json, SensorDeviceInfo.json).
std::string get_module_directory();

/// 在模块目录下查找数据文件，找到返回完整路径，未找到返回空字符串
/// Example: find_data_file("SensorDeviceInfo.json")
std::string find_data_file(const char* filename);

/// 查找随程序分发的数据文件，能识别 macOS 应用包的目录结构。
///
/// macOS 应用包里数据文件放在 `Contents/Resources/`，可执行文件在
/// `Contents/MacOS/`；Windows/Linux 构建把数据文件放在模块目录里。
/// 按候选顺序返回第一个真实存在的路径，都没有时返回空字符串。
inline std::string find_bundled_data_file(const char* filename) {
    if (!filename || filename[0] == '\0') return "";
    const std::vector<std::string> candidates = {
        get_module_directory() + "../Resources/" + filename,  // macOS app bundle
        find_data_file(filename),                             // next to the module
        filename,
    };
    for (const auto& path : candidates) {
        if (path.empty()) continue;
        std::ifstream probe(path, std::ios::binary);
        if (probe.is_open()) return path;
    }
    return "";
}

} // namespace mg::core::platform
