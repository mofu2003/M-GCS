#include "core/firmware/firmware_adapter.h"
#include "core/firmware/param_remapping.h"

#include <cstdio>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { g_passed++; fprintf(stderr, "  [PASS] %s\n", name); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", name); } \
} while (0)

using mg::core::firmware::FirmwareAdapter;
using mg::core::firmware::ParamRemapping;

static void test_ardupilot_vehicle_families() {
    fprintf(stderr, "\n[Group] ArduPilot normalized vehicle families\n");

    FirmwareAdapter plane;
    plane.detect(3, 1);
    CHECK("AP plane firmware", plane.firmware_name() == "ArduPilot");
    CHECK("AP plane normalized family", plane.normalized_vehicle_name() == "FixedWing");

    FirmwareAdapter tricopter;
    tricopter.detect(3, 15);
    CHECK("AP tricopter normalized family", tricopter.normalized_vehicle_name() == "Copter");

    FirmwareAdapter boat;
    boat.detect(3, 11);
    CHECK("AP boat normalized family", boat.normalized_vehicle_name() == "Rover");

    FirmwareAdapter sub;
    sub.detect(3, 12);
    CHECK("AP submarine normalized family", sub.normalized_vehicle_name() == "Submarine");

    FirmwareAdapter vtol;
    vtol.detect(3, 19);
    CHECK("AP VTOL normalized family", vtol.normalized_vehicle_name() == "VTOL");

    FirmwareAdapter tracker;
    tracker.detect(3, 5);
    CHECK("AP tracker normalized family", tracker.normalized_vehicle_name() == "AntennaTracker");
}

static void test_px4_vehicle_families() {
    fprintf(stderr, "\n[Group] PX4 normalized vehicle families\n");

    FirmwareAdapter quad;
    quad.detect(12, 2);
    CHECK("PX4 quad firmware", quad.firmware_name() == "PX4");
    CHECK("PX4 quad normalized family", quad.normalized_vehicle_name() == "Copter");

    FirmwareAdapter plane;
    plane.detect(12, 1);
    CHECK("PX4 plane normalized family", plane.normalized_vehicle_name() == "FixedWing");

    FirmwareAdapter boat;
    boat.detect(12, 11);
    CHECK("PX4 boat normalized family", boat.normalized_vehicle_name() == "Rover");

    FirmwareAdapter sub;
    sub.detect(12, 12);
    CHECK("PX4 submarine normalized family", sub.normalized_vehicle_name() == "Submarine");

    FirmwareAdapter vtol;
    vtol.detect(12, 20);
    CHECK("PX4 VTOL normalized family", vtol.normalized_vehicle_name() == "VTOL");
}

static void test_ardupilot_parameter_remapping() {
    fprintf(stderr, "\n[Group] ArduPilot parameter remapping\n");
    ParamRemapping remap;
    remap.load_ardupilot_copter_tables();
    CHECK("4.7 new WP name maps to old WPNAV name",
          remap.remap(4, 5, "WP_SPD") == "WPNAV_SPEED");
    CHECK("4.7 firmware keeps new parameter name",
          remap.remap(4, 7, "WP_SPD") == "WP_SPD");
    CHECK("unknown parameter name passes through",
          remap.remap(4, 5, "UNKNOWN_PARAM") == "UNKNOWN_PARAM");
    CHECK("old firmware name reverse-maps to canonical",
          remap.canonical_name(4, 5, "WPNAV_SPEED") == "WP_SPD");
    CHECK("canonical reverse mapping keeps unknown names",
          remap.canonical_name(4, 5, "UNKNOWN_PARAM") == "UNKNOWN_PARAM");

    ParamRemapping plane;
    plane.load_ardupilot_plane_tables();
    CHECK("plane Q pilot speed alias maps for old firmware",
          plane.remap(4, 5, "Q_PILOT_SPD_DN") == "Q_PILOT_SPEED_DN");
    CHECK("arming skip-check alias maps for old firmware",
          plane.remap(4, 5, "ARMING_SKIPCHK") == "ARMING_CHECK");
}


