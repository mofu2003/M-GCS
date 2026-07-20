/**
 * param_remapping.cc - Parameter name version remapping implementation
 * 参数重映射实现 — 根据固件版本翻译参数名
 *
 * Remap tables sourced from QGC:
 *   ArduCopterFirmwarePlugin.cc (lines 80-160) — 4.0 and 4.7 renames
 *   ArduPlaneFirmwarePlugin.cc — 4.7 renames (subset)
 *   ArduRoverFirmwarePlugin.cc — 4.7 renames (subset)
 */

#include "core/firmware/param_remapping.h"
#include <algorithm>

namespace mg::core::firmware {

ParamRemapping::ParamRemapping() = default;
ParamRemapping::~ParamRemapping() = default;

void ParamRemapping::add(int major, int minor, const char* new_name, const char* old_name) {
    remap_table_[major][minor][new_name] = old_name;
}

std::string ParamRemapping::remap(int major, int minor, const std::string& param_name) const {
    // Unknown firmware version — return unchanged
    if (major == 0 && minor == 0) return param_name;

    auto major_it = remap_table_.find(major);
    if (major_it == remap_table_.end()) return param_name;

    const auto& minor_map = major_it->second;

    // Walk backwards from highest known minor version down to vehicle's actual version.
    // At each step, if the name matches a "new_name", translate to "old_name".
    // This handles chains: name_a -> name_b -> name_c across multiple versions.
    std::string current = param_name;

    // Collect minor versions in descending order
    std::vector<int> versions;
    for (const auto& [ver, _] : minor_map) {
        if (ver > minor) {
            versions.push_back(ver);
        }
    }
    // Sort descending
    std::sort(versions.begin(), versions.end(), std::greater<int>());

    for (int ver : versions) {
        auto it = minor_map.find(ver);
        if (it == minor_map.end()) continue;
        auto entry = it->second.find(current);
        if (entry != it->second.end()) {
            current = entry->second;
        }
    }

    return current;
}

bool ParamRemapping::has_remap(const std::string& param_name) const {
    for (const auto& [major, minor_map] : remap_table_) {
        for (const auto& [minor, entries] : minor_map) {
            if (entries.count(param_name)) return true;
        }
    }
    return false;
}

int ParamRemapping::highest_minor_version(int major) const {
    auto it = remap_table_.find(major);
    if (it == remap_table_.end()) return -1;
    int highest = -1;
    for (const auto& [minor, _] : it->second) {
        if (minor > highest) highest = minor;
    }
    return highest;
}

// ================================================================
// ArduPilot Copter remap tables
// Source: QGC ArduCopterFirmwarePlugin.cc lines 80-160
// ================================================================
void ParamRemapping::load_ardupilot_copter_tables() {
    // ArduPilot 4.0 renames
    add(4, 0, "TUNE_MIN", "TUNE_LOW");
    add(4, 0, "TUNE_MAX", "TUNE_HIGH");

    // ArduPilot 4.7: massive parameter rename and SI unit conversion
    // Position controller: PSC_VELXY_* -> PSC_NE_VEL_*
    add(4, 7, "PSC_NE_VEL_P",    "PSC_VELXY_P");
    add(4, 7, "PSC_NE_VEL_I",    "PSC_VELXY_I");
    add(4, 7, "PSC_NE_VEL_D",    "PSC_VELXY_D");
    add(4, 7, "PSC_NE_VEL_IMAX", "PSC_VELXY_IMAX");
    add(4, 7, "PSC_NE_VEL_FLTE", "PSC_VELXY_FLTE");
    add(4, 7, "PSC_NE_VEL_FLTD", "PSC_VELXY_FLTD");
    add(4, 7, "PSC_NE_VEL_FF",   "PSC_VELXY_FF");

    // Position controller: PSC_VELZ_* -> PSC_D_VEL_*
    add(4, 7, "PSC_D_VEL_P",     "PSC_VELZ_P");
    add(4, 7, "PSC_D_VEL_I",     "PSC_VELZ_I");
    add(4, 7, "PSC_D_VEL_D",     "PSC_VELZ_D");
    add(4, 7, "PSC_D_VEL_IMAX",  "PSC_VELZ_IMAX");
    add(4, 7, "PSC_D_VEL_FLTE",  "PSC_VELZ_FLTE");
    add(4, 7, "PSC_D_VEL_FF",    "PSC_VELZ_FF");

    // Position controller: PSC_ACCZ_* -> PSC_D_ACC_*
    add(4, 7, "PSC_D_ACC_P",     "PSC_ACCZ_P");
    add(4, 7, "PSC_D_ACC_I",     "PSC_ACCZ_I");
    add(4, 7, "PSC_D_ACC_D",     "PSC_ACCZ_D");
    add(4, 7, "PSC_D_ACC_IMAX",  "PSC_ACCZ_IMAX");
    add(4, 7, "PSC_D_ACC_FLTD",  "PSC_ACCZ_FLTD");
    add(4, 7, "PSC_D_ACC_FLTE",  "PSC_ACCZ_FLTE");
    add(4, 7, "PSC_D_ACC_FLTT",  "PSC_ACCZ_FLTT");
    add(4, 7, "PSC_D_ACC_FF",    "PSC_ACCZ_FF");
    add(4, 7, "PSC_D_ACC_SMAX",  "PSC_ACCZ_SMAX");

    // Position controller renames
    add(4, 7, "PSC_NE_POS_P",    "PSC_POSXY_P");
    add(4, 7, "PSC_D_POS_P",     "PSC_POSZ_P");

    // Waypoint navigation: WPNAV_* -> WP_*
    add(4, 7, "WP_ACC",          "WPNAV_ACCEL");
    add(4, 7, "WP_ACC_CNR",      "WPNAV_ACCEL_C");
    add(4, 7, "WP_ACC_Z",        "WPNAV_ACCEL_Z");
    add(4, 7, "WP_RADIUS_M",     "WPNAV_RADIUS");
    add(4, 7, "WP_SPD",          "WPNAV_SPEED");
    add(4, 7, "WP_SPD_DN",       "WPNAV_SPEED_DN");
    add(4, 7, "WP_SPD_UP",       "WPNAV_SPEED_UP");

    // RTL parameters
    add(4, 7, "RTL_ALT_M",       "RTL_ALT");
    add(4, 7, "RTL_SPEED_MS",    "RTL_SPEED");
    add(4, 7, "RTL_ALT_FINAL_M", "RTL_ALT_FINAL");
    add(4, 7, "RTL_CLIMB_MIN_M", "RTL_CLIMB_MIN");

    // Landing parameters
    add(4, 7, "LAND_SPD_MS",     "LAND_SPEED");
    add(4, 7, "LAND_SPD_HIGH_MS","LAND_SPEED_HIGH");
    add(4, 7, "LAND_ALT_LOW_M",  "LAND_ALT_LOW");

    // Loiter parameters
    add(4, 7, "LOIT_SPEED_MS",   "LOIT_SPEED");
    add(4, 7, "LOIT_ACC_MAX_M",  "LOIT_ACC_MAX");
    add(4, 7, "LOIT_BRK_ACC_M",  "LOIT_BRK_ACCEL");
    add(4, 7, "LOIT_BRK_JRK_M",  "LOIT_BRK_JERK");

    // Pilot parameters
    add(4, 7, "PILOT_ACC_Z",     "PILOT_ACCEL_Z");
    add(4, 7, "PILOT_SPD_UP",    "PILOT_SPEED_UP");
    add(4, 7, "PILOT_SPD_DN",    "PILOT_SPEED_DN");
    add(4, 7, "PILOT_TKO_ALT_M", "PILOT_TKOFF_ALT");

    // Attitude controller
    add(4, 7, "ATC_ANGLE_MAX",   "ANGLE_MAX");
    add(4, 7, "ATC_ACC_R_MAX",   "ATC_ACCEL_R_MAX");
    add(4, 7, "ATC_ACC_P_MAX",   "ATC_ACCEL_P_MAX");
    add(4, 7, "ATC_ACC_Y_MAX",   "ATC_ACCEL_Y_MAX");
    add(4, 7, "ATC_RATE_WPY_MAX","ATC_SLEW_YAW");

    // Circle
    add(4, 7, "CIRCLE_RADIUS_M", "CIRCLE_RADIUS");

    // PosHold
    add(4, 7, "PHLD_BRK_ANGLE",  "PHLD_BRAKE_ANGLE");
    add(4, 7, "PHLD_BRK_RATE",   "PHLD_BRAKE_RATE");

    // EKF
    add(4, 7, "EK3_FLOW_MAX",    "EK3_MAX_FLOW");
}

void ParamRemapping::load_ardupilot_plane_tables() {
    // ArduPilot Plane 4.7 renames (subset of most common)
    // WPNAV -> WP (shared with Copter)
    add(4, 7, "WP_ACC",          "WPNAV_ACCEL");
    add(4, 7, "WP_RADIUS_M",     "WPNAV_RADIUS");
    add(4, 7, "WP_SPD",          "WPNAV_SPEED");

    // RTL
    add(4, 7, "RTL_ALT_M",       "RTL_ALT");
    add(4, 7, "RTL_SPEED_MS",    "RTL_SPEED");

    // TECS (Total Energy Control System) — Plane specific
    add(4, 7, "TECS_CLMB_MAX_M",  "TECS_CLMB_MAX");
    add(4, 7, "TECS_SINK_MAX_M",  "TECS_SINK_MAX");
    add(4, 7, "TECS_SPDWEIGHT",   "TECS_SPDWEIGHT");
    add(4, 7, "TECS_HGT_OMEGA",   "TECS_HGT_OMEGA");
    add(4, 7, "TECS_SPD_OMEGA",   "TECS_SPD_OMEGA");

    // ARSPD
    add(4, 7, "ARSPD_FBW_MIN_M",  "ARSPD_FBW_MIN");
    add(4, 7, "ARSPD_FBW_MAX_M",  "ARSPD_FBW_MAX");
}

void ParamRemapping::load_ardupilot_rover_tables() {
    // ArduPilot Rover 4.7 renames
    add(4, 7, "WP_ACC",          "WPNAV_ACCEL");
    add(4, 7, "WP_RADIUS_M",     "WPNAV_RADIUS");
    add(4, 7, "WP_SPD",          "WPNAV_SPEED");

    // Rover speed parameters
    add(4, 7, "CRUISE_SPEED_M",  "CRUISE_SPEED");
    add(4, 7, "CRUISE_THROTTLE", "CRUISE_THROTTLE");
}

} // namespace mg::core::firmware
