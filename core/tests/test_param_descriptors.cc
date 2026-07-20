#include "core/config/param_descriptors.h"

#include <cstdio>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { g_passed++; fprintf(stderr, "  [PASS] %s\n", name); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", name); } \
} while (0)

using mg::core::config::ParamDescriptors;

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
    CHECK("vtol Q_ENABLE enum label", db.enum_label("vtol", "Q_ENABLE", 2) == "启用 VTOL AUTO");

    const auto copter_q = db.get("copter", "Q_ENABLE");
    CHECK("copter Q_ENABLE not found", !copter_q.found);

    const auto gps_rate = db.get("copter", "GPS1_RATE_MS");
    CHECK("GPS1_RATE_MS found", gps_rate.found);
    CHECK("GPS1_RATE_MS unit", !gps_rate.unit.empty());
    CHECK("GPS1_RATE_MS range", gps_rate.min_value != gps_rate.max_value);
}

static void test_chinese_translation_overlay() {
    fprintf(stderr, "\n[Group] Chinese parameter translation overlay\n");

    ParamDescriptors db(descriptor_path(), translation_path());
    CHECK("descriptor database with translation loaded", db.is_loaded());

    const auto vtol_enable = db.get("vtol", "Q_ENABLE");
    CHECK("Q_ENABLE Chinese display", vtol_enable.display_name == "启用 QuadPlane");
    CHECK("Q_ENABLE Chinese description",
          vtol_enable.description.find("启用 QuadPlane 功能") != std::string::npos);
    CHECK("Q_ENABLE Chinese option label", db.enum_label("vtol", "Q_ENABLE", 2) == "启用 VTOL AUTO");

    const auto batt = db.get_group("copter", "BATT");
    CHECK("BATT group Chinese display", batt.display_name == "电池");

    const auto monitor = db.get("copter", "BATT_MONITOR");
    CHECK("BATT_MONITOR Chinese display", monitor.display_name == "电池监测器");
    CHECK("BATT_MONITOR keeps enum options", monitor.options.size() >= 10);

    const auto serial1 = db.get("copter", "SERIAL1_PROTOCOL");
    CHECK("SERIAL1_PROTOCOL Chinese display", serial1.display_name == "串口协议");
    CHECK("SERIAL1_PROTOCOL Chinese option label", db.enum_label("copter", "SERIAL1_PROTOCOL", 2) == "MAVLink 2");
}

int main() {
    fprintf(stderr, "=== Parameter Descriptor Tests ===\n");

    test_ardupilot_profiles();
    test_chinese_translation_overlay();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return g_failed > 0 ? 1 : 0;
}
