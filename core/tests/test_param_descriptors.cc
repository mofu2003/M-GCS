#include "core/config/param_descriptors.h"

#include <cstdio>
#include <set>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { g_passed++; fprintf(stderr, "  [PASS] %s\n", name); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", name); } \
} while (0)

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
    CHECK("descriptor database loaded", db.is_loaded());

    const auto copter_batt = db.get("copter", "BATT_MONITOR");
    CHECK("copter BATT_MONITOR found", copter_batt.found);
    CHECK("copter BATT_MONITOR display", copter_batt.display_name == "电池监测器");
    CHECK("copter BATT_MONITOR enum", copter_batt.type == "enum");
    CHECK("copter BATT_MONITOR options", copter_batt.options.size() >= 10);

    const auto plane_batt = db.get("plane", "BATT_MONITOR");
    CHECK("plane BATT_MONITOR found", plane_batt.found);
    CHECK("plane BATT_MONITOR reboot", plane_batt.requires_reboot);

    const auto vtol_batt = db.get("vtol", "BATT_MONITOR");
    CHECK("vtol inherits plane BATT_MONITOR", vtol_batt.found);

    const auto vtol_enable = db.get("vtol", "Q_ENABLE");
    CHECK("vtol Q_ENABLE found", vtol_enable.found);
    CHECK("vtol Q_ENABLE display", vtol_enable.display_name == "启用 QuadPlane");
    CHECK("vtol Q_ENABLE enum label",
          db.enum_label("vtol", "Q_ENABLE", 2) == "启用 VTOL AUTO (Enable VTOL AUTO)");

    const auto copter_q = db.get("copter", "Q_ENABLE");
    CHECK("copter Q_ENABLE not found", !copter_q.found);

    const auto gps_rate = db.get("copter", "GPS1_RATE_MS");
    CHECK("GPS1_RATE_MS found", gps_rate.found);
    CHECK("GPS1_RATE_MS unit", !gps_rate.unit.empty());
    CHECK("GPS1_RATE_MS range", gps_rate.min_value != gps_rate.max_value);

    CHECK("read-only descriptor is rejected",
          db.validate("copter", "BARO1_DEVID", 1.0f) == "Parameter is read-only");
    CHECK("range validation still works",
          db.validate("copter", "ACRO_RP_RATE", -1.0f).find("between") != std::string::npos);

    const auto coverage = db.coverage("copter", {"BATT_CAPACITY", "MGC_UNKNOWN"});
    CHECK("coverage counts described parameters",
          coverage.total == 2 && coverage.described == 1);
    CHECK("coverage lists fallback parameters",
          coverage.fallback == 1 && coverage.missing.size() == 1 &&
              coverage.missing.front() == "MGC_UNKNOWN");
}

static void test_chinese_translation_overlay() {
    fprintf(stderr, "\n[Group] Chinese parameter translation overlay\n");

    ParamDescriptors db(descriptor_path(), translation_path());
    CHECK("descriptor database with translation loaded", db.is_loaded());

    const auto vtol_enable = db.get("vtol", "Q_ENABLE");
    CHECK("Q_ENABLE Chinese display", vtol_enable.display_name == "启用 QuadPlane");
    CHECK("Q_ENABLE Chinese description",
          vtol_enable.description.find("启用 QuadPlane 功能") != std::string::npos);
    CHECK("Q_ENABLE bilingual option label",
          db.enum_label("vtol", "Q_ENABLE", 2).find("启用 VTOL AUTO") != std::string::npos &&
          db.enum_label("vtol", "Q_ENABLE", 2).find("Enable VTOL AUTO") != std::string::npos);

    const auto batt = db.get_group("copter", "BATT");
    CHECK("BATT group Chinese display", batt.display_name == "电池");

    const auto monitor = db.get("copter", "BATT_MONITOR");
    CHECK("BATT_MONITOR Chinese display", monitor.display_name == "电池监测器");
    CHECK("BATT_MONITOR keeps enum options", monitor.options.size() >= 10);

    const auto serial1 = db.get("copter", "SERIAL1_PROTOCOL");
    CHECK("SERIAL1_PROTOCOL Chinese display", serial1.display_name == "串口协议");
    CHECK("SERIAL1_PROTOCOL bilingual option label",
          contains_localized_text(db.enum_label("copter", "SERIAL1_PROTOCOL", 2)) &&
          db.enum_label("copter", "SERIAL1_PROTOCOL", 2).find("MAVLink") != std::string::npos);
    CHECK("SERIAL1_PROTOCOL compound option label",
          contains_localized_text(db.enum_label("copter", "SERIAL1_PROTOCOL", 7)));

    const auto copter_mode = db.get("copter", "FLTMODE1");
    CHECK("Copter flight mode option label",
          db.enum_label("copter", "FLTMODE1", 0) == "自稳 (Stabilize)");

    const auto plane_mode = db.get("plane", "FLTMODE1");
    CHECK("Plane flight mode option label",
          db.enum_label("plane", "FLTMODE1", 5) == "电传 A (FBWA)");

    const auto servo = db.get("copter", "SERVO1_FUNCTION");
    CHECK("Servo function option label",
          contains_localized_text(db.enum_label("copter", "SERVO1_FUNCTION", 6)));

    const auto rc_option = db.get("copter", "RC10_OPTION");
    CHECK("RC option label",
          db.enum_label("copter", "RC10_OPTION", 9) == "相机触发 (Camera Trigger)");

    const auto copter_accel = db.get("copter", "ATC_ACC_P_MAX");
    CHECK("Copter fallback Chinese display",
          copter_accel.display_name.find("姿态控制") != std::string::npos &&
          copter_accel.display_name.find("加速度") != std::string::npos);
    CHECK("Copter fallback preserves official description",
          copter_accel.description.find("ArduPilot 官方英文说明") != std::string::npos &&
          copter_accel.description.find("Maximum acceleration in pitch axis") != std::string::npos);
    CHECK("Copter fallback common option label",
          db.enum_label("copter", "ATC_ACC_P_MAX", 0) == "禁用 (Disabled)");

    const auto plane_tecs = db.get("plane", "TECS_SPDWEIGHT");
    CHECK("Plane fallback Chinese display",
          plane_tecs.display_name.find("速度控制权重") != std::string::npos);
    CHECK("Plane fallback Chinese description",
          contains_localized_text(plane_tecs.description));

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
    CHECK("Copter localized display coverage at least 99%",
          copter_coverage.total > 0 &&
          copter_coverage.localized_names * 100 >= copter_coverage.total * 99);
    CHECK("Plane localized display coverage at least 99%",
          plane_coverage.total > 0 &&
          plane_coverage.localized_names * 100 >= plane_coverage.total * 99);
    CHECK("Copter localized description coverage at least 99%",
          copter_coverage.localized_descriptions * 100 >= copter_coverage.total * 99);
    CHECK("Plane localized description coverage at least 99%",
          plane_coverage.localized_descriptions * 100 >= plane_coverage.total * 99);
}

int main() {
    fprintf(stderr, "=== Parameter Descriptor Tests ===\n");

    test_ardupilot_profiles();
    test_chinese_translation_overlay();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
