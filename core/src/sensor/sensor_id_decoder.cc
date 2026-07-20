/**
 * sensor_id_decoder.cc - Offline sensor device ID decoder
 * 传感器识别实现 — 解码 ArduPilot/PX4 设备 ID 位段
 *
 * Supports both ArduPilot and PX4 device ID encoding.
 *
 * ArduPilot bit layout (AP_HAL/Device.h via MP Device.cs):
 *   bus_type = devid & 0x7         (3 bits)
 *   bus      = (devid >> 3) & 0x1F (5 bits)
 *   address  = (devid >> 8) & 0xFF (8 bits)
 *   devtype  = (devid >> 16) & 0xFF (8 bits)
 *
 * PX4 bit layout (px4_platforms/devices.h):
 *   address  = devid & 0xFF         (8 bits)
 *   bus      = (devid >> 8) & 0xFF  (8 bits)
 *   bus_type = (devid >> 16) & 0xFF (8 bits)
 *   devtype  = (devid >> 24) & 0xFF (8 bits)
 */

#include "core/sensor/sensor_id_decoder.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace mg::core::sensor {

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static std::string find_str(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' &&
           json[end] != ' ' && json[end] != '\n' && json[end] != '\r') end++;
    return json.substr(pos, end - pos);
}

struct DevEntry {
    std::string name, sensor_type, family;
};

static void parse_section(const std::string& json, const std::string& section,
                           std::map<int, DevEntry>& out) {
    size_t sec_pos = json.find("\"" + section + "\"");
    if (sec_pos == std::string::npos) return;
    size_t obj_start = json.find('{', sec_pos);
    if (obj_start == std::string::npos) return;
    size_t pos = obj_start + 1;
    int depth = 1;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') { depth--; if (depth == 0) break; }
        else if (json[pos] == '"') {
            size_t kstart = pos + 1, kend = json.find('"', kstart);
            if (kend == std::string::npos) break;
            int key_int = (int)strtol(json.substr(kstart, kend - kstart).c_str(), nullptr, 0);
            size_t inner_start = json.find('{', kend);
            if (inner_start == std::string::npos) { pos = kend + 1; continue; }
            size_t inner_end = json.find('}', inner_start);
            if (inner_end == std::string::npos) break;
            std::string inner = json.substr(inner_start, inner_end - inner_start + 1);
            DevEntry e;
            e.name = find_str(inner, "name");
            e.sensor_type = find_str(inner, "sensor");
            e.family = find_str(inner, "family");
            if (!e.name.empty()) out[key_int] = e;
            pos = inner_end + 1; continue;
        }
        pos++;
    }
}

SensorIdDecoder::SensorIdDecoder(const std::string& json_path) {
    if (!json_path.empty()) loaded_ = load_json(json_path);
}

SensorIdDecoder::~SensorIdDecoder() = default;

