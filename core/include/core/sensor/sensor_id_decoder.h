/**
 * sensor_id_decoder.h - Offline sensor device ID decoder
 * 传感器识别 — 离线解码 ArduPilot 传感器设备 ID
 *
 * Loads SensorDeviceInfo.json at construction time.
 * Decodes ArduPilot device IDs into human-readable sensor names.
 * Zero network dependency.
 *
 * ArduPilot encoding:
 *   bus_type = devid & 0x7         (3 bits)
 *   bus      = (devid >> 3) & 0x1F (5 bits)
 *   address  = (devid >> 8) & 0xFF (8 bits)
 *   devtype  = (devid >> 16) & 0xFF (8 bits)
 */

#pragma once

#include <string>
#include <map>

namespace mg::core::sensor {

struct DecodedSensor {
    std::string name;
    std::string sensor_type;
    std::string family;
    int bus = 0;
    int address = 0;
    int devtype = 0;
    int bus_type = 0;
    bool valid = false;
};

struct DecodedParamSensor {
    std::string name;
    std::string family;
    bool valid = false;
};

class SensorIdDecoder {
public:
    explicit SensorIdDecoder(const std::string& json_path = "");
    ~SensorIdDecoder();

    /// Decode a device ID using ArduPilot encoding
    DecodedSensor decode_ardupilot_id(int device_id, const std::string& category) const;

    /// Decode a device ID (ArduPilot encoding)
    DecodedSensor decode_device_id(int device_id, const std::string& category) const;

    /// Legacy: decode type parameter (airspeed only, ArduPilot)
    DecodedParamSensor decode_type_param(int type_value, const std::string& category) const;

    bool is_loaded() const { return loaded_; }

private:
    bool load_json(const std::string& path);

    struct DeviceEntry { std::string name, sensor_type, family; };
    struct ParamEntry { std::string name, family; };

    // ArduPilot tables (keyed by devtype)
    std::map<int, DeviceEntry> ardu_ins_devtypes_;
    std::map<int, DeviceEntry> ardu_compass_devtypes_;
    std::map<int, DeviceEntry> ardu_baro_devtypes_;
    std::map<int, ParamEntry>  ardu_airspeed_types_;

    bool loaded_ = false;
};

} // namespace mg::core::sensor
