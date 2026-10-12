#include "core/config/param_descriptors.h"

#include <cstdio>
#include <set>
#include <string>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_passed = mgcs_test::passed();
static int& g_failed = mgcs_test::failed();


using mg::core::config::ParamDescriptors;

static bool contains_localized_text(const std::string& value) {
    for (unsigned char ch : value) {
        if (ch >= 0x80) return true;
    }
    return false;
}

struct TranslationCoverage {
    int total = 0;
    int localized_names = 0;
    int localized_descriptions = 0;
};

static TranslationCoverage translation_coverage(const ParamDescriptors& db,
                                                const std::string& profile) {
    std::set<std::string> parameter_names;
    for (const auto& group : db.get_all_groups(profile)) {
        parameter_names.insert(group.param_names.begin(), group.param_names.end());
    }

    TranslationCoverage coverage;
    coverage.total = static_cast<int>(parameter_names.size());
    for (const auto& parameter_name : parameter_names) {
        const auto descriptor = db.get(profile, parameter_name);
        if (contains_localized_text(descriptor.display_name)) coverage.localized_names++;
        if (contains_localized_text(descriptor.description)) coverage.localized_descriptions++;
    }
    return coverage;
}

static std::string descriptor_path() {
#ifdef MGCORE_TEST_ROOT
    return std::string(MGCORE_TEST_ROOT) + "/core/src/config/ParameterDescriptors.json";
#else
    return "core/src/config/ParameterDescriptors.json";
#endif
}

static std::string translation_path() {
#ifdef MGCORE_TEST_ROOT
    return std::string(MGCORE_TEST_ROOT) + "/core/src/config/ParameterTranslations.zh-CN.json";
#else
    return "core/src/config/ParameterTranslations.zh-CN.json";
#endif
}

static void test_ardupilot_profiles() {
    fprintf(stderr, "\n[Group] ArduPilot parameter metadata profiles\n");

    ParamDescriptors db(descriptor_path());
    CHECK(db.is_loaded(), "descriptor database loaded");

    const auto copter_batt = db.get("copter", "BATT_MONITOR");
    CHECK(copter_batt.found, "copter BATT_MONITOR found");
    CHECK(copter_batt.display_name == "电池监测器", "copter BATT_MONITOR display");
    CHECK(copter_batt.type == "enum", "copter BATT_MONITOR enum");
    CHECK(copter_batt.options.size() >= 10, "copter BATT_MONITOR options");

    const auto plane_batt = db.get("plane", "BATT_MONITOR");
    CHECK(plane_batt.found, "plane BATT_MONITOR found");
    CHECK(plane_batt.requires_reboot, "plane BATT_MONITOR reboot");

    const auto vtol_batt = db.get("vtol", "BATT_MONITOR");
    CHECK(vtol_batt.found, "vtol inherits plane BATT_MONITOR");

    const auto vtol_enable = db.get("vtol", "Q_ENABLE");
    CHECK(vtol_enable.found, "vtol Q_ENABLE found");
    CHECK(vtol_enable.display_name == "启用 QuadPlane", "vtol Q_ENABLE display");
    CHECK(db.enum_label("vtol", "Q_ENABLE", 2) == "启用 VTOL AUTO (Enable VTOL AUTO)", "vtol Q_ENABLE enum label");

    const auto copter_q = db.get("copter", "Q_ENABLE");
    CHECK(!copter_q.found, "copter Q_ENABLE not found");

    const auto gps_rate = db.get("copter", "GPS1_RATE_MS");
    CHECK(gps_rate.found, "GPS1_RATE_MS found");
    CHECK(!gps_rate.unit.empty(), "GPS1_RATE_MS unit");
    CHECK(gps_rate.min_value != gps_rate.max_value, "GPS1_RATE_MS range");

    CHECK(db.validate("copter", "BARO1_DEVID", 1.0f) == "Parameter is read-only", "read-only descriptor is rejected");
    CHECK(db.validate("copter", "ACRO_RP_RATE", -1.0f).find("between") != std::string::npos, "range validation still works");

    const auto coverage = db.coverage("copter", {"BATT_CAPACITY", "MGC_UNKNOWN"});
    CHECK(coverage.total == 2 && coverage.described == 1, "coverage counts described parameters");
    CHECK(coverage.fallback == 1 && coverage.missing.size() == 1 &&
              coverage.missing.front() == "MGC_UNKNOWN", "coverage lists fallback parameters");
    const auto canonical_coverage = db.coverage(
        "copter", {"WP_SPD", "WP_RADIUS_M", "WP_SPD_UP"});
    CHECK(canonical_coverage.described == canonical_coverage.total &&
              canonical_coverage.fallback == 0, "canonical ArduPilot names are covered");
}

