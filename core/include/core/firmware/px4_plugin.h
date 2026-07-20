/**
 * px4_plugin.h - PX4 FirmwarePlugin implementation
 * PX4 固件插件 — PX4 飞行模式解码（main_mode/sub_mode 组合）
 *
 * Implements IFirmwarePlugin for PX4 autopilot.
 * PX4 encodes flight modes as (main_mode << 16) | (sub_mode << 24).
 *
 * QGC precedent: src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc
 */

#ifndef MGCORE_PX4_PLUGIN_H
#define MGCORE_PX4_PLUGIN_H

#include "core/firmware/firmware_plugin.h"

namespace mg::core::firmware {

class PX4Plugin : public IFirmwarePlugin {
public:
    FirmwareKind firmware_kind() const override { return FirmwareKind::PX4; }
    const char* plugin_name() const override { return "PX4"; }

    std::string decode_flight_mode(VehicleKind vehicle, uint32_t custom_mode) const override;
    int encode_flight_mode(VehicleKind vehicle, const std::string& mode_name) const override;
    std::vector<FlightModeEntry> available_modes(VehicleKind vehicle) const override;

    const char* rtl_mode_name()       const override { return "Auto RTL"; }
    const char* land_mode_name()      const override { return "Auto Land"; }
    const char* guided_mode_name()    const override { return "Offboard"; }
    const char* loiter_mode_name()    const override { return "Auto Loiter"; }
    const char* auto_mode_name()      const override { return "Auto Mission"; }
    const char* smart_rtl_mode_name() const override { return ""; }  // PX4 has no SmartRTL

    bool is_guided_mode(const std::string& mode_name) const override {
        return mode_name == "Offboard";
    }

    uint32_t capabilities(VehicleKind vehicle) const override;
    float minimum_takeoff_altitude(VehicleKind vehicle) const override;
    float default_loiter_radius(VehicleKind vehicle) const override;

    std::string remap_param_name(VehicleKind /*vehicle*/, int /*major*/, int /*minor*/, const std::string& name) const override { return name; }
    bool has_param_remapping() const override { return false; }
};

} // namespace mg::core::firmware

#endif // MGCORE_PX4_PLUGIN_H
