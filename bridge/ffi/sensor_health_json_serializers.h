#ifndef MGCORE_SENSOR_HEALTH_JSON_SERIALIZERS_H
#define MGCORE_SENSOR_HEALTH_JSON_SERIALIZERS_H

#include "json_builder.h"

#include <array>
#include <cstring>
#include <vector>

namespace mg::bridge {

struct SensorHealthBitDef {
    uint32_t bit;
    const char* key;
    const char* label;
    const char* group;
};

inline const std::array<SensorHealthBitDef, 17>& sensor_health_bit_defs() {
    static const std::array<SensorHealthBitDef, 17> defs{{
        {1u, "gyro", "3D Gyroscope", "core"},
        {2u, "accel", "3D Accelerometer", "core"},
        {4u, "mag", "3D Magnetometer", "core"},
        {8u, "baro", "Barometer", "core"},
        {16u, "diff_pressure", "Differential Pressure", "airdata"},
        {32u, "gps", "GPS", "navigation"},
        {64u, "optical_flow", "Optical Flow", "navigation"},
        {128u, "vision_position", "Vision Position", "navigation"},
        {256u, "laser_position", "Laser Position", "navigation"},
        {512u, "ground_truth", "External Ground Truth", "navigation"},
        {1024u, "angular_rate_control", "Angular Rate Control", "control"},
        {2048u, "attitude_stabilization", "Attitude Stabilization", "control"},
        {4096u, "yaw_position", "Yaw Position", "control"},
        {8192u, "z_altitude", "Z Altitude Control", "control"},
        {16384u, "xy_position", "XY Position Control", "control"},
        {32768u, "motors", "Motor Outputs", "actuation"},
        {65536u, "rc", "RC Receiver", "io"},
    }};
    return defs;
}

inline const std::array<SensorHealthBitDef, 6>& extended_sensor_health_bit_defs() {
    static const std::array<SensorHealthBitDef, 6> defs{{
        {131072u, "gyro2", "Secondary Gyroscope", "redundancy"},
        {262144u, "accel2", "Secondary Accelerometer", "redundancy"},
        {524288u, "mag2", "Secondary Magnetometer", "redundancy"},
        {33554432u, "battery", "Battery Monitor", "power"},
        {67108864u, "proximity", "Proximity Sensor", "navigation"},
        {1073741824u, "propulsion", "Propulsion", "power"},
    }};
    return defs;
}

inline const char* sensor_health_status_name(bool present, bool enabled, bool healthy) {
    if (!present) return "missing";
    if (!enabled) return "disabled";
    return healthy ? "healthy" : "fault";
}

inline std::string sensor_health_item_json(const SensorHealthBitDef& def,
                                           uint32_t present_mask,
                                           uint32_t enabled_mask,
                                           uint32_t health_mask) {
    const bool present = (present_mask & def.bit) != 0;
    const bool enabled = (enabled_mask & def.bit) != 0;
    const bool healthy = (health_mask & def.bit) != 0;

    JsonBuilder jb;
    jb.begin_object();
    jb.field("key", def.key);
    jb.field("label", def.label);
    jb.field("group", def.group);
    jb.field("bit", static_cast<int64_t>(def.bit));
    jb.field("present", present);
    jb.field("enabled", enabled);
    jb.field("healthy", healthy);
    jb.field("status", sensor_health_status_name(present, enabled, healthy));
    jb.end_object();
    return jb.str();
}

inline std::string sensor_health_json(const mg::core::mavlink::SysStatusInfo& ss,
                                      uint8_t autopilot_type = 0) {
    const uint32_t present = ss.onboard_control_sensors_present;
    const uint32_t enabled = ss.onboard_control_sensors_enabled;
    const uint32_t health = ss.onboard_control_sensors_health;

    std::vector<std::string> all_items;
    std::vector<std::string> core_items;
    std::vector<std::string> nav_items;
    std::vector<std::string> control_items;
    std::vector<std::string> io_items;
    std::vector<std::string> power_items;
    std::vector<std::string> redundancy_items;

    int healthy_count = 0;
    int fault_count = 0;
    int missing_count = 0;
    int disabled_count = 0;

    auto collect_item = [&](const SensorHealthBitDef& def) {
        const bool bit_present = (present & def.bit) != 0;
        const bool bit_enabled = (enabled & def.bit) != 0;
        const bool bit_healthy = (health & def.bit) != 0;
        const std::string item_json = sensor_health_item_json(def, present, enabled, health);

        all_items.push_back(item_json);
        if (std::strcmp(def.group, "core") == 0 || std::strcmp(def.group, "airdata") == 0) {
            core_items.push_back(item_json);
        } else if (std::strcmp(def.group, "navigation") == 0) {
            nav_items.push_back(item_json);
        } else if (std::strcmp(def.group, "control") == 0 || std::strcmp(def.group, "actuation") == 0) {
            control_items.push_back(item_json);
        } else if (std::strcmp(def.group, "io") == 0) {
            io_items.push_back(item_json);
        } else if (std::strcmp(def.group, "power") == 0) {
            power_items.push_back(item_json);
        } else if (std::strcmp(def.group, "redundancy") == 0) {
            redundancy_items.push_back(item_json);
        }

        if (!bit_present) {
            ++missing_count;
        } else if (!bit_enabled) {
            ++disabled_count;
        } else if (bit_healthy) {
            ++healthy_count;
        } else {
            ++fault_count;
        }
    };

    for (const auto& def : sensor_health_bit_defs()) {
        collect_item(def);
    }
    for (const auto& def : extended_sensor_health_bit_defs()) {
        collect_item(def);
    }

    const char* firmware_name = "generic";
    if (autopilot_type == 3) firmware_name = "ardupilot";
    else if (autopilot_type == 12) firmware_name = "px4";

    auto list_to_json = [](const std::vector<std::string>& items) {
        JsonBuilder jb;
        jb.begin_array();
        for (const auto& item : items) {
            jb.append_json(item);
        }
        jb.end_array();
        return jb.str();
    };
    auto init_list_to_json = [&](std::initializer_list<std::string> items) {
        JsonBuilder jb;
        jb.begin_array();
        for (const auto& item : items) {
            jb.append_json(item);
        }
        jb.end_array();
        return jb.str();
    };

    JsonBuilder summary;
    summary.begin_object();
    summary.field("healthy_count", healthy_count);
    summary.field("fault_count", fault_count);
    summary.field("missing_count", missing_count);
    summary.field("disabled_count", disabled_count);
    summary.field("overall_status", fault_count > 0 ? "fault" : (healthy_count > 0 ? "healthy" : "unknown"));
    summary.end_object();

    JsonBuilder groups;
    groups.begin_object();
    groups.raw_field("core", list_to_json(core_items));
    groups.raw_field("navigation", list_to_json(nav_items));
    groups.raw_field("control", list_to_json(control_items));
    groups.raw_field("io", list_to_json(io_items));
    groups.raw_field("power", list_to_json(power_items));
    groups.raw_field("redundancy", list_to_json(redundancy_items));
    groups.end_object();

    JsonBuilder firmware_view;
    firmware_view.begin_object();
    firmware_view.field("profile", firmware_name);
    if (autopilot_type == 3) {
        firmware_view.raw_field("ardupilot", init_list_to_json({
            sensor_health_item_json(sensor_health_bit_defs()[0], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[1], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[2], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[3], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[5], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[15], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[16], present, enabled, health),
            sensor_health_item_json(extended_sensor_health_bit_defs()[3], present, enabled, health),
        }));
    } else if (autopilot_type == 12) {
        firmware_view.raw_field("px4", init_list_to_json({
            sensor_health_item_json(sensor_health_bit_defs()[0], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[1], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[2], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[5], present, enabled, health),
            sensor_health_item_json(sensor_health_bit_defs()[15], present, enabled, health),
            sensor_health_item_json(extended_sensor_health_bit_defs()[3], present, enabled, health),
        }));
    } else {
        firmware_view.raw_field("generic", list_to_json(all_items));
    }
    firmware_view.end_object();

    JsonBuilder jb;
    jb.begin_object();
    jb.field("present", static_cast<int64_t>(present));
    jb.field("enabled", static_cast<int64_t>(enabled));
    jb.field("health",  static_cast<int64_t>(health));
    jb.raw_field("summary", summary.str());
    jb.raw_field("groups", groups.str());
    jb.raw_field("items", list_to_json(all_items));
    jb.raw_field("firmware_view", firmware_view.str());
    jb.end_object();
    return jb.str();
}

} // namespace mg::bridge

#endif /* MGCORE_SENSOR_HEALTH_JSON_SERIALIZERS_H */
