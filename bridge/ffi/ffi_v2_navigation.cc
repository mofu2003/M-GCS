#define MGCORE_EXPORTS
/**
 * V2 normalized navigation and motion telemetry FFI entrypoints.
 */
#include "ffi_v2_response.h"

#include <cstdint>

using mg::bridge::JsonBuilder;

MGCORE_API char* mg_vehicle_get_session_status(mg_core_t* core, int vehicle_id) {
    CHECK_CORE_RET(core, mg::bridge::ffi::v2::error("CORE_NULL", "null core"));
    const auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) {
        return mg::bridge::ffi::v2::error("SESSION_NOT_FOUND", "vehicle session not found");
    }

    auto& cache = vehicle->cache();
    const auto gps = cache.snapshot_gps().data;
    const auto vfr = cache.snapshot_vfr_hud().data;
    const auto global_position = cache.snapshot_global_pos().data;
    const auto battery = cache.snapshot_battery().data;
    const auto sys_status = cache.snapshot_sys_status().data;

    const bool gps_fresh = cache.is_fresh(mg::bridge::TelemetryType::kGps);
    const bool vfr_fresh = cache.is_fresh(mg::bridge::TelemetryType::kVfrHud);
    const bool global_position_fresh =
        cache.is_fresh(mg::bridge::TelemetryType::kGlobalPosition);
    const bool battery_fresh = cache.is_fresh(mg::bridge::TelemetryType::kBattery);
    const bool sys_status_fresh = cache.is_fresh(mg::bridge::TelemetryType::kSysStatus);

    const int satellite_count = gps.satellites_visible == UINT8_MAX
        ? -1
        : static_cast<int>(gps.satellites_visible);

    bool heading_valid = false;
    double heading_deg = 0.0;
    if (vfr_fresh && vfr.heading >= 0 && vfr.heading < 360) {
        heading_valid = true;
        heading_deg = static_cast<double>(vfr.heading);
    } else if (global_position_fresh && global_position.hdg < 36000) {
        heading_valid = true;
        heading_deg = static_cast<double>(global_position.hdg) / 100.0;
    }

    const bool battery_message_fresh = battery_fresh || sys_status_fresh;
    const double battery_voltage = battery_fresh && battery.voltage_v > 0.0f
        ? static_cast<double>(battery.voltage_v)
        : (sys_status_fresh && sys_status.battery_voltage > 0
            ? static_cast<double>(sys_status.battery_voltage) / 1000.0
            : 0.0);
    const int battery_remaining = battery_fresh && battery.battery_remaining >= 0
        ? static_cast<int>(battery.battery_remaining)
        : (sys_status_fresh && sys_status.battery_remaining >= 0
            ? static_cast<int>(sys_status.battery_remaining)
            : -1);

    JsonBuilder data;
    data.begin_object();
    data.field("session_id", vehicle_id);
    data.field("connected", vehicle->conn().is_connected());
    data.field("heartbeat_alive", vehicle->conn().heartbeat_alive(3000));
    data.field("gps_fresh", gps_fresh);
    data.field("gps_fix_type", gps_fresh ? static_cast<int>(gps.fix_type) : -1);
    data.field("gps_satellites", gps_fresh ? satellite_count : -1);
    data.field("heading_fresh", heading_valid);
    if (heading_valid) data.field("heading_deg", heading_deg);
    else data.raw_field("heading_deg", "null");
    data.field("battery_fresh", battery_message_fresh);
    if (battery_voltage > 0.0) data.field("battery_voltage_v", battery_voltage);
    else data.raw_field("battery_voltage_v", "null");
    if (battery_remaining >= 0) data.field("battery_remaining_pct", battery_remaining);
    else data.raw_field("battery_remaining_pct", "null");
    data.end_object();

    return mg::bridge::ffi::v2::ok(data.str());
}

MGCORE_API char* mg_vehicle_get_global_position(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"lat_deg":0,"lon_deg":0,"alt_msl_m":0,"alt_rel_m":0,"vx_m_s":0,"vy_m_s":0,"vz_m_s":0,"heading_deg":0,"valid":false})",
        [](auto& cache, JsonBuilder& data) {
            auto gp_snap = cache.snapshot_global_pos();
            const auto& gp = gp_snap.data;
            data.field("lat_deg", gp.lat / 1e7);
            data.field("lon_deg", gp.lon / 1e7);
            data.field("alt_msl_m", gp.alt / 1000.0);
            data.field("alt_rel_m", gp.relative_alt / 1000.0);
            data.field("vx_m_s", gp.vx / 100.0);
            data.field("vy_m_s", gp.vy / 100.0);
            data.field("vz_m_s", gp.vz / 100.0);
            data.field("heading_deg", gp.hdg / 100.0);
            data.field("valid", (gp.lat != 0 || gp.lon != 0));
        });
}

