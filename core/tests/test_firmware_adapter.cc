#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"
#include "core/firmware/modes/px4_modes.h"
#include "core/firmware/param_remapping.h"

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
using mg::core::firmware::ParamRemapping;
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

    FirmwareAdapter tracker;
    tracker.detect(3, 5);
    CHECK(tracker.normalized_vehicle_name() == "AntennaTracker", "AP tracker normalized family");
}

static void test_px4_vehicle_families() {
    fprintf(stderr, "\n[Group] PX4 normalized vehicle families\n");

    FirmwareAdapter quad;
    quad.detect(12, 2);
    CHECK(quad.firmware_name() == "PX4", "PX4 quad firmware");
    CHECK(quad.normalized_vehicle_name() == "Copter", "PX4 quad normalized family");

    FirmwareAdapter plane;
    plane.detect(12, 1);
    CHECK(plane.normalized_vehicle_name() == "FixedWing", "PX4 plane normalized family");

    FirmwareAdapter boat;
    boat.detect(12, 11);
    CHECK(boat.normalized_vehicle_name() == "Rover", "PX4 boat normalized family");

    FirmwareAdapter sub;
    sub.detect(12, 12);
    CHECK(sub.normalized_vehicle_name() == "Submarine", "PX4 submarine normalized family");

    FirmwareAdapter vtol;
    vtol.detect(12, 20);
    CHECK(vtol.normalized_vehicle_name() == "VTOL", "PX4 VTOL normalized family");
}

static void test_ardupilot_parameter_remapping() {
    fprintf(stderr, "\n[Group] ArduPilot parameter remapping\n");
    ParamRemapping remap;
    remap.load_ardupilot_copter_tables();
    CHECK(remap.remap(4, 5, "WP_SPD") == "WPNAV_SPEED", "4.7 new WP name maps to old WPNAV name");
    CHECK(remap.remap(4, 7, "WP_SPD") == "WP_SPD", "4.7 firmware keeps new parameter name");
    CHECK(remap.remap(4, 5, "UNKNOWN_PARAM") == "UNKNOWN_PARAM", "unknown parameter name passes through");
    CHECK(remap.canonical_name(4, 5, "WPNAV_SPEED") == "WP_SPD", "old firmware name reverse-maps to canonical");
    CHECK(remap.canonical_name(4, 5, "UNKNOWN_PARAM") == "UNKNOWN_PARAM", "canonical reverse mapping keeps unknown names");

    ParamRemapping plane;
    plane.load_ardupilot_plane_tables();
    CHECK(plane.remap(4, 5, "Q_PILOT_SPD_DN") == "Q_PILOT_SPEED_DN", "plane Q pilot speed alias maps for old firmware");
    CHECK(plane.remap(4, 5, "ARMING_SKIPCHK") == "ARMING_CHECK", "arming skip-check alias maps for old firmware");

    // Plane 4.5 段此前整段缺失（QGC ArduPlaneFirmwarePlugin.cc 的 remapV4_5）。
    CHECK(plane.remap(4, 4, "AIRSPEED_MIN") == "ARSPD_FBW_MIN", "plane 4.5 airspeed rename maps");
    CHECK(plane.remap(4, 4, "AIRSPEED_MAX") == "ARSPD_FBW_MAX", "plane 4.5 airspeed max rename maps");
    CHECK(plane.remap(4, 4, "RTL_ALTITUDE") == "ALT_HOLD_RTL", "plane 4.5 RTL altitude rename maps");

    // QuadPlane 的 4.7 改名（QGC remapV4_7 的 Q_* 段）。
    CHECK(plane.remap(4, 5, "Q_WP_RADIUS_M") == "Q_WP_RADIUS", "quadplane waypoint rename maps");
    CHECK(plane.remap(4, 5, "Q_P_NE_VEL_P") == "Q_P_VELXY_P", "quadplane position-controller rename maps");

    // 编造条目必须不存在：这些"新名"在 ArduPilot 与 QGC 里都没有，
    // 却曾被当成 4.7 的规范名，把飞控报的旧名错误地规范化成不存在的参数。
    // 负例查询要选"旧名"侧（canonical_name 才走 old->new 方向）。
    CHECK(plane.canonical_name(4, 5, "TECS_CLMB_MAX") == "TECS_CLMB_MAX", "no fabricated TECS rename");
    CHECK(plane.canonical_name(4, 5, "ARSPD_FBW_MIN") != "ARSPD_FBW_MIN_M", "no fabricated ARSPD rename");

    // Rover 与 QGC 一致：只有 EK3_FLOW_MAX 与 ARMING_SKIPCHK 两条改名。
    ParamRemapping rover;
    rover.load_ardupilot_rover_tables();
    CHECK(rover.remap(4, 5, "EK3_FLOW_MAX") == "EK3_MAX_FLOW", "rover EKF rename maps");
    CHECK(rover.remap(4, 5, "ARMING_SKIPCHK") == "ARMING_CHECK", "rover arming rename maps");
    CHECK(rover.canonical_name(4, 5, "CRUISE_SPEED") == "CRUISE_SPEED", "no fabricated rover cruise rename");
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

    FirmwareAdapter px4;
    px4.detect(12, 2);  // MAV_AUTOPILOT_PX4
    const auto px4_rtl = px4.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch);
    CHECK(px4_rtl.has_value() && *px4_rtl != 6u, "PX4 RTL role resolves to a packed custom_mode");
    CHECK(px4_rtl.has_value() && px4.decode_flight_mode(*px4_rtl).mode_name == "Auto RTL", "PX4 RTL decodes back to Auto RTL");

    FirmwareAdapter unknown;
    CHECK(!unknown.resolve_mode_role(FirmwareAdapter::ModeRole::ReturnToLaunch).has_value(), "Unknown firmware resolves nothing");
    CHECK(copter.encode_flight_mode("rtl") == copter.encode_flight_mode("RTL"), "Name lookup is case insensitive");
    CHECK(!copter.encode_flight_mode("WarpDrive").has_value(), "Unknown mode name resolves to nullopt");
}