static void test_chinese_translation_overlay() {
    fprintf(stderr, "\n[Group] Chinese parameter translation overlay\n");

    ParamDescriptors db(descriptor_path(), translation_path());
    CHECK(db.is_loaded(), "descriptor database with translation loaded");

    const auto vtol_enable = db.get("vtol", "Q_ENABLE");
    CHECK(vtol_enable.display_name == "启用 QuadPlane", "Q_ENABLE Chinese display");
    CHECK(vtol_enable.description.find("启用 QuadPlane 功能") != std::string::npos, "Q_ENABLE Chinese description");
    CHECK(db.enum_label("vtol", "Q_ENABLE", 2).find("启用 VTOL AUTO") != std::string::npos &&
          db.enum_label("vtol", "Q_ENABLE", 2).find("Enable VTOL AUTO") != std::string::npos, "Q_ENABLE bilingual option label");

    const auto batt = db.get_group("copter", "BATT");
    CHECK(batt.display_name == "电池", "BATT group Chinese display");

    const auto monitor = db.get("copter", "BATT_MONITOR");
    CHECK(monitor.display_name == "电池监测器", "BATT_MONITOR Chinese display");
    CHECK(monitor.options.size() >= 10, "BATT_MONITOR keeps enum options");

    const auto serial1 = db.get("copter", "SERIAL1_PROTOCOL");
    CHECK(serial1.display_name == "串口协议", "SERIAL1_PROTOCOL Chinese display");
    CHECK(contains_localized_text(db.enum_label("copter", "SERIAL1_PROTOCOL", 2)) &&
          db.enum_label("copter", "SERIAL1_PROTOCOL", 2).find("MAVLink") != std::string::npos, "SERIAL1_PROTOCOL bilingual option label");
    CHECK(contains_localized_text(db.enum_label("copter", "SERIAL1_PROTOCOL", 7)), "SERIAL1_PROTOCOL compound option label");

    CHECK(db.enum_label("copter", "FLTMODE1", 0) == "自稳 (Stabilize)", "Copter flight mode option label");

    CHECK(db.enum_label("plane", "FLTMODE1", 5) == "电传 A (FBWA)", "Plane flight mode option label");

    const auto servo = db.get("copter", "SERVO1_FUNCTION");
    CHECK(contains_localized_text(db.enum_label("copter", "SERVO1_FUNCTION", 6)), "Servo function option label");

    CHECK(db.enum_label("copter", "RC10_OPTION", 9) == "相机触发 (Camera Trigger)", "RC option label");

    const auto copter_accel = db.get("copter", "ATC_ACC_P_MAX");
    CHECK(copter_accel.display_name.find("姿态控制") != std::string::npos &&
          copter_accel.display_name.find("加速度") != std::string::npos, "Copter fallback Chinese display");
    // ArduPilot 的 @Description 是 GPLv3 文本，不得回填（ADR-0002）。这里断言
    // 描述是我们自己的中文模板，且不含上游英文原文。
    CHECK(contains_localized_text(copter_accel.description),
          "Copter fallback description is our own Chinese template");
    CHECK(copter_accel.description.find("Maximum acceleration in pitch axis") == std::string::npos,
          "Copter fallback description carries no ArduPilot English prose");
    CHECK(db.enum_label("copter", "ATC_ACC_P_MAX", 0) == "禁用 (Disabled)", "Copter fallback common option label");

    const auto plane_tecs = db.get("plane", "TECS_SPDWEIGHT");
    CHECK(plane_tecs.display_name.find("速度控制权重") != std::string::npos, "Plane fallback Chinese display");
    CHECK(contains_localized_text(plane_tecs.description), "Plane fallback Chinese description");

    const auto copter_coverage = translation_coverage(db, "copter");
    const auto plane_coverage = translation_coverage(db, "plane");
    fprintf(stderr,
            "  [INFO] Copter localized display coverage: %d/%d; descriptions: %d/%d\n",
            copter_coverage.localized_names,
            copter_coverage.total,
            copter_coverage.localized_descriptions,
            copter_coverage.total);
    fprintf(stderr,
            "  [INFO] Plane localized display coverage: %d/%d; descriptions: %d/%d\n",
            plane_coverage.localized_names,
            plane_coverage.total,
            plane_coverage.localized_descriptions,
            plane_coverage.total);
    CHECK(copter_coverage.total > 0 &&
          copter_coverage.localized_names * 100 >= copter_coverage.total * 99, "Copter localized display coverage at least 99%");
    CHECK(plane_coverage.total > 0 &&
          plane_coverage.localized_names * 100 >= plane_coverage.total * 99, "Plane localized display coverage at least 99%");
    CHECK(copter_coverage.localized_descriptions * 100 >= copter_coverage.total * 99, "Copter localized description coverage at least 99%");
    CHECK(plane_coverage.localized_descriptions * 100 >= plane_coverage.total * 99, "Plane localized description coverage at least 99%");
}

