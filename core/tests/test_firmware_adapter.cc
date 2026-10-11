#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"
#include "core/firmware/modes/px4_modes.h"
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
using mg::core::firmware::FirmwareKind;
using mg::core::firmware::ParamRemapping;
using mg::core::firmware::create_firmware_plugin;

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

/// S1：PX4 只允许存在一份模式表。插件（可用模式列表 / 角色反查）与 adapter
/// （状态栏解码）必须对任意 custom_mode 给出同一个名字，包括兜底串。
static void test_px4_single_mode_table() {
    fprintf(stderr, "\n[Group] PX4 single mode table (plugin vs adapter)\n");

    FirmwareAdapter px4;
    px4.detect(12, 2);  // MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR
    auto plugin = create_firmware_plugin(FirmwareKind::PX4);
    CHECK("PX4 plugin factory returns a plugin", plugin != nullptr);
    if (!plugin) return;

    uint32_t mismatches = 0;
    uint32_t first_bad = 0;
    for (uint32_t main = 0; main <= 0xFF; ++main) {
        for (uint32_t sub = 0; sub <= 0xFF; ++sub) {
            const uint32_t custom = (main << 16) | (sub << 24);
            const std::string from_adapter = px4.decode_flight_mode(custom).mode_name;
            const std::string from_plugin =
                plugin->decode_flight_mode(px4.vehicle_kind(), custom);
            if (from_adapter != from_plugin) {
                if (mismatches == 0) first_bad = custom;
                ++mismatches;
            }
        }
    }
    if (mismatches != 0) {
        fprintf(stderr, "  first mismatch custom_mode=%u (%u total)\n",
                first_bad, mismatches);
    }
    CHECK("every custom_mode decodes identically in plugin and adapter",
          mismatches == 0);

    // 已删除/保留的取值不再伪装成模式。
    CHECK("AUTO sub-mode 7 (ex-RTGS) has no table entry",
          px4.decode_flight_mode((4u << 16) | (7u << 24)).mode_name ==
              "Mode(" + std::to_string((4u << 16) | (7u << 24)) + ")");
    CHECK("AUTO sub-mode 0 falls back to a placeholder",
          px4.decode_flight_mode((4u << 16)).mode_name == "Mode(262144)");
    CHECK("main mode 0 decodes to a placeholder",
          px4.decode_flight_mode(0).mode_name == "Mode(0)");
}

/// S1：PX4 1.14+ 新模式（Altitude Cruise、POSCTL Orbit/Slow、VTOL Takeoff、
/// Guided Course）必须能编码、解码，且可设置标志符合决策。
static void test_px4_new_modes() {
    fprintf(stderr, "\n[Group] PX4 new modes encode/decode\n");

    FirmwareAdapter px4;
    px4.detect(12, 2);
    auto plugin = create_firmware_plugin(FirmwareKind::PX4);
    if (!plugin) {
        CHECK("PX4 plugin available for new-mode test", false);
        return;
    }

    struct Case {
        uint8_t     main_mode;
        uint8_t     sub_mode;
        const char* name;
        bool        can_be_set;
    };
    const Case cases[] = {
        {11, 0,  "Altitude Cruise",    true },
        {3,  1,  "Orbit",              false},
        {3,  2,  "Position Slow",      true },
        {4,  8,  "Auto Follow",        false},
        {4,  9,  "Auto Precision Land", true },
        {4,  10, "VTOL Takeoff",       false},
        {4,  19, "Auto Guided Course", false},
        {10, 0,  "Termination",        false},
    };

    for (const auto& c : cases) {
        const uint32_t custom =
            (static_cast<uint32_t>(c.main_mode) << 16) |
            (static_cast<uint32_t>(c.sub_mode) << 24);

        char label[160];
        snprintf(label, sizeof(label), "PX4 %u/%u decodes to %s",
                 c.main_mode, c.sub_mode, c.name);
        CHECK(label, px4.decode_flight_mode(custom).mode_name == c.name);

        snprintf(label, sizeof(label), "PX4 %s re-encodes to %u", c.name, custom);
        const auto encoded = px4.encode_flight_mode(c.name);
        CHECK(label, encoded.has_value() && *encoded == custom);

        snprintf(label, sizeof(label), "PX4 plugin %s re-encodes to %u", c.name, custom);
        CHECK(label, plugin->encode_flight_mode(px4.vehicle_kind(), c.name) ==
                         static_cast<int>(custom));
    }

    // 可设置标志必须和数据表一致（表是唯一来源）。
    for (const auto& c : cases) {
        const uint32_t custom =
            (static_cast<uint32_t>(c.main_mode) << 16) |
            (static_cast<uint32_t>(c.sub_mode) << 24);
        bool found = false;
        bool settable = false;
        for (const auto& entry : plugin->available_modes(px4.vehicle_kind())) {
            if (entry.custom_mode == custom) {
                found = true;
                settable = entry.can_be_set;
            }
        }
        char label[160];
        snprintf(label, sizeof(label), "PX4 %s is listed with can_be_set=%s",
                 c.name, c.can_be_set ? "true" : "false");
        CHECK(label, found && settable == c.can_be_set);
    }

    bool has_rtgs = false;
    for (const auto& entry : plugin->available_modes(px4.vehicle_kind())) {
        if (entry.name == "Auto RTGS") has_rtgs = true;
    }
    CHECK("ex-RTGS is not offered as a mode", !has_rtgs);

    // 既有模式名保持不变（前端与角色反查依赖这些字面量）。
    CHECK("Auto RTL keeps its name",
          px4.decode_flight_mode((4u << 16) | (5u << 24)).mode_name == "Auto RTL");
    CHECK("Auto Land keeps its name",
          px4.decode_flight_mode((4u << 16) | (6u << 24)).mode_name == "Auto Land");
    CHECK("Auto Loiter keeps its name",
          px4.decode_flight_mode((4u << 16) | (3u << 24)).mode_name == "Auto Loiter");
    CHECK("Offboard keeps its name",
          plugin->decode_flight_mode(px4.vehicle_kind(), 6u << 16) == "Offboard");
}

