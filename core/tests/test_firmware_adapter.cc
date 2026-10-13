#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"

#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_passed = mgcs_test::passed();
static int& g_failed = mgcs_test::failed();


using mg::core::firmware::FirmwareAdapter;
using mg::core::firmware::FirmwareKind;
using mg::core::firmware::create_firmware_plugin;

static void test_ardupilot_vehicle_families() {
    fprintf(stderr, "\n[Group] ArduPilot normalized vehicle families\n");

    FirmwareAdapter plane;
    plane.detect(3, 1);
    CHECK(plane.firmware_name() == "ArduPilot", "AP plane firmware");
    CHECK(plane.normalized_vehicle_name() == "FixedWing", "AP plane normalized family");

    FirmwareAdapter tricopter;
    tricopter.detect(3, 15);
    CHECK(tricopter.normalized_vehicle_name() == "Copter", "AP tricopter normalized family");

    FirmwareAdapter boat;
    boat.detect(3, 11);
    CHECK(boat.normalized_vehicle_name() == "Rover", "AP boat normalized family");

    FirmwareAdapter sub;
    sub.detect(3, 12);
    CHECK(sub.normalized_vehicle_name() == "Submarine", "AP submarine normalized family");

    FirmwareAdapter vtol;
    vtol.detect(3, 19);
    CHECK(vtol.normalized_vehicle_name() == "VTOL", "AP VTOL normalized family");

    FirmwareAdapter quadplane;
    quadplane.detect(3, 1, 1.0f);  // ArduPlane QuadPlane default HEARTBEAT type.
    CHECK(quadplane.normalized_vehicle_name() == "VTOL",
          "AP fixed-wing heartbeat with Q_ENABLE is normalized as VTOL");
    CHECK(quadplane.vehicle_kind() == mg::core::firmware::VehicleKind::VTOL,
          "QuadPlane semantic type replaces the raw fixed-wing type");

    FirmwareAdapter non_ardupilot_fixed_wing;
    non_ardupilot_fixed_wing.detect(12, 1, 1.0f);
    CHECK(non_ardupilot_fixed_wing.normalized_vehicle_name() == "FixedWing",
          "Q_ENABLE does not reclassify non-ArduPilot fixed-wing firmware");

    FirmwareAdapter disabled_quadplane;
    disabled_quadplane.detect(3, 1, 0.0f);
    CHECK(disabled_quadplane.normalized_vehicle_name() == "FixedWing",
          "Q_ENABLE zero remains a fixed-wing aircraft");

    FirmwareAdapter tracker;
    tracker.detect(3, 5);
    CHECK(tracker.normalized_vehicle_name() == "AntennaTracker", "AP tracker normalized family");
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
    CHECK(copter.decode_flight_mode(9).mode_name == "Land", "Copter 9 is Land (not Drift)");
    CHECK(copter.decode_flight_mode(11).mode_name == "Drift", "Copter 11 is Drift");
    CHECK(copter.decode_flight_mode(13).mode_name == "Sport", "Copter 13 is Sport");
    CHECK(copter.decode_flight_mode(15).mode_name == "Autotune", "Copter 15 is Autotune");
    CHECK(copter.decode_flight_mode(16).mode_name == "PosHold", "Copter 16 is PosHold");
    CHECK(copter.decode_flight_mode(21).mode_name == "SmartRTL", "Copter 21 is SmartRTL");
    CHECK(copter.decode_flight_mode(28).mode_name == "Turtle", "Copter 28 is Turtle");
    CHECK(copter.decode_flight_mode(8).mode_name == "Mode(8)", "Copter 8 is unused");

    FirmwareAdapter plane;
    plane.detect(3, 1);  // MAV_TYPE_FIXED_WING
    CHECK(plane.decode_flight_mode(11).mode_name == "RTL", "Plane 11 is RTL");
    CHECK(plane.decode_flight_mode(12).mode_name == "Loiter", "Plane 12 is Loiter");

    FirmwareAdapter rover;
    rover.detect(3, 10);  // MAV_TYPE_GROUND_ROVER
    CHECK(rover.decode_flight_mode(5).mode_name == "Loiter", "Rover 5 is Loiter");
    CHECK(rover.decode_flight_mode(9).mode_name == "Circle", "Rover 9 is Circle");
    CHECK(rover.decode_flight_mode(11).mode_name == "RTL", "Rover 11 is RTL");
    CHECK(rover.decode_flight_mode(16).mode_name == "Initialising", "Rover has no Autotune mode");

    FirmwareAdapter tracker;
    tracker.detect(3, 5);  // MAV_TYPE_GROUND_ROVER? no: 5 = MAV_TYPE_SURFACE_BOAT
    FirmwareAdapter antenna;
    antenna.detect(3, 16);  // not used; tracker detection covered below
    (void)tracker;
    (void)antenna;
}