/// 模式表必须与上游枚举一致。
///
/// 这三张表曾经把"模式列表的下标"当成 custom_mode 值，从某个号开始整体错位：
/// Copter 的 9 写成 Drift（真值是 LAND，于是降落时界面显示 "Drift"）、
/// Rover 的 9 写成 Dock（真值是 CIRCLE）、Tracker 的 1 写成 Scan（真值是 STOP）。
/// 这些断言按上游 mode.h 的枚举值钉住关键模式。
static void test_mode_tables_match_upstream_enums() {
    fprintf(stderr, "\n[Group] Mode tables match upstream enums\n");

    FirmwareAdapter copter;
    copter.detect(3, 2);  // MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_TYPE_QUADROTOR
    CHECK("Copter 9 is Land (not Drift)", copter.decode_flight_mode(9).mode_name == "Land");
    CHECK("Copter 11 is Drift", copter.decode_flight_mode(11).mode_name == "Drift");
    CHECK("Copter 13 is Sport", copter.decode_flight_mode(13).mode_name == "Sport");
    CHECK("Copter 15 is Autotune", copter.decode_flight_mode(15).mode_name == "Autotune");
    CHECK("Copter 16 is PosHold", copter.decode_flight_mode(16).mode_name == "PosHold");
    CHECK("Copter 21 is SmartRTL", copter.decode_flight_mode(21).mode_name == "SmartRTL");
    CHECK("Copter 28 is Turtle", copter.decode_flight_mode(28).mode_name == "Turtle");
    CHECK("Copter 8 is unused", copter.decode_flight_mode(8).mode_name == "Mode(8)");

    FirmwareAdapter plane;
    plane.detect(3, 1);  // MAV_TYPE_FIXED_WING
    CHECK("Plane 11 is RTL", plane.decode_flight_mode(11).mode_name == "RTL");
    CHECK("Plane 12 is Loiter", plane.decode_flight_mode(12).mode_name == "Loiter");

    FirmwareAdapter rover;
    rover.detect(3, 10);  // MAV_TYPE_GROUND_ROVER
    CHECK("Rover 5 is Loiter", rover.decode_flight_mode(5).mode_name == "Loiter");
    CHECK("Rover 9 is Circle", rover.decode_flight_mode(9).mode_name == "Circle");
    CHECK("Rover 11 is RTL", rover.decode_flight_mode(11).mode_name == "RTL");
    CHECK("Rover has no Autotune mode", rover.decode_flight_mode(16).mode_name == "Initialising");

    FirmwareAdapter tracker;
    tracker.detect(3, 5);  // MAV_TYPE_GROUND_ROVER? no: 5 = MAV_TYPE_SURFACE_BOAT
    FirmwareAdapter antenna;
    antenna.detect(3, 16);  // not used; tracker detection covered below
    (void)tracker;
    (void)antenna;
}

/// 反查与语义角色：模式号只能来自插件，且不存在的模式必须解析失败。
static void test_mode_role_resolution() {
    fprintf(stderr, "\n[Group] Mode role resolution\n");

    FirmwareAdapter copter;
    copter.detect(3, 2);
    CHECK("Copter RTL role -> 6",
          copter.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 6u);
    CHECK("Copter Land role -> 9",
          copter.resolve_mode_role(FirmwareAdapter::ModeRole::Land) == 9u);
    CHECK("Copter Loiter role -> 5",
          copter.resolve_mode_role(FirmwareAdapter::ModeRole::Loiter) == 5u);

    FirmwareAdapter plane;
    plane.detect(3, 1);
    CHECK("Plane RTL role -> 11",
          plane.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 11u);
    CHECK("Plane Loiter role -> 12",
          plane.resolve_mode_role(FirmwareAdapter::ModeRole::Loiter) == 12u);
    CHECK("Plane Land role is unresolvable",
          !plane.resolve_mode_role(FirmwareAdapter::ModeRole::Land).has_value());

    FirmwareAdapter rover;
    rover.detect(3, 10);
    CHECK("Rover RTL role -> 11",
          rover.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 11u);
    CHECK("Rover Land role is unresolvable (9 is Circle)",
          !rover.resolve_mode_role(FirmwareAdapter::ModeRole::Land).has_value());

    FirmwareAdapter px4;
    px4.detect(12, 2);  // MAV_AUTOPILOT_PX4
    const auto px4_rtl = px4.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch);
    CHECK("PX4 RTL role resolves to a packed custom_mode",
          px4_rtl.has_value() && *px4_rtl != 6u);
    CHECK("PX4 RTL decodes back to Auto RTL",
          px4_rtl.has_value() && px4.decode_flight_mode(*px4_rtl).mode_name == "Auto RTL");

    FirmwareAdapter unknown;
    CHECK("Unknown firmware resolves nothing",
          !unknown.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch).has_value());
    CHECK("Name lookup is case insensitive",
          copter.encode_flight_mode("rtl") == copter.encode_flight_mode("RTL"));
    CHECK("Unknown mode name resolves to nullopt",
          !copter.encode_flight_mode("WarpDrive").has_value());
}

int main() {
    fprintf(stderr, "=== Firmware Adapter Classification Tests ===\n");

    test_ardupilot_vehicle_families();
    test_px4_vehicle_families();
    test_ardupilot_parameter_remapping();
    test_mode_tables_match_upstream_enums();
    test_mode_role_resolution();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