static void test_param_roles() {
    fprintf(stderr, "\n[Group] Semantic parameter roles (S6)\n");

    ParamDescriptors db(descriptor_path());
    CHECK(db.is_loaded(), "role database loaded");

    const auto copter = db.roles("copter");
    CHECK(copter.size() >= 40, "copter role table is populated");
    CHECK(copter.count("rc.map.roll") && copter.at("rc.map.roll") == "RCMAP_ROLL", "copter rc.map.roll");
    CHECK(copter.count("flight_mode.slot") && copter.at("flight_mode.slot") == "FLTMODE{n}", "copter flight_mode.slot template");
    CHECK(copter.count("rc.channel_option") &&
              copter.at("rc.channel_option") == "RC{n}_OPTION", "copter rc.channel_option template");
    CHECK(copter.count("pid.rate.roll.p") && copter.at("pid.rate.roll.p") == "ATC_RAT_RLL_P", "copter pid.rate.roll.p");
    CHECK(copter.count("takeoff.alt_m") && copter.at("takeoff.alt_m") == "PILOT_TKO_ALT_M", "copter takeoff.alt_m");
    CHECK(copter.count("takeoff.alt_cm_legacy") &&
              copter.at("takeoff.alt_cm_legacy") == "PILOT_TKOFF_ALT", "copter legacy takeoff cm name is registered");
    CHECK(copter.count("pos.ne.pos_p") && copter.at("pos.ne.pos_p") == "PSC_NE_POS_P", "copter pos.ne.pos_p");
    CHECK(copter.count("arming.check") && copter.at("arming.check") == "ARMING_CHECK", "copter inherits ardupilot arming.check");
    CHECK(copter.count("fixedwing.rate.roll.p") == 0, "copter does not leak fixed-wing roles");

    const auto plane = db.roles("plane");
    CHECK(plane.size() >= 90, "plane role table is populated");
    CHECK(plane.count("frame.quadplane_enable") &&
              plane.at("frame.quadplane_enable") == "Q_ENABLE", "plane frame.quadplane_enable");
    CHECK(plane.count("fixedwing.rate.roll.p") &&
              plane.at("fixedwing.rate.roll.p") == "RLL_RATE_P", "plane fixedwing.rate.roll.p");
    CHECK(plane.count("pid.rate.roll.p") && plane.at("pid.rate.roll.p") == "Q_A_RAT_RLL_P", "plane QuadPlane hover PID");
    CHECK(plane.count("loiter.radius") && plane.at("loiter.radius") == "WP_LOITER_RAD", "plane loiter.radius");
    CHECK(plane.count("tecs.speed_weight") && plane.at("tecs.speed_weight") == "TECS_SPDWEIGHT", "plane tecs.speed_weight");
    CHECK(plane.count("takeoff.vtol_alt_m") &&
              plane.at("takeoff.vtol_alt_m") == "Q_PILOT_TKO_ALT_M", "plane VTOL takeoff altitude (m)");
    CHECK(plane.count("takeoff.vtol_alt_cm_legacy") &&
              plane.at("takeoff.vtol_alt_cm_legacy") == "Q_PILOT_TKOFF_ALT", "plane VTOL takeoff fallback (cm legacy)");
    CHECK(plane.count("rc.map.roll") && plane.at("rc.map.roll") == "RCMAP_ROLL", "plane inherits ardupilot rc.map.roll");
    CHECK(plane.count("takeoff.alt_m") == 0, "plane does not leak copter-only roles");

    const auto vtol = db.roles("vtol");
    CHECK(vtol.count("fixedwing.rate.roll.p") && vtol.at("fixedwing.rate.roll.p") == "RLL_RATE_P", "vtol inherits plane roles");
    CHECK(vtol.count("pid.angle.roll.p") && vtol.at("pid.angle.roll.p") == "Q_A_ANG_RLL_P", "vtol keeps QuadPlane hover PID");
    CHECK(vtol.size() == plane.size(), "vtol role count matches plane");

    CHECK(db.roles("rover").size() == db.roles("ardupilot").size(), "rover inherits only the ardupilot base roles");

    CHECK(db.roles("does-not-exist").empty(), "unknown profile has no role table");
}

int main() {
    fprintf(stderr, "=== Parameter Descriptor Tests ===\n");

    test_ardupilot_profiles();
    test_chinese_translation_overlay();
    test_param_roles();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
