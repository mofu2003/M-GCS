/**
 * param_remapping.h - Parameter name version remapping
 * 参数重映射 — 跨固件版本的参数名自动翻译
 *
 * ArduPilot renames parameters across firmware versions. For example,
 * TUNE_LOW was renamed to TUNE_MIN in ArduPilot 4.0, and WPNAV_SPEED
 * was renamed to WP_SPD in ArduPilot 4.7.
 *
 * When the GCS sends a parameter using the new name to an old firmware,
 * the FC won't recognize it. This module translates new names to old
 * names based on the detected firmware version.
 *
 * Design (QGC FirmwarePlugin::remapParamNameMajorVersionMap_t pattern):
 *   - Remap tables are organized by [major_version][minor_version]
 *   - Each entry maps new_name -> old_name
 *   - remap() walks backwards from highest known version to the vehicle's
 *     actual version, translating at each step
 *   - Unknown names pass through unchanged (safe to always run)
 *
 * Usage:
 *   ParamRemapping remap;
 *   remap.load_ardupilot_copter_tables();
 *   std::string actual = remap.remap(4, 5, "WP_SPD");  // -> "WPNAV_SPEED"
 *   std::string actual2 = remap.remap(4, 7, "WP_SPD"); // -> "WP_SPD" (no change)
 *
 * QGC reference:
 *   src/FirmwarePlugin/APM/ArduCopterFirmwarePlugin.cc (lines 80-160)
 *   src/FirmwarePlugin/APM/ArduPlaneFirmwarePlugin.cc
 */

#ifndef MGCORE_PARAM_REMAPPING_H
#define MGCORE_PARAM_REMAPPING_H

#include <string>
#include <map>
#include <vector>

namespace mg::core::firmware {

class ParamRemapping {
public:
    ParamRemapping();
    ~ParamRemapping();

    /// Load ArduPilot Copter remap tables (from QGC ArduCopterFirmwarePlugin.cc)
    void load_ardupilot_copter_tables();

    /// Load ArduPilot Plane remap tables (from QGC ArduPlaneFirmwarePlugin.cc)
    void load_ardupilot_plane_tables();

    /// Load ArduPilot Rover remap tables
    void load_ardupilot_rover_tables();

    /// Remap a parameter name for the given firmware version.
    /// If the vehicle is running firmware (major.minor), this translates
    /// the current (newest) name to the name that firmware version expects.
    /// Returns the original name if no remapping is needed.
    std::string remap(int major, int minor, const std::string& param_name) const;

    /// Translate a firmware parameter name back to the newest canonical name.
    /// This is the inverse direction of remap() and is used for prefix matching
    /// against parameters reported by older firmware.
    std::string canonical_name(int major, int minor, const std::string& param_name) const;

    /// Check if a parameter name has any remap entries.
    bool has_remap(const std::string& param_name) const;

    /// Get the highest minor version that has remap entries for a major version.
    /// Returns -1 if no entries exist.
    int highest_minor_version(int major) const;

private:
    // remap_table_[major][minor][new_name] = old_name
    using MinorMap = std::map<std::string, std::string>;  // new_name -> old_name
    using MajorMap = std::map<int, MinorMap>;              // minor -> {new->old}

    std::map<int, MajorMap> remap_table_;

    /// Helper to add a single remap entry
    void add(int major, int minor, const char* new_name, const char* old_name);
};

} // namespace mg::core::firmware

#endif // MGCORE_PARAM_REMAPPING_H
