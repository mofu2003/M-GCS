/**
 * test_param_file_io.cc - Unit tests for ParamFileIO (no hardware)
 *
 * Tests: export/import .param, CSV, JSON formats, merge, filter_modified.
 */
#include "core/config/param_file_io.h"
#include "core/config/parameter_change_set.h"
#include "core/mavlink/mavlink_adapter.h"
#include <iostream>
#include <string>
#include <map>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& passed = mgcs_test::passed();
static int& failed = mgcs_test::failed();



int main() {
    std::cout << "=== ParamFileIO Unit Tests ===" << std::endl;

    using namespace mg::core::config;

    // ---- .param format round-trip ----
    {
        std::cout << "\n[Group] .param format" << std::endl;
        std::vector<ParamEntry> params = {
            {"SERIAL1_PROTOCOL", 2.0f, 9},
            {"SERIAL1_BAUD", 100.0f, 9},
            {"BATT_CAPACITY", 5000.0f, 0},
        };

        std::string exported = ParamFileIO::export_param(params);
        CHECK(!exported.empty(), "export non-empty");
        CHECK(exported.find("SERIAL1_PROTOCOL") != std::string::npos, "export has 3 lines");
        CHECK(exported.find("SERIAL1_BAUD") != std::string::npos, "export has SERIAL1_BAUD");
        CHECK(exported.find("BATT_CAPACITY") != std::string::npos, "export has BATT_CAPACITY");

        auto imported = ParamFileIO::import_param(exported);
        CHECK(imported.size() == 3, "import returns 3 params");
        CHECK(imported[0].name == "BATT_CAPACITY", "imported name[0]");
        CHECK_NEAR(imported[0].value, 5000.0f, 1e-5f, "imported value[0]");
        CHECK(imported[2].name == "SERIAL1_PROTOCOL", "imported name[2]");
        CHECK_NEAR(imported[2].value, 2.0f, 1e-5f, "imported value[2]");
        CHECK(exported.find("BATT_CAPACITY") < exported.find("SERIAL1_BAUD"), "export is sorted");
    }

    // ---- Mission Planner-compatible separators and exclusions ----
    {
        std::cout << "\n[Group] Mission Planner compatibility" << std::endl;
        std::string content =
            "PARAM_SPACE 1.25\n"
            "PARAM_TAB\t2.5\n"
            "PARAM_COMMA,3.75\n"
            "FORMAT_VERSION,120\n"
            "SYS_NUM_RESETS,9\n";

        auto imported = ParamFileIO::import_param(content);
        CHECK(imported.size() == 3, "accepts space comma and tab separators");
        CHECK(imported[0].name == "PARAM_SPACE", "space-separated value");
        CHECK_NEAR(imported[1].value, 2.5f, 1e-5f, "tab-separated value");
        CHECK(imported[2].name == "PARAM_COMMA", "skips volatile Mission Planner parameters");
    }

    // ---- .param import with comments ----
    {
        std::cout << "\n[Group] .param import with comments" << std::endl;
        std::string content =
            "# This is a comment\n"
            "PARAM_A,1.0\n"
            "# Another comment\n"
            "\n"
            "PARAM_B,2.5\n";

        auto imported = ParamFileIO::import_param(content);
        CHECK(imported.size() == 2, "skips comments and blank lines");
        CHECK(imported[0].name == "PARAM_A", "first param");
        CHECK_NEAR(imported[0].value, 1.0f, 1e-5f, "first value");
    }

    // ---- CSV format round-trip ----
    {
        std::cout << "\n[Group] CSV format" << std::endl;
        std::vector<ParamEntry> params = {
            {"ALT_HOLD_RTL", 100.0f, 9},
            {"WP_RADIUS", 30.0f, 9},
        };

        std::string csv = ParamFileIO::export_csv(params);
        CHECK(csv.find("name,value,type") != std::string::npos, "csv has header");
        CHECK(csv.find("ALT_HOLD_RTL") != std::string::npos, "csv has ALT_HOLD_RTL");

        auto imported = ParamFileIO::import_csv(csv);
        CHECK(imported.size() == 2, "import returns 2 params");
        CHECK(imported[0].name == "ALT_HOLD_RTL", "imported name[0]");
        CHECK_NEAR(imported[0].value, 100.0f, 1e-5f, "imported value[0]");
        CHECK(imported[0].type == 9, "imported type");
    }

    // ---- JSON format round-trip ----
    {
        std::cout << "\n[Group] JSON format" << std::endl;
        std::vector<ParamEntry> params = {
            {"THR_MAX", 1000.0f, 9},
            {"THR_MIN", 130.0f, 9},
        };

        std::string json = ParamFileIO::export_json(params);
        CHECK(!json.empty(), "json non-empty");
        CHECK(json.find("THR_MAX") != std::string::npos, "json has THR_MAX");

        auto imported = ParamFileIO::import_json(json);
        CHECK(imported.size() == 2, "import returns 2 params");
        CHECK(imported[0].name == "THR_MAX", "imported name[0]");
        CHECK_NEAR(imported[0].value, 1000.0f, 1e-5f, "imported value[0]");
    }

    // ---- filter_modified ----
    {
        std::cout << "\n[Group] filter_modified" << std::endl;
        std::map<std::string, float> current = {
            {"PARAM_A", 10.0f},
            {"PARAM_B", 20.0f},
            {"PARAM_C", 30.0f},
        };
        std::map<std::string, float> defaults = {
            {"PARAM_A", 10.0f},   // same
            {"PARAM_B", 15.0f},   // different
            // PARAM_C not in defaults = modified
        };

        auto modified = ParamFileIO::filter_modified(current, defaults);
        CHECK(modified.size() == 2, "2 modified params");

        bool has_b = false, has_c = false;
        for (const auto& p : modified) {
            if (p.name == "PARAM_B") has_b = true;
            if (p.name == "PARAM_C") has_c = true;
        }
        CHECK(has_b, "PARAM_B is modified");
        CHECK(has_c, "PARAM_C is modified (new)");
    }

    // ---- merge ----
    {
        std::cout << "\n[Group] merge" << std::endl;
        std::map<std::string, float> existing = {
            {"A", 1.0f},
            {"B", 2.0f},
        };
        std::vector<ParamEntry> imported = {
            {"B", 99.0f, 9},   // override
            {"C", 3.0f, 9},    // new
        };

        auto merged = ParamFileIO::merge(existing, imported);
        CHECK(merged.size() == 3, "merged has 3 entries");
        CHECK_NEAR(merged["A"], 1.0f, 1e-5f, "A unchanged");
        CHECK_NEAR(merged["B"], 99.0f, 1e-5f, "B overridden");
        CHECK_NEAR(merged["C"], 3.0f, 1e-5f, "C added");
    }

    // ---- Import edge cases ----
    {
        std::cout << "\n[Group] Import edge cases" << std::endl;
        auto empty = ParamFileIO::import_param("");
        CHECK(empty.empty(), "empty string returns empty");

        auto empty2 = ParamFileIO::import_param("# only comments\n\n");
        CHECK(empty2.empty(), "only comments returns empty");

        auto bad = ParamFileIO::import_param("BAD_NO_VALUE\n");
        CHECK(bad.empty(), "malformed line is skipped");

        auto partial = ParamFileIO::import_param("GOOD,1.0\nBAD_NO_VALUE\nALSO_GOOD,2.0\n");
        CHECK(partial.size() == 2, "partial parse works");
    }

    // ---- Parameter change-set planning ----
    {
        std::cout << "\n[Group] Parameter change-set" << std::endl;
        std::vector<mg::core::mavlink::ParamInfo> current = {
            {"BATT_ENABLE", 1.0f, 9, 0, 3},
            {"BATT_MONITOR", 0.0f, 9, 1, 3},
            {"SERIAL1_PROTOCOL", 2.0f, 6, 2, 3},
        };
        std::vector<ParamEntry> requested = {
            {"BATT_ENABLE", 0.0f, 0},
            {"BATT_MONITOR", 4.0f, 0},
            {"SERIAL1_PROTOCOL", 2.0f, 0},
            {"UNKNOWN_PARAM", 7.0f, 0},
            {"BATT_MONITOR", 5.0f, 0},
        };

        const auto prepared = ParameterChangeSetPlanner::prepare(requested, current);
        CHECK(prepared.changes.size() == 4, "deduplicates requested entries");
        CHECK(prepared.ready_count == 2, "classifies ready changes");
        CHECK(prepared.unchanged_count == 1, "classifies unchanged entries");
        CHECK(prepared.unknown_count == 1, "classifies unknown entries");

        const auto ordered = ParameterChangeSetPlanner::ordered_ready(prepared);
        CHECK(ordered.size() == 2, "orders two ready changes");
        CHECK(ordered[0].entry.name == "BATT_MONITOR", "writes dependent value before enable");
        CHECK(ordered[1].entry.name == "BATT_ENABLE", "writes enable parameter last");
        CHECK_NEAR(ordered[0].entry.value, 5.0f, 1e-5f, "uses final duplicate value");
        CHECK(ordered[0].entry.type == 9, "uses cached MAVLink type");
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}