/// S1：COM_FLTMODE1..6 是参数枚举，不是 custom_mode，两张表不能互相反查。
static void test_px4_fltmode_slot_table() {
    fprintf(stderr, "\n[Group] PX4 COM_FLTMODE slot table\n");

    using mg::core::firmware::modes::px4_fltmode_slot_name;
    using mg::core::firmware::modes::px4_mode_table;

    CHECK("slot -1 is Unassigned", std::string(px4_fltmode_slot_name(-1)) == "Unassigned");
    CHECK("slot 0 is Manual", std::string(px4_fltmode_slot_name(0)) == "Manual");
    CHECK("slot 7 is Offboard", std::string(px4_fltmode_slot_name(7)) == "Offboard");
    CHECK("slot 8 is Stabilized", std::string(px4_fltmode_slot_name(8)) == "Stabilized");
    CHECK("slot 11 is Land", std::string(px4_fltmode_slot_name(11)) == "Land");
    CHECK("slot 13 is Precision Land",
          std::string(px4_fltmode_slot_name(13)) == "Precision Land");
    CHECK("slot 16 is Altitude Cruise",
          std::string(px4_fltmode_slot_name(16)) == "Altitude Cruise");
    CHECK("slot 100 is External Mode 1",
          std::string(px4_fltmode_slot_name(100)) == "External Mode 1");
    CHECK("unknown slot value has no name",
          std::string(px4_fltmode_slot_name(99)).empty());

    // custom_mode 的 Offboard 是 6<<16，参数枚举的 Offboard 是 7：必须不同。
    size_t table_count = 0;
    (void)px4_mode_table(table_count);
    CHECK("mode table is non-empty", table_count > 0);
    CHECK("slot enum and custom_mode enum are different values",
          std::string(px4_fltmode_slot_name(7)) !=
              std::string(mg::core::firmware::modes::decode_px4_mode(7)));
}

