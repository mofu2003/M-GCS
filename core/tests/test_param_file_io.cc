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

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; std::cout << "  PASS: " << name << std::endl; } \
    else { failed++; std::cout << "  FAIL: " << name << std::endl; } \
} while(0)

#define TEST_NEAR(name, actual, expected, eps) do { \
    float _a = (actual), _e = (expected), _d = (eps); \
    if ((_a - _e) > _d || (_e - _a) > _d) { \
        failed++; std::cout << "  FAIL: " << name << " (got " << _a << ", expected ~" << _e << ")" << std::endl; \
    } else { passed++; std::cout << "  PASS: " << name << std::endl; } \
} while(0)

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
        TEST("export non-empty", !exported.empty());
        TEST("export has 3 lines", exported.find("SERIAL1_PROTOCOL") != std::string::npos);
        TEST("export has SERIAL1_BAUD", exported.find("SERIAL1_BAUD") != std::string::npos);
        TEST("export has BATT_CAPACITY", exported.find("BATT_CAPACITY") != std::string::npos);

        auto imported = ParamFileIO::import_param(exported);
        TEST("import returns 3 params", imported.size() == 3);
        TEST("imported name[0]", imported[0].name == "BATT_CAPACITY");
        TEST_NEAR("imported value[0]", imported[0].value, 5000.0f, 1e-5f);
        TEST("imported name[2]", imported[2].name == "SERIAL1_PROTOCOL");
        TEST_NEAR("imported value[2]", imported[2].value, 2.0f, 1e-5f);
        TEST("export is sorted", exported.find("BATT_CAPACITY") < exported.find("SERIAL1_BAUD"));
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
        TEST("accepts space comma and tab separators", imported.size() == 3);
        TEST("space-separated value", imported[0].name == "PARAM_SPACE");
        TEST_NEAR("tab-separated value", imported[1].value, 2.5f, 1e-5f);
        TEST("skips volatile Mission Planner parameters",
             imported[2].name == "PARAM_COMMA");
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
        TEST("skips comments and blank lines", imported.size() == 2);
        TEST("first param", imported[0].name == "PARAM_A");
        TEST_NEAR("first value", imported[0].value, 1.0f, 1e-5f);
    }

    // ---- CSV format round-trip ----
    {
        std::cout << "\n[Group] CSV format" << std::endl;
        std::vector<ParamEntry> params = {
            {"ALT_HOLD_RTL", 100.0f, 9},
            {"WP_RADIUS", 30.0f, 9},
        };

        std::string csv = ParamFileIO::export_csv(params);
        TEST("csv has header", csv.find("name,value,type") != std::string::npos);
        TEST("csv has ALT_HOLD_RTL", csv.find("ALT_HOLD_RTL") != std::string::npos);

        auto imported = ParamFileIO::import_csv(csv);
        TEST("import returns 2 params", imported.size() == 2);
        TEST("imported name[0]", imported[0].name == "ALT_HOLD_RTL");
        TEST_NEAR("imported value[0]", imported[0].value, 100.0f, 1e-5f);
        TEST("imported type", imported[0].type == 9);
    }

    // ---- JSON format round-trip ----
    {
        std::cout << "\n[Group] JSON format" << std::endl;
        std::vector<ParamEntry> params = {
            {"THR_MAX", 1000.0f, 9},
            {"THR_MIN", 130.0f, 9},
        };

        std::string json = ParamFileIO::export_json(params);
        TEST("json non-empty", !json.empty());
        TEST("json has THR_MAX", json.find("THR_MAX") != std::string::npos);

        auto imported = ParamFileIO::import_json(json);
        TEST("import returns 2 params", imported.size() == 2);
        TEST("imported name[0]", imported[0].name == "THR_MAX");
        TEST_NEAR("imported value[0]", imported[0].value, 1000.0f, 1e-5f);
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
        TEST("2 modified params", modified.size() == 2);

        bool has_b = false, has_c = false;
        for (const auto& p : modified) {
            if (p.name == "PARAM_B") has_b = true;
            if (p.name == "PARAM_C") has_c = true;
        }
        TEST("PARAM_B is modified", has_b);
        TEST("PARAM_C is modified (new)", has_c);
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
        TEST("merged has 3 entries", merged.size() == 3);
        TEST_NEAR("A unchanged", merged["A"], 1.0f, 1e-5f);
        TEST_NEAR("B overridden", merged["B"], 99.0f, 1e-5f);
        TEST_NEAR("C added", merged["C"], 3.0f, 1e-5f);
    }

    // ---- Import edge cases ----
    {
        std::cout << "\n[Group] Import edge cases" << std::endl;
        auto empty = ParamFileIO::import_param("");
        TEST("empty string returns empty", empty.empty());

        auto empty2 = ParamFileIO::import_param("# only comments\n\n");
        TEST("only comments returns empty", empty2.empty());

        auto bad = ParamFileIO::import_param("BAD_NO_VALUE\n");
        TEST("malformed line is skipped", bad.empty());

        auto partial = ParamFileIO::import_param("GOOD,1.0\nBAD_NO_VALUE\nALSO_GOOD,2.0\n");
        TEST("partial parse works", partial.size() == 2);
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
        TEST("deduplicates requested entries", prepared.changes.size() == 4);
        TEST("classifies ready changes", prepared.ready_count == 2);
        TEST("classifies unchanged entries", prepared.unchanged_count == 1);
        TEST("classifies unknown entries", prepared.unknown_count == 1);

        const auto ordered = ParameterChangeSetPlanner::ordered_ready(prepared);
        TEST("orders two ready changes", ordered.size() == 2);
        TEST("writes dependent value before enable", ordered[0].entry.name == "BATT_MONITOR");
        TEST("writes enable parameter last", ordered[1].entry.name == "BATT_ENABLE");
        TEST_NEAR("uses final duplicate value", ordered[0].entry.value, 5.0f, 1e-5f);
        TEST("uses cached MAVLink type", ordered[0].entry.type == 9);
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}
