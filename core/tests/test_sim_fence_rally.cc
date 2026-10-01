/**
 * test_sim_fence_rally.cc - End-to-end fence/rally mission upload
 *
 * Exercises MAV_MISSION_TYPE_FENCE (1) and MAV_MISSION_TYPE_RALLY (2) through
 * the mission document FFI against the mock flight controller, which now keeps
 * per-mission-type stores and echoes the mission type in its protocol replies.
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static unsigned long long document_revision(mg_core_t* core) {
    char* json = mg_mission_doc_get(core);
    unsigned long long revision = 0;
    cJSON* root = json ? cJSON_Parse(json) : nullptr;
    if (root) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(root, "revision");
        if (item && cJSON_IsNumber(item)) {
            revision = static_cast<unsigned long long>(item->valuedouble);
        }
        cJSON_Delete(root);
    }
    if (json) mg_free_string(json);
    return revision;
}

static bool wait_for(MockVehicleTestEnv& env,
                     const std::function<bool()>& predicate,
                     int timeout_ms = 4000) {
    env.start_pump();
    const auto start = std::chrono::steady_clock::now();
    bool satisfied = predicate();
    while (!satisfied) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        satisfied = predicate();
    }
    env.stop_pump();
    return satisfied;
}

// 从 mg_mission_doc_* 的返回 envelope 里取 document.geo_fence / rally_points。
static int json_array_size(const char* json, const char* section,
                           const char* array_key) {
    if (!json) return -1;
    cJSON* root = cJSON_Parse(json);
    int size = -1;
    if (root) {
        cJSON* document = cJSON_GetObjectItemCaseSensitive(root, "document");
        cJSON* object = section ? cJSON_GetObjectItemCaseSensitive(document, section)
                                : document;
        cJSON* array = cJSON_GetObjectItemCaseSensitive(object, array_key);
        if (cJSON_IsArray(array)) size = cJSON_GetArraySize(array);
        cJSON_Delete(root);
    }
    return size;
}

int main() {
    fprintf(stderr, "=== Fence / Rally Mission Simulation Test ===\n");

    MockVehicleTestEnv env({}, false);
    CHECK("core created", env.core != nullptr);
    CHECK("mock vehicle connects", env.wait_connected());

    char* result =
        mg_mission_doc_create(env.core, R"({"metadata":{"name":"Fence Rally"}})");
    if (result) mg_free_string(result);

    const char* plan = R"({
        "fileType": "Plan",
        "geoFence": {
            "version": 2,
            "polygons": [{
                "version": 1,
                "inclusion": true,
                "polygon": [[39.9, 116.4], [39.91, 116.41], [39.92, 116.4]]
            }],
            "circles": [{
                "version": 1,
                "inclusion": false,
                "circle": {"center": [39.9, 116.4], "radius": 90}
            }],
            "breachReturn": [39.9, 116.4, 35]
        },
        "rallyPoints": {
            "version": 2,
            "points": [[39.9, 116.4, 30], [39.91, 116.41, 40]]
        },
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "SimpleItem",
                "frame": 3,
                "command": 16,
                "autoContinue": true,
                "params": [0, 2, 0, 0, 39.9, 116.4, 50]
            }]
        }
    })";
    result = mg_mission_doc_import_format(env.core, "plan", plan);
    CHECK("plan with fence and rally imports", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);

    const unsigned long long revision = document_revision(env.core);
    CHECK("document revision is available for upload", revision != 0);

    fprintf(stderr, "\n[Fence upload]\n");
    result = mg_mission_doc_upload_fence(env.core, revision);
    CHECK("fence upload starts", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);

    const bool fence_arrived = wait_for(env, [&]() {
        return env.mock->stored_mission_type(MAV_MISSION_TYPE_FENCE).size() == 5;
    });
    CHECK("fence reaches the vehicle as five items", fence_arrived);
    const auto& fence = env.mock->stored_mission_type(MAV_MISSION_TYPE_FENCE);
    if (fence.size() == 5) {
        CHECK("polygon vertex keeps count and fence mission type",
              fence[0].command == 5001 && fence[0].param1 == 3.0f &&
                  fence[0].mission_type == MAV_MISSION_TYPE_FENCE);
        CHECK("polygon vertices arrive in order",
              fence[2].command == 5001 && fence[2].seq == 2);
        CHECK("exclusion circle keeps its radius",
              fence[3].command == 5004 &&
                  static_cast<int>(fence[3].param1) == 90);
        CHECK("breach-return point keeps its altitude",
              fence[4].command == 5000 &&
                  static_cast<int>(fence[4].z) == 35);
    }

    fprintf(stderr, "\n[Fence download / import]\n");
    result = mg_fence_download(env.core);
    CHECK("fence download starts", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);
    CHECK("fence download completes",
          wait_for(env, [&]() {
              return env.mav->fence_get_plan_snapshot()
                         .provenance.last_completed_download_revision != 0;
          }));
    result = mg_mission_doc_import_fence_from_vehicle(env.core);
    CHECK("fence imports back into the document",
          result && strstr(result, "\"success\":true"));
    CHECK("imported fence keeps its polygon",
          json_array_size(result, "geo_fence", "polygons") == 1);
    CHECK("imported fence keeps its circle",
          json_array_size(result, "geo_fence", "circles") == 1);
    if (result) mg_free_string(result);

    result = mg_mission_doc_verify_fence(env.core, 1e-6);
    CHECK("fence verify matches after import",
          result && strstr(result, "\"matches\":true"));
    if (result) mg_free_string(result);

    fprintf(stderr, "\n[Rally upload]\n");
    const unsigned long long rally_revision = document_revision(env.core);
    result = mg_mission_doc_upload_rally(env.core, rally_revision);
    CHECK("rally upload starts", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);

    const bool rally_arrived = wait_for(env, [&]() {
        return env.mock->stored_mission_type(MAV_MISSION_TYPE_RALLY).size() == 2;
    });
    CHECK("rally points reach the vehicle", rally_arrived);
    const auto& rally = env.mock->stored_mission_type(MAV_MISSION_TYPE_RALLY);
    if (rally.size() == 2) {
        CHECK("rally item uses NAV_RALLY_POINT and rally mission type",
              rally[0].command == 5100 &&
                  rally[0].mission_type == MAV_MISSION_TYPE_RALLY &&
                  rally[0].frame == MAV_FRAME_GLOBAL_RELATIVE_ALT);
        CHECK("rally altitude is preserved",
              static_cast<int>(rally[1].z) == 40);
    }

    fprintf(stderr, "\n[Rally download / import]\n");
    result = mg_rally_download(env.core);
    CHECK("rally download starts", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);
    CHECK("rally download completes",
          wait_for(env, [&]() {
              return env.mav->rally_get_plan_snapshot()
                         .provenance.last_completed_download_revision != 0;
          }));
    result = mg_mission_doc_import_rally_from_vehicle(env.core);
    CHECK("rally points import back into the document",
          result && strstr(result, "\"success\":true"));
    CHECK("imported document keeps both rally points",
          json_array_size(result, nullptr, "rally_points") == 2);
    if (result) mg_free_string(result);

    result = mg_mission_doc_verify_rally(env.core, 1e-6);
    CHECK("rally verify matches after import",
          result && strstr(result, "\"matches\":true"));
    if (result) mg_free_string(result);

    fprintf(stderr, "\n[Fence clear]\n");
    result = mg_mission_doc_clear_fence(env.core);
    CHECK("fence clear starts", result && strstr(result, "\"success\":true"));
    if (result) mg_free_string(result);
    CHECK("fence cleared on the vehicle",
          wait_for(env, [&]() {
              return env.mock->stored_mission_type(MAV_MISSION_TYPE_FENCE).empty();
          }));

    fprintf(stderr, "\n[Fence verify after clear]\n");
    result = mg_fence_download(env.core);
    if (result) mg_free_string(result);
    CHECK("cleared fence downloads again",
          wait_for(env, [&]() {
              const auto snapshot = env.mav->fence_get_plan_snapshot();
              return snapshot.provenance.completed_download_count >= 2;
          }));
    result = mg_mission_doc_verify_fence(env.core, 1e-6);
    CHECK("verify reports a mismatch once the vehicle fence is cleared",
          result && strstr(result, "\"matches\":false") &&
              strstr(result, "polygon_count") && strstr(result, "breach_return"));
    if (result) mg_free_string(result);

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