bool SensorIdDecoder::load_json(const std::string& path) {
    std::string json = read_file(path);
    if (json.empty()) return false;

    // ArduPilot tables (backward-compatible: try old key names first)
    std::map<int, DevEntry> tmp;
    
    parse_section(json, "imu_devtypes", tmp);
    if (tmp.empty()) parse_section(json, "ardupilot_imu_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_ins_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    parse_section(json, "compass_devtypes", tmp);
    if (tmp.empty()) parse_section(json, "ardupilot_compass_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_compass_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    parse_section(json, "baro_devtypes", tmp);
    if (tmp.empty()) parse_section(json, "ardupilot_baro_devtypes", tmp);
    for (auto& [k, v] : tmp) ardu_baro_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    // ArduPilot airspeed (ParamEntry style)
    {
        std::map<int, DevEntry> aspd_tmp;
        parse_section(json, "airspeed_devtypes", aspd_tmp);
        if (aspd_tmp.empty()) parse_section(json, "ardupilot_airspeed_devtypes", aspd_tmp);
        for (auto& [k, v] : aspd_tmp) ardu_airspeed_types_[k] = {v.name, v.family};
    }

    // PX4 tables
    parse_section(json, "px4_imu_devtypes", tmp);
    for (auto& [k, v] : tmp) px4_imu_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    parse_section(json, "px4_mag_devtypes", tmp);
    for (auto& [k, v] : tmp) px4_mag_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    parse_section(json, "px4_baro_devtypes", tmp);
    for (auto& [k, v] : tmp) px4_baro_devtypes_[k] = {v.name, v.sensor_type, v.family};
    tmp.clear();

    parse_section(json, "px4_differential_pressure_devtypes", tmp);
    for (auto& [k, v] : tmp) px4_dp_devtypes_[k] = {v.name, v.sensor_type, v.family};

    loaded_ = true;
    return true;
}

// ================================================================
// ArduPilot decoding
// ================================================================

DecodedSensor SensorIdDecoder::decode_ardupilot_id(int device_id, const std::string& category) const {
    DecodedSensor result;
    result.detected_firmware = SensorIdFirmware::ArduPilot;
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

// ================================================================
// PX4 decoding
// ================================================================

DecodedSensor SensorIdDecoder::decode_px4_id(int device_id, const std::string& category) const {
    DecodedSensor result;
    result.detected_firmware = SensorIdFirmware::PX4;
    if (device_id == 0) return result;
    unsigned int d = static_cast<unsigned int>(device_id);

    const std::map<int, DeviceEntry>* table = nullptr;
    if (category == "accel" || category == "imu" || category == "gyro") table = &px4_imu_devtypes_;
    else if (category == "mag" || category == "compass") table = &px4_mag_devtypes_;
    else if (category == "baro") table = &px4_baro_devtypes_;
    else if (category == "diff_pres" || category == "airspeed") table = &px4_dp_devtypes_;

    // Strategy 1: Standard PX4 encoding (sensors.hpp)
    //   bits[7:0]   = address
    //   bits[15:8]  = bus
    //   bits[23:16] = bus_type (BusType enum: 1=I2C, 4=SPI, 5=UAVCAN)
    //   bits[31:24] = devtype (DeviceType enum)
    result.address  = d & 0xFF;
    result.bus      = (d >> 8) & 0xFF;
    result.bus_type = (d >> 16) & 0xFF;
    result.devtype  = (d >> 24) & 0xFF;

    if (table) {
        auto it = table->find(result.devtype);
        if (it != table->end()) {
            result.name = it->second.name;
            result.sensor_type = it->second.sensor_type;
            result.family = it->second.family;
            result.valid = true;
            return result;
        }
    }

    // Strategy 2: Some PX4 builds use ArduPilot-compatible encoding
    //   bits[2:0]   = bus_type (BusType: 0=I2C, 2=SPI)
    //   bits[7:3]   = bus number
    //   bits[15:8]  = address
    //   bits[23:16] = devtype (device class specific)
    unsigned int ap_bus_type = d & 0x7;
    unsigned int ap_bus_num  = (d >> 3) & 0x1F;
    unsigned int ap_addr    = (d >> 8) & 0xFF;
    unsigned int ap_devtype = (d >> 16) & 0xFF;

    // Only use AP decode if bus_type field is a known AP bus type (0=I2C, 2=SPI, 3=CAN)
    if (ap_bus_type <= 3 && ap_devtype > 0) {
        // Check both PX4 and AP tables for the devtype
        if (table) {
            auto it = table->find(ap_devtype);
            if (it != table->end()) {
                result.devtype  = ap_devtype;
                result.bus_type = ap_bus_type;
                result.bus      = ap_bus_num;
                result.address  = ap_addr;
                result.name = it->second.name;
                result.sensor_type = it->second.sensor_type;
                result.family = it->second.family;
                result.valid = true;
                return result;
            }
        }
        // Also check ArduPilot tables (some PX4 builds use AP-compatible devtypes)
        const std::map<int, DeviceEntry>* ap_table = nullptr;
        if (category == "accel" || category == "imu" || category == "gyro") ap_table = &ardu_ins_devtypes_;
        else if (category == "mag" || category == "compass") ap_table = &ardu_compass_devtypes_;
        else if (category == "baro") ap_table = &ardu_baro_devtypes_;
        if (ap_table) {
            auto it = ap_table->find(ap_devtype);
            if (it != ap_table->end()) {
                result.devtype  = ap_devtype;
                result.bus_type = ap_bus_type;
                result.bus      = ap_bus_num;
                result.address  = ap_addr;
                result.name = it->second.name;
                result.sensor_type = it->second.sensor_type;
                result.family = it->second.family;
                result.valid = true;
                return result;
            }
        }
        // Record decoded values even without match (for diagnostics)
        result.devtype  = ap_devtype;
        result.bus_type = ap_bus_type;
        result.bus      = ap_bus_num;
        result.address  = ap_addr;
    }

    return result;
}

// ================================================================
// Auto-detect: try PX4 first if high bits suggest PX4 encoding,
// otherwise try ArduPilot
// ================================================================

DecodedSensor SensorIdDecoder::decode_device_id(int device_id, const std::string& category) const {
    // Try PX4 first if device_id is large (PX4 puts devtype in upper byte)
    if (static_cast<unsigned int>(device_id) > 0xFFFFFF) {
        auto px4_result = decode_px4_id(device_id, category);
        if (px4_result.valid) return px4_result;
        // Fall through to ArduPilot if PX4 lookup failed
    }
    auto ardu_result = decode_ardupilot_id(device_id, category);
    if (ardu_result.valid) return ardu_result;

    // Both failed - return the ArduPilot-decoded bits (more common case)
    return decode_ardupilot_id(device_id, category);
}

// ================================================================
// PX4 type value direct lookup (for SENS_*_ID parameters that
// may contain just the devtype, not the full devid)
// ================================================================

DecodedSensor SensorIdDecoder::decode_px4_type_value(int devtype, const std::string& category) const {
    DecodedSensor result;
    result.detected_firmware = SensorIdFirmware::PX4;
    result.devtype = devtype;

    const std::map<int, DeviceEntry>* table = nullptr;
    if (category == "accel" || category == "imu" || category == "gyro") table = &px4_imu_devtypes_;
    else if (category == "mag" || category == "compass") table = &px4_mag_devtypes_;
    else if (category == "baro") table = &px4_baro_devtypes_;
    else if (category == "diff_pres" || category == "airspeed") table = &px4_dp_devtypes_;

    if (table) {
        auto it = table->find(devtype);
        if (it != table->end()) {
            result.name = it->second.name;
            result.sensor_type = it->second.sensor_type;
            result.family = it->second.family;
            result.valid = true;
        }
    }
    return result;
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
