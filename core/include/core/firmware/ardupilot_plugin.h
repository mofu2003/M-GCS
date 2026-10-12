/**
 * ardupilot_plugin.h - ArduPilot FirmwarePlugin implementation
 * ArduPilot 固件插件 — 覆盖所有 ArduPilot 机型的模式表和行为
 *
 * Implements IFirmwarePlugin for all ArduPilot vehicle types.
 * Mode tables are sourced from existing modes/ headers (already in the project).
 *
 * QGC precedent: src/FirmwarePlugin/APM/APMFirmwarePlugin.cc
 */

#ifndef MGCORE_ARDUPILOT_PLUGIN_H
#define MGCORE_ARDUPILOT_PLUGIN_H

#include "core/firmware/firmware_plugin.h"
#include "core/platform/platform_mutex.h"

namespace mg::core::firmware {

class ArduPilotPlugin : public IFirmwarePlugin {
public:
    FirmwareKind firmware_kind() const override { return FirmwareKind::ArduPilot; }
    const char* plugin_name() const override { return "ArduPilot"; }

    std::string decode_flight_mode(VehicleKind vehicle, uint32_t custom_mode) const override;
    int encode_flight_mode(VehicleKind vehicle, const std::string& mode_name) const override;
    std::vector<FlightModeEntry> available_modes(VehicleKind vehicle) const override;

    const char* rtl_mode_name()       const override { return "RTL"; }
    const char* land_mode_name()      const override { return "Land"; }
    const char* guided_mode_name()    const override { return "Guided"; }
    const char* loiter_mode_name()    const override { return "Loiter"; }
    const char* auto_mode_name()      const override { return "Auto"; }
    const char* smart_rtl_mode_name() const override { return "SmartRTL"; }

    bool is_guided_mode(const std::string& mode_name) const override {
        return mode_name == "Guided";
    }

    uint32_t capabilities(VehicleKind vehicle) const override;
    float minimum_takeoff_altitude(VehicleKind vehicle) const override;


private:
};

} // namespace mg::core::firmware

#endif // MGCORE_ARDUPILOT_PLUGIN_H