/// S1：PX4 只允许存在一份模式表。插件（可用模式列表 / 角色反查）与 adapter
/// （状态栏解码）必须对任意 custom_mode 给出同一个名字，包括兜底串。
static void test_px4_single_mode_table() {
    fprintf(stderr, "\n[Group] PX4 single mode table (plugin vs adapter)\n");

    FirmwareAdapter px4;
    px4.detect(12, 2);  // MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR
    auto plugin = create_firmware_plugin(FirmwareKind::PX4);
    CHECK(plugin != nullptr, "PX4 plugin factory returns a plugin");
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
    CHECK(mismatches == 0, "every custom_mode decodes identically in plugin and adapter");

    // 已删除/保留的取值不再伪装成模式。
    CHECK(px4.decode_flight_mode((4u << 16) | (7u << 24)).mode_name ==
              "Mode(" + std::to_string((4u << 16) | (7u << 24)) + ")", "AUTO sub-mode 7 (ex-RTGS) has no table entry");
    CHECK(px4.decode_flight_mode((4u << 16)).mode_name == "Mode(262144)", "AUTO sub-mode 0 falls back to a placeholder");
    CHECK(px4.decode_flight_mode(0).mode_name == "Mode(0)", "main mode 0 decodes to a placeholder");
}

/// S1：PX4 1.14+ 新模式（Altitude Cruise、POSCTL Orbit/Slow、VTOL Takeoff、
/// Guided Course）必须能编码、解码，且可设置标志符合决策。
static void test_px4_new_modes() {
    fprintf(stderr, "\n[Group] PX4 new modes encode/decode\n");

    FirmwareAdapter px4;
    px4.detect(12, 2);
    auto plugin = create_firmware_plugin(FirmwareKind::PX4);
    if (!plugin) {
        CHECK(false, "PX4 plugin available for new-mode test");
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
        CHECK(px4.decode_flight_mode(custom).mode_name == c.name, label);

        snprintf(label, sizeof(label), "PX4 %s re-encodes to %u", c.name, custom);
        const auto encoded = px4.encode_flight_mode(c.name);
        CHECK(encoded.has_value() && *encoded == custom, label);

        snprintf(label, sizeof(label), "PX4 plugin %s re-encodes to %u", c.name, custom);
        CHECK(plugin->encode_flight_mode(px4.vehicle_kind(), c.name) ==
                         static_cast<int>(custom), label);
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
        CHECK(found && settable == c.can_be_set, label);
    }

    bool has_rtgs = false;
    for (const auto& entry : plugin->available_modes(px4.vehicle_kind())) {
        if (entry.name == "Auto RTGS") has_rtgs = true;
    }
    CHECK(!has_rtgs, "ex-RTGS is not offered as a mode");

    // 既有模式名保持不变（前端与角色反查依赖这些字面量）。
    CHECK(px4.decode_flight_mode((4u << 16) | (5u << 24)).mode_name == "Auto RTL", "Auto RTL keeps its name");
    CHECK(px4.decode_flight_mode((4u << 16) | (6u << 24)).mode_name == "Auto Land", "Auto Land keeps its name");
    CHECK(px4.decode_flight_mode((4u << 16) | (3u << 24)).mode_name == "Auto Loiter", "Auto Loiter keeps its name");
    CHECK(plugin->decode_flight_mode(px4.vehicle_kind(), 6u << 16) == "Offboard", "Offboard keeps its name");
}

