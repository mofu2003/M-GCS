/**
 * sensor_id_decoder.h - Offline sensor device ID decoder
 * 传感器识别 — 离线解码 ArduPilot/PX4 传感器设备 ID
 *
 * Loads SensorDeviceInfo.json at construction time.
 * Decodes ArduPilot and PX4 device IDs into human-readable sensor names.
 * Zero network dependency.
 *
 * ArduPilot encoding:
 *   bus_type = devid & 0x7         (3 bits)
 *   bus      = (devid >> 3) & 0x1F (5 bits)
 *   address  = (devid >> 8) & 0xFF (8 bits)
 *   devtype  = (devid >> 16) & 0xFF (8 bits)
 *
 * PX4 encoding:
 *   address    = devid & 0xFF         (8 bits)
 *   bus        = (devid >> 8) & 0xFF  (8 bits)
 *   bus_type   = (devid >> 16) & 0xFF (8 bits)
 *   devtype    = (devid >> 24) & 0xFF (8 bits)
 */

#pragma once

#include <string>
#include <map>

namespace mg::core::sensor {

/// Firmware family for sensor ID decoding
enum class SensorIdFirmware {
    Auto,         // Auto-detect from ID patterns (try PX4 first if high bits set)
    ArduPilot,
    PX4,
};

struct DecodedSensor {
    std::string name;
    std::string sensor_type;
    std::string family;
    int bus = 0;
    int address = 0;
    int devtype = 0;
    int bus_type = 0;
    bool valid = false;
    SensorIdFirmware detected_firmware = SensorIdFirmware::ArduPilot;
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

    /// Decode a device ID using PX4 encoding
    DecodedSensor decode_px4_id(int device_id, const std::string& category) const;

    /// Decode a device ID, auto-detecting firmware if needed
    DecodedSensor decode_device_id(int device_id, const std::string& category) const;

    /// Decode a PX4 type parameter value (direct lookup by devtype)
    DecodedSensor decode_px4_type_value(int devtype, const std::string& category) const;

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

    // PX4 tables (keyed by devtype)
    std::map<int, DeviceEntry> px4_imu_devtypes_;
    std::map<int, DeviceEntry> px4_mag_devtypes_;
    std::map<int, DeviceEntry> px4_baro_devtypes_;
    std::map<int, DeviceEntry> px4_dp_devtypes_;

    bool loaded_ = false;
};

} // namespace mg::core::sensor
