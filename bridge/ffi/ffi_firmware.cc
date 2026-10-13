#define MGCORE_EXPORTS

#include "ffi_internal.h"
#include "ffi_response.h"
#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"
#include "core/telemetry/json_serializers.h"

#include <vector>

namespace {

const char* firmware_profile(mg::core::firmware::FirmwareKind kind) {
    return kind == mg::core::firmware::FirmwareKind::ArduPilot
        ? "ardupilot" : "unknown";
}

void capability_field(mg::core::telemetry::JsonBuilder& jb,
                      const char* name,
                      uint32_t flags,
                      mg::core::firmware::FirmwareCapability capability) {
    jb.field(name, (flags & static_cast<uint32_t>(capability)) != 0);
}

std::string capability_json(uint32_t flags) {
    mg::core::telemetry::JsonBuilder caps;
    caps.begin_object();
    capability_field(caps, "guided_mode", flags, mg::core::firmware::CapGuidedMode);
    capability_field(caps, "pause", flags, mg::core::firmware::CapPause);
    capability_field(caps, "takeoff", flags, mg::core::firmware::CapTakeoff);
    capability_field(caps, "orbit", flags, mg::core::firmware::CapOrbit);
    capability_field(caps, "smart_rtl", flags, mg::core::firmware::CapSmartRTL);
    capability_field(caps, "set_home", flags, mg::core::firmware::CapSetHome);
    capability_field(caps, "motor_test", flags, mg::core::firmware::CapMotorTest);
    capability_field(caps, "calibration", flags, mg::core::firmware::CapCalibration);
    capability_field(caps, "roi", flags, mg::core::firmware::CapRoi);
    capability_field(caps, "change_heading", flags, mg::core::firmware::CapChangeHeading);
    capability_field(caps, "guided_land", flags, mg::core::firmware::CapGuidedLand);
    caps.end_object();
    return caps.str();
}

}  // namespace