/// S1：COM_FLTMODE1..6 是参数枚举，不是 custom_mode，两张表不能互相反查。
static void test_px4_fltmode_slot_table() {
    fprintf(stderr, "\n[Group] PX4 COM_FLTMODE slot table\n");

    using mg::core::firmware::modes::px4_fltmode_slot_name;
    using mg::core::firmware::modes::px4_mode_table;

    CHECK(std::string(px4_fltmode_slot_name(-1)) == "Unassigned", "slot -1 is Unassigned");
    CHECK(std::string(px4_fltmode_slot_name(0)) == "Manual", "slot 0 is Manual");
    CHECK(std::string(px4_fltmode_slot_name(7)) == "Offboard", "slot 7 is Offboard");
    CHECK(std::string(px4_fltmode_slot_name(8)) == "Stabilized", "slot 8 is Stabilized");
    CHECK(std::string(px4_fltmode_slot_name(11)) == "Land", "slot 11 is Land");
    CHECK(std::string(px4_fltmode_slot_name(13)) == "Precision Land", "slot 13 is Precision Land");
    CHECK(std::string(px4_fltmode_slot_name(16)) == "Altitude Cruise", "slot 16 is Altitude Cruise");
    CHECK(std::string(px4_fltmode_slot_name(100)) == "External Mode 1", "slot 100 is External Mode 1");
    CHECK(std::string(px4_fltmode_slot_name(99)).empty(), "unknown slot value has no name");

    // custom_mode 的 Offboard 是 6<<16，参数枚举的 Offboard 是 7：必须不同。
    size_t table_count = 0;
    (void)px4_mode_table(table_count);
    CHECK(table_count > 0, "mode table is non-empty");
    CHECK(std::string(px4_fltmode_slot_name(7)) !=
              std::string(mg::core::firmware::modes::decode_px4_mode(7)), "slot enum and custom_mode enum are different values");
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
    CHECK(plugin != nullptr, "PX4 plugin created");
    if (!plugin) return;

    // QGC `PX4FirmwarePlugin::gotoFlightMode()` = AUTO_LOITER；
    // `isGuidedMode()` = pause/takeoff/land 三者之一。
    CHECK(std::string(plugin->guided_mode_name()) == "Auto Loiter", "PX4 guided mode name is Auto Loiter");
    CHECK(plugin->is_guided_mode("Auto Loiter"), "PX4 is_guided_mode accepts Auto Loiter");
    CHECK(plugin->is_guided_mode("Auto Takeoff"), "PX4 is_guided_mode accepts Auto Takeoff");
    CHECK(plugin->is_guided_mode("Auto Land"), "PX4 is_guided_mode accepts Auto Land");
    CHECK(!plugin->is_guided_mode("Offboard"), "PX4 is_guided_mode rejects Offboard");
    CHECK(!plugin->is_guided_mode("Auto Mission"), "PX4 is_guided_mode rejects Auto Mission");

    // QGC PX4 不覆写基类默认最小起飞高度：10 ft = 3.048 m。
    const float min_alt = plugin->minimum_takeoff_altitude(VehicleKind::Quadrotor);
    CHECK(min_alt > 3.04f && min_alt < 3.05f, "PX4 minimum takeoff altitude is 3.048 m");

    const uint32_t multi = plugin->capabilities(VehicleKind::Quadrotor);
    CHECK((multi & (CapGuidedMode | CapPause | CapCalibration)) ==
              (CapGuidedMode | CapPause | CapCalibration), "PX4 multirotor has guided/pause/calibration");
    CHECK((multi & (CapTakeoff | CapOrbit)) == (CapTakeoff | CapOrbit), "PX4 multirotor has takeoff + orbit");
    CHECK((multi & (CapRoi | CapChangeHeading)) == (CapRoi | CapChangeHeading), "PX4 multirotor has ROI + change heading");
    CHECK((multi & CapSetHome) != 0, "PX4 multirotor exposes set home");

    const uint32_t vtol = plugin->capabilities(VehicleKind::VTOL);
    CHECK((vtol & (CapTakeoff | CapOrbit)) == (CapTakeoff | CapOrbit), "PX4 VTOL has takeoff + orbit");
    CHECK((vtol & CapRoi) == 0, "PX4 VTOL has no ROI");
    CHECK((vtol & CapChangeHeading) == 0, "PX4 VTOL has no change heading");

    const uint32_t fw = plugin->capabilities(VehicleKind::FixedWing);
    CHECK((fw & CapOrbit) != 0, "PX4 fixed wing has orbit");
    CHECK((fw & CapTakeoff) == 0, "PX4 fixed wing has no guided takeoff");

    const uint32_t rover = plugin->capabilities(VehicleKind::GroundRover);
    CHECK((rover & CapGuidedMode) != 0, "PX4 rover has driven guided mode");
    CHECK((rover & CapOrbit) == 0, "PX4 rover has no orbit");
    CHECK((rover & CapRoi) == 0, "PX4 rover has no ROI");
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
    test_px4_vehicle_families();
    test_ardupilot_parameter_remapping();
    test_mode_tables_match_upstream_enums();
    test_ardupilot_single_mode_table();
    test_ardupilot_mode_values_match_upstream();
    test_mode_role_resolution();
    test_px4_single_mode_table();
    test_px4_new_modes();
    test_px4_fltmode_slot_table();
    test_px4_capabilities_and_guided_semantics();
    test_ardupilot_capability_bits();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
