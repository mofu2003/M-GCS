/**
 * param_file_io.h - Parameter file import/export
 * 参数文件 I/O — 支持 .param / CSV / XML 格式的导入导出
 *
 * Supports ArduPilot MissionPlanner .param format:
 *   PARAM_NAME,VALUE
 *   PARAM_NAME,VALUE
 *
 * Also supports .param.xml and CSV formats.
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace mg::core::config {

/// A single parameter entry for file I/O
struct ParamEntry {
    std::string name;
    float value;
    uint8_t type = 9;  // MAV_PARAM_TYPE_REAL32 default
};

class ParamFileIO {
public:
    /// Export parameters to .param format (MissionPlanner compatible)
    static std::string export_param(const std::vector<ParamEntry>& params);

    /// Import parameters from .param format
    static std::vector<ParamEntry> import_param(const std::string& content);

    /// Export parameters to CSV format
    static std::string export_csv(const std::vector<ParamEntry>& params);

    /// Import parameters from CSV format
    static std::vector<ParamEntry> import_csv(const std::string& content);

    /// Export parameters to JSON format
    static std::string export_json(const std::vector<ParamEntry>& params);

    /// Import parameters from JSON format
    static std::vector<ParamEntry> import_json(const std::string& content);

    /// Export only modified parameters (compare with defaults)
    static std::vector<ParamEntry> filter_modified(
        const std::map<std::string, float>& current,
        const std::map<std::string, float>& defaults);

    /// Merge imported parameters with existing (for partial restore)
    static std::map<std::string, float> merge(
        const std::map<std::string, float>& existing,
        const std::vector<ParamEntry>& imported);
};

} // namespace mg::core::config