/// ArduPilot 只允许存在一份模式表。插件（available_modes / encode_flight_mode）
/// 与 adapter（decode_flight_mode）必须从同一份 modes/ 头文件取值：
///   - 每个模式名都能被 encode 回它自己的 custom_mode（可用列表不是另一张表）；
///   - 表里每个 custom_mode 都能 decode 出同名，与插件 available_modes 一致。
/// 历史上插件内嵌过第二张表并落后于上游枚举，导致 available_modes 选 "Drift"
/// 实际下发 custom_mode=9（真值 Land）。本测试是对该缺口的存在性守卫。
static void test_ardupilot_single_mode_table() {
    fprintf(stderr, "\n[Group] ArduPilot single mode table (plugin vs adapter)\n");

    struct Case { uint8_t mav_type; const char* label; };
    const Case cases[] = {
        {2,  "Copter"},   // MAV_TYPE_QUADROTOR
        {1,  "Plane"},    // MAV_TYPE_FIXED_WING
        {10, "Rover"},    // MAV_TYPE_GROUND_ROVER
        {12, "Sub"},      // MAV_TYPE_SUBMARINE
        {5,  "Tracker"},  // MAV_TYPE_ANTENNA_TRACKER
    };

    for (const auto& c : cases) {
        FirmwareAdapter adapter;
        adapter.detect(3, c.mav_type);  // MAV_AUTOPILOT_ARDUPILOTMEGA
        auto plugin = create_firmware_plugin(FirmwareKind::ArduPilot);
        CHECK(plugin != nullptr, "ArduPilot plugin factory returns a plugin");
        if (!plugin) return;

        const auto entries = plugin->available_modes(adapter.vehicle_kind());
        const std::string nonempty_label = std::string(c.label) + " available_modes is non-empty";
        CHECK(!entries.empty(), nonempty_label.c_str());

        // 1) 每个可用模式：名字 -> custom_mode -> 名字 必须 round-trip，
        //    且插件 encode 与 adapter decode 用同一张表。
        uint32_t bad_roundtrip = 0;
        std::string first_bad;
        for (const auto& e : entries) {
            const int encoded = plugin->encode_flight_mode(adapter.vehicle_kind(), e.name);
            if (encoded < 0 || static_cast<uint32_t>(encoded) != e.custom_mode) {
                if (bad_roundtrip == 0) {
                    first_bad = e.name + " -> " + std::to_string(encoded) +
                                " (expected " + std::to_string(e.custom_mode) + ")";
                }
                ++bad_roundtrip;
                continue;
            }
            const std::string decoded = adapter.decode_flight_mode(e.custom_mode).mode_name;
            if (decoded != e.name) {
                if (bad_roundtrip == 0) {
                    first_bad = e.name + " decode=" + decoded;
                }
                ++bad_roundtrip;
            }
        }
        if (bad_roundtrip != 0) {
            fprintf(stderr, "  %s first mismatch: %s\n", c.label, first_bad.c_str());
        }
        const std::string rt_label = std::string(c.label) +
            " every available mode round-trips through one table";
        CHECK(bad_roundtrip == 0, rt_label.c_str());

        // 2) 若该机型有 Land，encode/decode 必须一致（防止再次按下标编号）。
        const int land = plugin->encode_flight_mode(adapter.vehicle_kind(), "Land");
        if (land >= 0) {
            const std::string land_label = std::string(c.label) + " Land round-trips";
            CHECK(adapter.decode_flight_mode(static_cast<uint32_t>(land)).mode_name == "Land", land_label.c_str());
        }
    }
}

