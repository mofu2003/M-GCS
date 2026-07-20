/**
 * param_file_io.cc - Parameter file I/O implementation
 * 参数文件 I/O 实现 — .param / CSV / XML 格式解析与导出
 */
#include "core/config/param_file_io.h"
#include <sstream>
#include <fstream>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace mg::core::config {

std::string ParamFileIO::export_param(const std::vector<ParamEntry>& params) {
    std::ostringstream oss;
    for (const auto& p : params) {
        oss << p.name << "," << p.value << "\n";
    }
    return oss.str();
}

std::vector<ParamEntry> ParamFileIO::import_param(const std::string& content) {
    std::vector<ParamEntry> result;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments

        size_t comma = line.find(',');
        if (comma == std::string::npos) continue;

        ParamEntry entry;
        entry.name = line.substr(0, comma);
        try {
            entry.value = std::stof(line.substr(comma + 1));
            result.push_back(entry);
        } catch (...) {
            // Skip malformed lines
        }
    }

    return result;
}

std::string ParamFileIO::export_csv(const std::vector<ParamEntry>& params) {
    std::ostringstream oss;
    oss << "name,value,type\n";
    for (const auto& p : params) {
        oss << p.name << "," << p.value << "," << (int)p.type << "\n";
    }
    return oss.str();
}

std::vector<ParamEntry> ParamFileIO::import_csv(const std::string& content) {
    std::vector<ParamEntry> result;
    std::istringstream stream(content);
    std::string line;

    // Skip header
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream line_stream(line);
        std::string name, value_str, type_str;

        std::getline(line_stream, name, ',');
        std::getline(line_stream, value_str, ',');
        std::getline(line_stream, type_str, ',');

        ParamEntry entry;
        entry.name = name;
        try {
            entry.value = std::stof(value_str);
            if (!type_str.empty()) entry.type = static_cast<uint8_t>(std::stoi(type_str));
            result.push_back(entry);
        } catch (...) {
            // Skip malformed lines
        }
    }

    return result;
}

std::string ParamFileIO::export_json(const std::vector<ParamEntry>& params) {
    cJSON* root = cJSON_CreateArray();

    for (const auto& p : params) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", p.name.c_str());
        cJSON_AddNumberToObject(item, "value", p.value);
        cJSON_AddNumberToObject(item, "type", p.type);
        cJSON_AddItemToArray(root, item);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);

    return result;
}

std::vector<ParamEntry> ParamFileIO::import_json(const std::string& content) {
    std::vector<ParamEntry> result;

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) return result;

    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return result;
    }

    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;

        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        cJSON* type = cJSON_GetObjectItemCaseSensitive(item, "type");

        if (name && cJSON_IsString(name) && value && cJSON_IsNumber(value)) {
            ParamEntry entry;
            entry.name = name->valuestring;
            entry.value = static_cast<float>(value->valuedouble);
            if (type && cJSON_IsNumber(type)) entry.type = static_cast<uint8_t>(type->valueint);
            result.push_back(entry);
        }
    }

    cJSON_Delete(root);
    return result;
}

std::vector<ParamEntry> ParamFileIO::filter_modified(
    const std::map<std::string, float>& current,
    const std::map<std::string, float>& defaults) {
    std::vector<ParamEntry> result;

    for (const auto& [name, value] : current) {
        auto it = defaults.find(name);
        if (it == defaults.end() || it->second != value) {
            result.push_back({name, value});
        }
    }

    return result;
}

std::map<std::string, float> ParamFileIO::merge(
    const std::map<std::string, float>& existing,
    const std::vector<ParamEntry>& imported) {
    auto result = existing;
    for (const auto& p : imported) {
        result[p.name] = p.value;
    }
    return result;
}

} // namespace mg::core::config