MGCORE_API char* mg_vehicle_get_attitude(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"roll_deg":0,"pitch_deg":0,"yaw_deg":0,"rollspeed_dps":0,"pitchspeed_dps":0,"yawspeed_dps":0})",
        [](auto& cache, JsonBuilder& data) {
            auto att_snap = cache.snapshot_attitude();
            const auto& att = att_snap.data;
            const double r2d = 180.0 / 3.141592653589793;
            data.field("roll_deg",      att.roll * r2d);
            data.field("pitch_deg",     att.pitch * r2d);
            data.field("yaw_deg",       att.yaw * r2d);
            data.field("rollspeed_dps", att.rollspeed * r2d);
            data.field("pitchspeed_dps",att.pitchspeed * r2d);
            data.field("yawspeed_dps",  att.yawspeed * r2d);
        });
}

MGCORE_API char* mg_vehicle_get_gps(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"lat_deg":0,"lon_deg":0,"alt_m":0,"fix_type":0,"satellites":0,"valid":false})",
        [](auto& cache, JsonBuilder& data) {
            auto gps_snap = cache.snapshot_gps();
            const auto& gps = gps_snap.data;
            data.field("lat_deg",    gps.lat / 1e7);
            data.field("lon_deg",    gps.lon / 1e7);
            data.field("alt_m",      gps.alt / 1000.0);
            data.field("fix_type",   (int)gps.fix_type);
            data.field("satellites", (int)gps.satellites_visible);
            data.field("valid",      gps.fix_type >= 3);
        });
}

MGCORE_API char* mg_vehicle_get_home_position(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"lat_deg":0,"lon_deg":0,"alt_m":0,"valid":false})",
        [](auto& cache, JsonBuilder& data) {
            auto home_snap = cache.snapshot_home();
            const auto& hp = home_snap.data;
            data.field("lat_deg", hp.latitude / 1e7);
            data.field("lon_deg", hp.longitude / 1e7);
            data.field("alt_m",   hp.altitude / 1000.0);
            data.field("valid",   hp.latitude != 0 || hp.longitude != 0);
        });
}

MGCORE_API char* mg_vehicle_get_vfr_hud(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"airspeed_m_s":0,"groundspeed_m_s":0,"alt_msl_m":0,"climb_m_s":0,"heading_deg":0,"throttle_pct":0})",
        [](auto& cache, JsonBuilder& data) {
            auto snap = cache.snapshot_vfr_hud();
            const auto& d = snap.data;
            data.field("airspeed_m_s",   d.airspeed);
            data.field("groundspeed_m_s",d.groundspeed);
            data.field("alt_msl_m",      d.alt);
            data.field("climb_m_s",      d.climb);
            data.field("heading_deg",    (double)d.heading);
            data.field("throttle_pct",   (double)d.throttle);
        });
}

MGCORE_API char* mg_vehicle_get_extended_sys_state(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"vtol_state":0,"landed_state":0})",
        [](auto& cache, JsonBuilder& data) {
            auto snap = cache.snapshot_ext_sys();
            const auto& d = snap.data;
            data.field("vtol_state",   (int)d.vtol_state);
            data.field("landed_state", (int)d.landed_state);
        });
}

MGCORE_API char* mg_vehicle_get_wind_estimate(mg_core_t* core) {
    return mg::bridge::ffi::v2::object_from_cache(core,
        R"({"wind_dir_deg":0,"wind_speed_m_s":0,"vertical_m_s":0})",
        [](auto& cache, JsonBuilder& data) {
            auto snap = cache.snapshot_wind();
            const auto& d = snap.data;
            double ws = sqrt(d.wind_x * d.wind_x + d.wind_y * d.wind_y);
            double wd = atan2(-d.wind_x, -d.wind_y) * 180.0 / 3.141592653589793;
            if (wd < 0) wd += 360.0;
            data.field("wind_dir_deg",  wd);
            data.field("wind_speed_m_s", ws);
            data.field("vertical_m_s",  (double)d.wind_z);
        });
}
