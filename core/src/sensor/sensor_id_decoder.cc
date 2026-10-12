/**
 * sensor_id_decoder.cc - Offline sensor device ID decoder
 * 传感器识别实现 — 解码 ArduPilot 设备 ID 位段
 *
 * ArduPilot bit layout (AP_HAL/Device.h via MP Device.cs):
 *   bus_type = devid & 0x7         (3 bits)
 *   bus      = (devid >> 3) & 0x1F (5 bits)
 *   address  = (devid >> 8) & 0xFF (8 bits)
 *   devtype  = (devid >> 16) & 0xFF (8 bits)
 */

#include "core/sensor/sensor_id_decoder.h"
#include "core/util/json.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace mg::core::sensor {

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

struct DevEntry {
    std::string name, sensor_type, family;
};

/// cJSON 解析产物的所有权：由本包装负责释放。
struct CJsonDeleter {
    void operator()(cJSON* node) const { if (node) cJSON_Delete(node); }
};
using CJsonPtr = std::unique_ptr<cJSON, CJsonDeleter>;

std::string string_field(const cJSON* object, const char* key) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) return "";
    return item->valuestring;
}

/// 读取一个 `<section>` 对象段落：`{ "<devtype>": { name, sensor, family }, ... }`。
/// 键沿用旧扫描器的 strtol(base 0) 解析；无名条目不入表（与旧行为一致）。
/// 返回该段落是否至少写入了一条（旧扫描器用 `tmp.empty()` 判断是否回退到旧键名）。
bool read_section(const cJSON* root, const char* section, std::map<int, DevEntry>& out) {
    const cJSON* node = cJSON_GetObjectItemCaseSensitive(root, section);
    if (!cJSON_IsObject(node)) return false;

    const size_t before = out.size();
    const cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, node) {
        if (entry->string == nullptr) continue;
        DevEntry e;
        e.name = string_field(entry, "name");
        e.sensor_type = string_field(entry, "sensor");
        e.family = string_field(entry, "family");
        if (e.name.empty()) continue;
        out[static_cast<int>(strtol(entry->string, nullptr, 0))] = e;
    }
    return out.size() > before;
}

/// 段落键名兼容：先试 primary，空则回退 legacy。
void read_section_compat(const cJSON* root, const char* primary, const char* legacy,
                         std::map<int, DevEntry>& out) {
    if (read_section(root, primary, out)) return;
    read_section(root, legacy, out);
}

}  // namespace

SensorIdDecoder::SensorIdDecoder(const std::string& json_path) {
    if (!json_path.empty()) loaded_ = load_json(json_path);
}

SensorIdDecoder::~SensorIdDecoder() = default;

bool SensorIdDecoder::load_json(const std::string& path) {
    const std::string json = read_file(path);
    if (json.empty()) return false;

    CJsonPtr root(mg::core::json::parse(json.c_str()));
    if (!root || !cJSON_IsObject(root.get())) return false;

    // ArduPilot tables (backward-compatible: try old key names first)
    std::map<int, DevEntry> tmp;

    read_section_compat(root.get(), "imu_devtypes", "ardupilot_imu_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_ins_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    read_section_compat(root.get(), "compass_devtypes", "ardupilot_compass_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_compass_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    read_section_compat(root.get(), "baro_devtypes", "ardupilot_baro_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_baro_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    // ArduPilot airspeed (ParamEntry style)
    {
        std::map<int, DevEntry> aspd_tmp;
        read_section_compat(root.get(), "airspeed_devtypes", "ardupilot_airspeed_devtypes", aspd_tmp);
        for (auto& [k, v] : aspd_tmp) ardu_airspeed_types_[k] = {v.name, v.family};
    }

    loaded_ = true;
    return true;
}

// ================================================================
// ArduPilot decoding
// ================================================================

DecodedSensor SensorIdDecoder::decode_ardupilot_id(int device_id, const std::string& category) const {
    DecodedSensor result;
    if (device_id == 0) return result;
    unsigned int d = static_cast<unsigned int>(device_id);
    result.bus_type = d & 0x7;
    result.bus      = (d >> 3) & 0x1F;
    result.address  = (d >> 8) & 0xFF;
    result.devtype  = (d >> 16) & 0xFF;

    const std::map<int, DeviceEntry>* table = nullptr;
    if (category == "ins" || category == "imu") table = &ardu_ins_devtypes_;
    else if (category == "compass" || category == "mag") table = &ardu_compass_devtypes_;
    else if (category == "baro") table = &ardu_baro_devtypes_;

    if (table) {
        auto it = table->find(result.devtype);
        if (it != table->end()) {
            result.name = it->second.name;
            result.sensor_type = it->second.sensor_type;
            result.family = it->second.family;
            result.valid = true;
        }
    }
    return result;
}

DecodedSensor SensorIdDecoder::decode_device_id(int device_id, const std::string& category) const {
    return decode_ardupilot_id(device_id, category);
}

// ================================================================
// Legacy airspeed decode (ArduPilot)
// ================================================================

DecodedParamSensor SensorIdDecoder::decode_type_param(int type_value, const std::string& category) const {
    DecodedParamSensor result;
    if (type_value < 0) return result;
    const std::map<int, ParamEntry>* table = nullptr;
    if (category == "airspeed") table = &ardu_airspeed_types_;
    if (table) {
        auto it = table->find(type_value);
        if (it != table->end()) {
            result.name = it->second.name;
            result.family = it->second.family;
            result.valid = true;
        }
    }
    return result;
}

} // namespace mg::core::sensor