MGCORE_API char* mg_firmware_get_type(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) {
        mg::core::telemetry::JsonBuilder empty;
        empty.begin_object();
        empty.field("schema_version", 1);
        empty.field("firmware", "Unknown");
        empty.field("profile", "unknown");
        empty.field("vehicle_type", "Unknown");
        empty.field("vehicle_class", "unknown");
        empty.field("control_profile", "unknown");
        empty.field("mission_vehicle_class", "");
        empty.field("pause_mode", "");
        empty.field("forward_flight", false);
        empty.raw_field("takeoff_altitude_roles", "[]");
        empty.raw_field("flight_mode_pwm_ranges", "[]");
        empty.raw_field("arming_check_bits", "[]");
        empty.field("autopilot_type", 0);
        empty.field("vehicle_type_id", 0);
        empty.field("capability_flags", 0);
        empty.field("minimum_takeoff_altitude_m", 0.0);
        empty.raw_field("capabilities", capability_json(0));
        empty.end_object();
        return mg::bridge::ffi::response::ok(empty.str());
    }

    auto hb_snap = cache->snapshot_heartbeat();
    const auto& hb = hb_snap.data;
    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(hb.autopilot_type, hb.vehicle_type, ffi_quadplane_enable(core));
    auto mav = shared_active_mavlink(core);
    const bool is_ardupilot =
        adapter.firmware_kind() == mg::core::firmware::FirmwareKind::ArduPilot;
    auto plugin = mg::core::firmware::create_firmware_plugin(adapter.firmware_kind());
    const uint32_t flags = plugin ? plugin->capabilities(adapter.vehicle_kind()) : 0;
    const float minimum_takeoff_altitude_m =
        plugin ? plugin->minimum_takeoff_altitude(adapter.vehicle_kind()) : 0.0f;

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("schema_version", 1);
    jb.field("firmware", adapter.firmware_name());
    jb.field("profile", firmware_profile(adapter.firmware_kind()));
    const auto vehicle_name = adapter.normalized_vehicle_name();
    jb.field("vehicle_type", vehicle_name);
    const char* vehicle_class = "unknown";
    if (vehicle_name == "Copter") vehicle_class = "multirotor";
    else if (vehicle_name == "FixedWing") vehicle_class = "fixed_wing";
    else if (vehicle_name == "VTOL") vehicle_class = "vtol";
    else if (vehicle_name == "Rover") vehicle_class = "rover";
    else if (vehicle_name == "Submarine") vehicle_class = "submarine";
    else if (vehicle_name == "AntennaTracker") vehicle_class = "tracker";
    jb.field("vehicle_class", vehicle_class);

    const char* control_profile = "unknown";
    if (is_ardupilot) {
        if (vehicle_name == "Copter") control_profile = "copter";
        else if (vehicle_name == "FixedWing") control_profile = "fixed_wing";
        else if (vehicle_name == "VTOL") control_profile = "vtol";
        else if (vehicle_name == "Rover") control_profile = "rover";
    }
    jb.field("control_profile", control_profile);

    const char* mission_vehicle_class = "";
    if (is_ardupilot && vehicle_name == "Copter") mission_vehicle_class = "copter";
    else if (is_ardupilot && vehicle_name == "FixedWing") mission_vehicle_class = "plane";
    else if (is_ardupilot && vehicle_name == "VTOL") mission_vehicle_class = "vtol";
    else if (is_ardupilot && vehicle_name == "Rover") mission_vehicle_class = "rover";
    else if (is_ardupilot && vehicle_name == "Submarine") mission_vehicle_class = "sub";
    else if (is_ardupilot && vehicle_name == "AntennaTracker") mission_vehicle_class = "tracker";
    jb.field("mission_vehicle_class", mission_vehicle_class);

    const char* pause_mode = "";
    switch (is_ardupilot ? adapter.vehicle_kind()
                         : mg::core::firmware::VehicleKind::Unknown) {
        case mg::core::firmware::VehicleKind::Quadrotor:
        case mg::core::firmware::VehicleKind::Coaxial:
        case mg::core::firmware::VehicleKind::Helicopter:
        case mg::core::firmware::VehicleKind::Hexarotor:
        case mg::core::firmware::VehicleKind::Octorotor:
        case mg::core::firmware::VehicleKind::Tricopter:
            pause_mode = "Brake";
            break;
        case mg::core::firmware::VehicleKind::FixedWing:
        case mg::core::firmware::VehicleKind::VTOL:
            pause_mode = "Loiter";
            break;
        case mg::core::firmware::VehicleKind::GroundRover:
        case mg::core::firmware::VehicleKind::SurfaceBoat:
            pause_mode = "Hold";
            break;
        default:
            break;
    }
    jb.field("pause_mode", pause_mode);

    const auto extended_state = mav ? mav->latest_extended_sys_state() : std::nullopt;
    const bool forward_flight = is_ardupilot &&
        (vehicle_name == "FixedWing" ||
         (vehicle_name == "VTOL" && extended_state.has_value() &&
          extended_state->vtol_state == MAV_VTOL_STATE_FW));
    jb.field("forward_flight", forward_flight);

    std::vector<std::string> takeoff_altitude_roles;
    if (is_ardupilot && vehicle_name == "VTOL") {
        takeoff_altitude_roles = {"takeoff.vtol_alt_m", "takeoff.vtol_alt_cm_legacy",
                                  "takeoff.vtol_alt_fallback_m"};
    } else if (is_ardupilot && vehicle_name == "Copter") {
        takeoff_altitude_roles = {"takeoff.alt_m", "takeoff.alt_cm_legacy"};
    }
    mg::core::telemetry::JsonBuilder altitude_roles;
    altitude_roles.begin_array();
    for (const auto& role : takeoff_altitude_roles) altitude_roles.append_string(role);
    altitude_roles.end_array();
    jb.raw_field("takeoff_altitude_roles", altitude_roles.str());

    const bool fixed_wing_profile = is_ardupilot &&
        (vehicle_name == "FixedWing" || vehicle_name == "VTOL");
    struct PwmRange { const char* label; int minimum; int maximum; };
    const PwmRange copter_mode_pwm_ranges[] = {
        {"≤ 1230", 910, 1230}, {"1231–1360", 1231, 1360},
        {"1361–1490", 1361, 1490}, {"1491–1620", 1491, 1620},
        {"1621–1749", 1621, 1749}, {"≥ 1750", 1750, 2049},
    };
    const PwmRange plane_mode_pwm_ranges[] = {
        {"910–1230 / ≥2050", 910, 1230}, {"1231–1360", 1231, 1360},
        {"1361–1490", 1361, 1490}, {"1491–1620", 1491, 1620},
        {"1621–1749", 1621, 1749}, {"1750–2049", 1750, 2049},
    };
    mg::core::telemetry::JsonBuilder pwm_ranges;
    pwm_ranges.begin_array();
    for (size_t i = 0; i < 6; ++i) {
        const auto& row = fixed_wing_profile ? plane_mode_pwm_ranges[i]
                                             : copter_mode_pwm_ranges[i];
        mg::core::telemetry::JsonBuilder range;
        range.begin_object();
        range.field("label", row.label);
        range.field("minimum", row.minimum);
        range.field("maximum", row.maximum);
        range.end_object();
        pwm_ranges.append_json(range.str());
    }
    pwm_ranges.end_array();
    jb.raw_field("flight_mode_pwm_ranges", pwm_ranges.str());

    std::vector<int> arming_check_bits = {
        2, 4, 8, 16, 32, 64, 128, 256, 1024, 2048, 4096,
        8192, 16384, 32768, 65536, 131072, 524288,
    };
    if (fixed_wing_profile) arming_check_bits.push_back(512);
    if (is_ardupilot && vehicle_name == "Copter") arming_check_bits.push_back(262144);
    mg::core::telemetry::JsonBuilder arming_bits;
    arming_bits.begin_array();
    for (const int bit : arming_check_bits) arming_bits.append_json(std::to_string(bit));
    arming_bits.end_array();
    jb.raw_field("arming_check_bits", arming_bits.str());
    jb.field("autopilot_type", static_cast<int>(hb.autopilot_type));
    jb.field("vehicle_type_id", static_cast<int>(hb.vehicle_type));
    jb.field("capability_flags", static_cast<unsigned>(flags));
    jb.field("minimum_takeoff_altitude_m", static_cast<double>(minimum_takeoff_altitude_m));
    jb.raw_field("capabilities", capability_json(flags));
    jb.end_object();
    return mg::bridge::ffi::response::ok(jb.str());
}