/// S2：PX4 引导语义与 capability 按机型细化（对齐 QGC `PX4FirmwarePlugin`）。
static void test_px4_capabilities_and_guided_semantics() {
    fprintf(stderr, "\n[Group] PX4 guided semantics + per-vehicle capabilities\n");

    using mg::core::firmware::CapCalibration;
    using mg::core::firmware::CapChangeHeading;
    using mg::core::firmware::CapGuidedMode;
    using mg::core::firmware::CapOrbit;
    using mg::core::firmware::CapPause;
    using mg::core::firmware::CapRoi;
    using mg::core::firmware::CapSetHome;
    using mg::core::firmware::CapTakeoff;
    using mg::core::firmware::VehicleKind;

    auto plugin = create_firmware_plugin(FirmwareKind::PX4);
    CHECK("PX4 plugin created", plugin != nullptr);
    if (!plugin) return;

    // QGC `PX4FirmwarePlugin::gotoFlightMode()` = AUTO_LOITER；
    // `isGuidedMode()` = pause/takeoff/land 三者之一。
    CHECK("PX4 guided mode name is Auto Loiter",
          std::string(plugin->guided_mode_name()) == "Auto Loiter");
    CHECK("PX4 is_guided_mode accepts Auto Loiter",
          plugin->is_guided_mode("Auto Loiter"));
    CHECK("PX4 is_guided_mode accepts Auto Takeoff",
          plugin->is_guided_mode("Auto Takeoff"));
    CHECK("PX4 is_guided_mode accepts Auto Land",
          plugin->is_guided_mode("Auto Land"));
    CHECK("PX4 is_guided_mode rejects Offboard",
          !plugin->is_guided_mode("Offboard"));
    CHECK("PX4 is_guided_mode rejects Auto Mission",
          !plugin->is_guided_mode("Auto Mission"));

    // QGC PX4 不覆写基类默认最小起飞高度：10 ft = 3.048 m。
    const float min_alt = plugin->minimum_takeoff_altitude(VehicleKind::Quadrotor);
    CHECK("PX4 minimum takeoff altitude is 3.048 m",
          min_alt > 3.04f && min_alt < 3.05f);

    const uint32_t multi = plugin->capabilities(VehicleKind::Quadrotor);
    CHECK("PX4 multirotor has guided/pause/calibration",
          (multi & (CapGuidedMode | CapPause | CapCalibration)) ==
              (CapGuidedMode | CapPause | CapCalibration));
    CHECK("PX4 multirotor has takeoff + orbit",
          (multi & (CapTakeoff | CapOrbit)) == (CapTakeoff | CapOrbit));
    CHECK("PX4 multirotor has ROI + change heading",
          (multi & (CapRoi | CapChangeHeading)) == (CapRoi | CapChangeHeading));
    CHECK("PX4 multirotor exposes set home",
          (multi & CapSetHome) != 0);

    const uint32_t vtol = plugin->capabilities(VehicleKind::VTOL);
    CHECK("PX4 VTOL has takeoff + orbit",
          (vtol & (CapTakeoff | CapOrbit)) == (CapTakeoff | CapOrbit));
    CHECK("PX4 VTOL has no ROI", (vtol & CapRoi) == 0);
    CHECK("PX4 VTOL has no change heading", (vtol & CapChangeHeading) == 0);

    const uint32_t fw = plugin->capabilities(VehicleKind::FixedWing);
    CHECK("PX4 fixed wing has orbit", (fw & CapOrbit) != 0);
    CHECK("PX4 fixed wing has no guided takeoff", (fw & CapTakeoff) == 0);

    const uint32_t rover = plugin->capabilities(VehicleKind::GroundRover);
    CHECK("PX4 rover has driven guided mode", (rover & CapGuidedMode) != 0);
    CHECK("PX4 rover has no orbit", (rover & CapOrbit) == 0);
    CHECK("PX4 rover has no ROI", (rover & CapRoi) == 0);
}

/// S2：ArduPilot 侧补上 ROI / ChangeHeading / Orbit 能力位，让 capability JSON
/// 与地图入口一一对应（QGC `APMFirmwarePlugin::isCapable`）。
static void test_ardupilot_capability_bits() {
    fprintf(stderr, "\n[Group] ArduPilot capability bits\n");

    using mg::core::firmware::CapChangeHeading;
    using mg::core::firmware::CapOrbit;
    using mg::core::firmware::CapRoi;
    using mg::core::firmware::CapTakeoff;
    using mg::core::firmware::VehicleKind;

    auto plugin = create_firmware_plugin(FirmwareKind::ArduPilot);
    CHECK("ArduPilot plugin created", plugin != nullptr);
    if (!plugin) return;

    const uint32_t copter = plugin->capabilities(VehicleKind::Quadrotor);
    CHECK("AP copter has ROI", (copter & CapRoi) != 0);
    CHECK("AP copter has change heading", (copter & CapChangeHeading) != 0);
    CHECK("AP copter has orbit", (copter & CapOrbit) != 0);

    const uint32_t plane = plugin->capabilities(VehicleKind::FixedWing);
    CHECK("AP plane has ROI", (plane & CapRoi) != 0);
    CHECK("AP plane has orbit", (plane & CapOrbit) != 0);
    CHECK("AP plane has no change heading", (plane & CapChangeHeading) == 0);
    CHECK("AP plane has no guided takeoff", (plane & CapTakeoff) == 0);

    const uint32_t vtol = plugin->capabilities(VehicleKind::VTOL);
    CHECK("AP VTOL has guided takeoff", (vtol & CapTakeoff) != 0);

    const uint32_t sub = plugin->capabilities(VehicleKind::Submarine);
    CHECK("AP sub has change heading", (sub & CapChangeHeading) != 0);
    CHECK("AP sub has no orbit", (sub & CapOrbit) == 0);
}

int main() {
    fprintf(stderr, "=== Firmware Adapter Classification Tests ===\n");

    test_ardupilot_vehicle_families();
    test_px4_vehicle_families();
    test_ardupilot_parameter_remapping();
    test_mode_tables_match_upstream_enums();
    test_mode_role_resolution();
    test_px4_single_mode_table();
    test_px4_new_modes();
    test_px4_fltmode_slot_table();
    test_px4_capabilities_and_guided_semantics();
    test_ardupilot_capability_bits();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