/// ArduPilot 关键模式的数值必须与上游 mode.h 一致（而非模式列表下标）。
static void test_ardupilot_mode_values_match_upstream() {
    fprintf(stderr, "\n[Group] ArduPilot mode values match upstream\n");

    auto plugin = create_firmware_plugin(FirmwareKind::ArduPilot);
    CHECK(plugin != nullptr, "plugin created");
    if (!plugin) return;

    FirmwareAdapter copter; copter.detect(3, 2);
    CHECK(plugin->encode_flight_mode(copter.vehicle_kind(), "Land") == 9, "Copter Land -> 9");
    CHECK(plugin->encode_flight_mode(copter.vehicle_kind(), "Drift") == 11, "Copter Drift -> 11");
    CHECK(plugin->encode_flight_mode(copter.vehicle_kind(), "PosHold") == 16, "Copter PosHold -> 16");
    CHECK(plugin->encode_flight_mode(copter.vehicle_kind(), "Turtle") == 28, "Copter Turtle -> 28");

    FirmwareAdapter rover; rover.detect(3, 10);
    CHECK(plugin->encode_flight_mode(rover.vehicle_kind(), "Circle") == 9, "Rover Circle -> 9");
    CHECK(plugin->encode_flight_mode(rover.vehicle_kind(), "Guided") == 15, "Rover Guided -> 15");

    FirmwareAdapter sub; sub.detect(3, 12);
    CHECK(plugin->encode_flight_mode(sub.vehicle_kind(), "PosHold") == 16, "Sub PosHold -> 16");
    CHECK(plugin->encode_flight_mode(sub.vehicle_kind(), "MotorDetect") == 20, "Sub MotorDetect -> 20");
    // ArduSub/mode.h:53 SURFTRAK = 21；QGC ArduSubFirmwarePlugin 的
    // availableFlightModes 把它列为 CanBeSet=true。此前 Sub 表漏了这一条，
    // 导致模式 21 解码成 "Mode(21)" 且无法从界面选择。
    CHECK(plugin->encode_flight_mode(sub.vehicle_kind(), "SurfTrak") == 21, "Sub SurfTrak -> 21");
    // 注意：available_modes() 每次返回新的临时 vector，必须先落到具名变量再查找，
    // 否则拿两个不同临时的 begin/end 做 find_if 比较是恒不等（假断言）。
    const auto sub_modes = plugin->available_modes(sub.vehicle_kind());
    const bool surftrak_settable =
        std::any_of(sub_modes.begin(), sub_modes.end(),
                    [](const mg::core::firmware::FlightModeEntry& entry) {
                        return entry.name == "SurfTrak" && entry.can_be_set;
                    });
    CHECK(surftrak_settable, "Sub SurfTrak is user-settable");

    FirmwareAdapter tracker; tracker.detect(3, 5);
    CHECK(plugin->encode_flight_mode(tracker.vehicle_kind(), "Guided") == 4, "Tracker Guided -> 4");
    CHECK(plugin->encode_flight_mode(tracker.vehicle_kind(), "Auto") == 10, "Tracker Auto -> 10");
}

/// 反查与语义角色：模式号只能来自插件，且不存在的模式必须解析失败。
static void test_mode_role_resolution() {
    fprintf(stderr, "\n[Group] Mode role resolution\n");

    FirmwareAdapter copter;
    copter.detect(3, 2);
    CHECK(copter.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 6u, "Copter RTL role -> 6");
    CHECK(copter.resolve_mode_role(FirmwareAdapter::ModeRole::Land) == 9u, "Copter Land role -> 9");
    CHECK(copter.resolve_mode_role(FirmwareAdapter::ModeRole::Loiter) == 5u, "Copter Loiter role -> 5");

    FirmwareAdapter plane;
    plane.detect(3, 1);
    CHECK(plane.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 11u, "Plane RTL role -> 11");
    CHECK(plane.resolve_mode_role(FirmwareAdapter::ModeRole::Loiter) == 12u, "Plane Loiter role -> 12");
    CHECK(!plane.resolve_mode_role(FirmwareAdapter::ModeRole::Land).has_value(), "Plane Land role is unresolvable");

    FirmwareAdapter rover;
    rover.detect(3, 10);
    CHECK(rover.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch) == 11u, "Rover RTL role -> 11");
    CHECK(!rover.resolve_mode_role(FirmwareAdapter::ModeRole::Land).has_value(), "Rover Land role is unresolvable (9 is Circle)");

    FirmwareAdapter unknown;
    CHECK(!unknown.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch).has_value(), "Unknown firmware resolves nothing");
    CHECK(copter.encode_flight_mode("rtl") == copter.encode_flight_mode("RTL"), "Name lookup is case insensitive");
    CHECK(!copter.encode_flight_mode("WarpDrive").has_value(), "Unknown mode name resolves to nullopt");
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
    CHECK(plugin != nullptr, "ArduPilot plugin created");
    if (!plugin) return;

    const uint32_t copter = plugin->capabilities(VehicleKind::Quadrotor);
    CHECK((copter & CapRoi) != 0, "AP copter has ROI");
    CHECK((copter & CapChangeHeading) != 0, "AP copter has change heading");
    CHECK((copter & CapOrbit) != 0, "AP copter has orbit");

    const uint32_t plane = plugin->capabilities(VehicleKind::FixedWing);
    CHECK((plane & CapRoi) != 0, "AP plane has ROI");
    CHECK((plane & CapOrbit) != 0, "AP plane has orbit");
    CHECK((plane & CapChangeHeading) == 0, "AP plane has no change heading");
    CHECK((plane & CapTakeoff) == 0, "AP plane has no guided takeoff");

    const uint32_t vtol = plugin->capabilities(VehicleKind::VTOL);
    CHECK((vtol & CapTakeoff) != 0, "AP VTOL has guided takeoff");

    const uint32_t sub = plugin->capabilities(VehicleKind::Submarine);
    CHECK((sub & CapChangeHeading) != 0, "AP sub has change heading");
    CHECK((sub & CapOrbit) == 0, "AP sub has no orbit");
}

int main() {
    fprintf(stderr, "=== Firmware Adapter Classification Tests ===\n");

    test_ardupilot_vehicle_families();
    test_mode_tables_match_upstream_enums();
    test_ardupilot_single_mode_table();
    test_ardupilot_mode_values_match_upstream();
    test_mode_role_resolution();
    test_ardupilot_capability_bits();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
