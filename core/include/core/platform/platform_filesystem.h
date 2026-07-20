/**
 * platform_filesystem.h - 跨平台文件系统工具
 *
 * Provides platform-abstracted file operations used by the core engine.
 * No UI dependencies. Used by bridge layer and core modules.
 *
 * v0.18: Extracted from ffi_connection.cc (find_sensor_db_path).
 */
#pragma once

#include <string>

namespace mg::core::platform {

/// 获取当前可执行文件/DLL 所在目录，返回带尾部分隔符的路径，失败返回空字符串
/// Used to locate data files (USBBoardInfo.json, SensorDeviceInfo.json).
std::string get_module_directory();

/// 在模块目录下查找数据文件，找到返回完整路径，未找到返回空字符串
/// Example: find_data_file("SensorDeviceInfo.json")
std::string find_data_file(const char* filename);

} // namespace mg::core::platform