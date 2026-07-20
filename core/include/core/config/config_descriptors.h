/**
 * config_descriptors.h - Preset configurations and utility functions
 * 配置描述符 — 预设参数集（命名配置方案）
 *
 * Parameter descriptors are now in ParameterDescriptors.json.
 * This file contains only preset configurations (named parameter sets).
 */
#pragma once

#include <string>
#include <vector>
#include <map>

namespace mg::core::config {

/// A named preset configuration
struct ConfigPreset {
    std::string name;
    std::string description;
    std::map<std::string, float> params;  // param_name -> value
};

/// Get all available presets
inline std::vector<ConfigPreset> get_all_presets() {
    std::vector<ConfigPreset> presets;

    // Standard Plane
    {
        ConfigPreset p;
        p.name = "standard_plane";
        p.description = "Standard fixed-wing configuration";
        p.params = {
            {"SERIAL1_PROTOCOL", 2},   // MAVLink2
            {"SERIAL1_BAUD", 100},     // 115200
            {"SERIAL2_PROTOCOL", 5},   // GPS
            {"SERIAL2_BAUD", 100},     // 115200
            {"SERVO1_FUNCTION", 74},   // MotorTilt (VTOL)
            {"SERVO2_FUNCTION", 4},    // Aileron
            {"SERVO3_FUNCTION", 19},   // ElevonRight
            {"SERVO4_FUNCTION", 7},    // Elevator
            {"SERVO5_FUNCTION", 6},    // Rudder
            {"SERVO9_FUNCTION", 77},   // LandingGear
        };
        presets.push_back(p);
    }

    // Standard Copter
    {
        ConfigPreset p;
        p.name = "standard_copter";
        p.description = "Standard quadcopter configuration";
        p.params = {
            {"SERIAL1_PROTOCOL", 2},   // MAVLink2
            {"SERIAL1_BAUD", 100},     // 115200
            {"SERIAL2_PROTOCOL", 5},   // GPS
            {"SERVO1_FUNCTION", 33},   // Motor1
            {"SERVO2_FUNCTION", 34},   // Motor2
            {"SERVO3_FUNCTION", 35},   // Motor3
            {"SERVO4_FUNCTION", 36},   // Motor4
        };
        presets.push_back(p);
    }

    // VTOL QuadPlane
    {
        ConfigPreset p;
        p.name = "vtol_quadplane";
        p.description = "VTOL QuadPlane configuration";
        p.params = {
            {"SERIAL1_PROTOCOL", 2},
            {"SERIAL1_BAUD", 100},
            {"SERIAL2_PROTOCOL", 5},
            {"SERVO1_FUNCTION", 74},   // MotorTilt
            {"SERVO2_FUNCTION", 4},    // Aileron
            {"SERVO3_FUNCTION", 19},   // ElevonRight
            {"SERVO4_FUNCTION", 7},    // Elevator
            {"SERVO5_FUNCTION", 6},    // Rudder
            {"SERVO13_FUNCTION", 8},   // Motor1 (quad)
            {"SERVO14_FUNCTION", 9},   // Motor2 (quad)
            {"SERVO15_FUNCTION", 10},  // Motor3 (quad)
            {"SERVO16_FUNCTION", 11},  // Motor4 (quad)
        };
        presets.push_back(p);
    }

    // Helicopter
    {
        ConfigPreset p;
        p.name = "helicopter";
        p.description = "Single-rotor helicopter configuration";
        p.params = {
            {"SERIAL1_PROTOCOL", 2},
            {"SERIAL1_BAUD", 100},
            {"SERIAL2_PROTOCOL", 5},
            {"SERVO1_FUNCTION", 79},   // HeliRSC
            {"SERVO3_FUNCTION", 4},    // Aileron (swashplate)
            {"SERVO4_FUNCTION", 7},    // Elevator (swashplate)
            {"SERVO6_FUNCTION", 80},   // HeliTailRSC
        };
        presets.push_back(p);
    }

    return presets;
}

} // namespace mg::core::config
