/**
 * test_sitl_mission_scenario.cc - 端到端 SITL 任务场景（阶段 2 退出条件）
 *
 * 面向 ArduPilot SITL 的无人值守场景，从"新连接"开始走完一条主路径：
 *   连接 → 固件/机型识别 → 建任务文档 → 上传 → 等待传输结果
 *   → 回读核验 → 断链 → 重连 → 从飞控重建文档 → 再次回读核验
 *
 * 这不是 preflight 的一部分（标签 manual;hardware），因为它需要一个正在运行的
 * 飞控/仿真目标。用法：
 *
 *   MG_SITL_ENDPOINT=127.0.0.1:14550 ./build_check/test_sitl_mission_scenario
 *
 * 未设置 MG_SITL_ENDPOINT 时打印 [SKIP] 并以 0 退出，避免误伤普通测试运行。
 * 输出 `[EVIDENCE] key=value` 行，供 tools/sitl-mission-scenario.sh 落盘成记录。
 */
#define MGCORE_EXPORTS

#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 20
#include "cJSON.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

// 断言宏取自共享头（唯一的 CHECK 实现：core/tests/mgcs_test_check.h），
// 本文件不再自行定义。它在 tests/manual/ 下，用相对路径包含；CMake 里这个目标
// 没有把 core/tests 加进包含目录（那是构建脚本的事，不在本文件范围内）。
#include "../../core/tests/mgcs_test_check.h"

// 沿用本文件原有的计数器名字：EVIDENCE 行与汇总行都读它们。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();

#define EVIDENCE(key, value) \
    fprintf(stdout, "[EVIDENCE] %s=%s\n", (key), (value))

static std::string take(char* p) {
    std::string out = p ? p : "";
    if (p) mg_free_string(p);
    return out;
}

// 解析成功响应，返回 data 子树（调用方 cJSON_Delete root）。
static cJSON* ok_data(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return nullptr;
    cJSON* success = cJSON_GetObjectItemCaseSensitive(root, "success");
    if (!cJSON_IsTrue(success)) { cJSON_Delete(root); return nullptr; }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data) { cJSON_Delete(root); return nullptr; }
    return root;  // caller must delete; use cJSON_GetObjectItem(root,"data")
}

/// 链路是否已建立：车辆列表里存在 connected=true 的车辆。
static bool connection_connected(mg_core_t* core) {
    std::string json = take(mg_vehicle_list(core));
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return false;
    bool connected = false;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "connected"))) {
            connected = true;
            break;
        }
    }
    cJSON_Delete(root);
    return connected;
}

static bool wait_connected(mg_core_t* core, int timeout_ms) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        if (connection_connected(core)) return true;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

static bool wait_disconnected(mg_core_t* core, int timeout_ms) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        if (!connection_connected(core)) return true;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// 返回 {"state":..,"result":..,"result_text":..}; 直到 mission 操作不再是
// uploading/waiting_ack，或超时。
static std::string wait_mission_idle(mg_core_t* core, int timeout_ms) {
    const auto start = std::chrono::steady_clock::now();
    std::string last;
    while (true) {
        std::string snap = take(mg_operations_get_snapshot(core));
        last = snap;
        cJSON* root = cJSON_Parse(snap.c_str());
        if (root) {
            cJSON* mission = cJSON_GetObjectItemCaseSensitive(root, "mission");
            if (mission) {
                cJSON* state = cJSON_GetObjectItemCaseSensitive(mission, "state");
                const char* s = cJSON_IsString(state) ? state->valuestring : "";
                if (std::strcmp(s, "uploading") != 0 &&
                    std::strcmp(s, "waiting_ack") != 0 &&
                    std::strcmp(s, "downloading") != 0) {
                    cJSON_Delete(root);
                    return snap;
                }
            }
            cJSON_Delete(root);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) return last;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

static std::string mission_field(mg_core_t* core, const char* field) {
    std::string snap = take(mg_operations_get_snapshot(core));
    cJSON* root = cJSON_Parse(snap.c_str());
    std::string out;
    if (root) {
        cJSON* mission = cJSON_GetObjectItemCaseSensitive(root, "mission");
        cJSON* item = mission ? cJSON_GetObjectItemCaseSensitive(mission, field) : nullptr;
        if (cJSON_IsString(item)) out = item->valuestring;
        else if (cJSON_IsNumber(item)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(item->valuedouble));
            out = buf;
        }
        cJSON_Delete(root);
    }
    return out;
}

// 在 JSON 数组里按经纬度匹配。1e-4 度约 11 m，足以覆盖飞控端的坐标取整
// 与 JSON 序列化精度损失，同时远小于航点之间的间距（≥70 m）。
static bool array_has_coord(cJSON* array, double lat, double lon) {
    if (!cJSON_IsArray(array)) return false;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, array) {
        cJSON* jlat = cJSON_GetObjectItemCaseSensitive(item, "lat");
        cJSON* jlon = cJSON_GetObjectItemCaseSensitive(item, "lon");
        if (cJSON_IsNumber(jlat) && cJSON_IsNumber(jlon) &&
            std::fabs(jlat->valuedouble - lat) < 1e-4 &&
            std::fabs(jlon->valuedouble - lon) < 1e-4) {
            return true;
        }
    }
    return false;
}

// 统计文档里已启用的航点实体（NAV_WAYPOINT = 16）。
static int count_enabled_waypoints(cJSON* entities) {
    if (!cJSON_IsArray(entities)) return 0;
    int count = 0;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, entities) {
        cJSON* enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
        cJSON* command = cJSON_GetObjectItemCaseSensitive(item, "command");
        if (cJSON_IsTrue(enabled) && cJSON_IsNumber(command) && command->valueint == 16) {
            ++count;
        }
    }
    return count;
}

int main() {
    const char* endpoint_env = getenv("MG_SITL_ENDPOINT");
    if (!endpoint_env || !endpoint_env[0]) {
        fprintf(stderr, "[SKIP] MG_SITL_ENDPOINT not set; SITL scenario not run.\n");
        return 0;
    }
    const std::string endpoint = endpoint_env;
    std::string host_port = endpoint;
    const std::string scheme = "udp://";
    if (host_port.rfind(scheme, 0) == 0) host_port = host_port.substr(scheme.size());
    const std::string udp_uri = scheme + host_port;

    fprintf(stderr, "=== SITL Mission Scenario ===\n");
    fprintf(stderr, "endpoint: udp://%s\n", host_port.c_str());
    EVIDENCE("endpoint", endpoint.c_str());

    mg_core_t* core = mg_create();
    if (!core) { fprintf(stderr, "mg_create failed\n"); return 1; }

    // ---- 1. 新连接 ----
    const int link_id = mg_connection_open(core, udp_uri.c_str());
    CHECK(link_id > 0, "connect request accepted");
    const bool connected = wait_connected(core, 20000);
    CHECK(connected, "link reaches connected state (heartbeat received)");
    if (!connected) {
        fprintf(stderr, "no SITL heartbeat on udp://%s\n", host_port.c_str());
        mg_destroy(core);
        return 1;
    }
    EVIDENCE("connect_state", "connected");

    // ---- 2. 固件/机型识别 ----
    std::string summary = take(mg_vehicle_get_summary(core));
    {
        cJSON* root = cJSON_Parse(summary.c_str());
        cJSON* data = root ? cJSON_GetObjectItemCaseSensitive(root, "data") : nullptr;
        cJSON* sysid = data ? cJSON_GetObjectItemCaseSensitive(data, "system_id") : nullptr;
        cJSON* vtype = data ? cJSON_GetObjectItemCaseSensitive(data, "vehicle_type") : nullptr;
        cJSON* mode = data ? cJSON_GetObjectItemCaseSensitive(data, "flight_mode") : nullptr;
        CHECK(sysid && cJSON_IsNumber(sysid) && sysid->valueint > 0,
              "vehicle system id is known");
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d", sysid && cJSON_IsNumber(sysid) ? sysid->valueint : -1);
        EVIDENCE("system_id", buf);
        std::snprintf(buf, sizeof(buf), "%d", vtype && cJSON_IsNumber(vtype) ? vtype->valueint : -1);
        EVIDENCE("vehicle_type", buf);
        EVIDENCE("flight_mode", cJSON_IsString(mode) ? mode->valuestring : "");
        if (root) cJSON_Delete(root);
    }
    std::string firmware = take(mg_vehicle_get_firmware(core));
    EVIDENCE("firmware", firmware.c_str());
    // MAV_AUTOPILOT_ARDUPILOTMEGA == 3。
    EVIDENCE("firmware_is_ardupilot",
             firmware.find("\"autopilot_type\":3") != std::string::npos ? "true" : "false");

    // ---- 3. 建任务文档 ----
    std::string created = take(mg_mission_doc_create(
        core, R"({"metadata":{"name":"SITL scenario","firmware_profile":"ardupilot"}})"));
    CHECK(created.find("\"success\":true") != std::string::npos, "mission document created");

    const char* waypoints[] = {
        R"({"operation":"add","expected_revision":1,"entity":{"id":"wp1","type":"waypoint","command":16,"lat":-35.3632610,"lon":149.1652300,"alt_m":60,"params":[0,2,0,0]}})",
        R"({"operation":"add","expected_revision":2,"entity":{"id":"wp2","type":"waypoint","command":16,"lat":-35.3628000,"lon":149.1660000,"alt_m":70,"params":[0,2,0,0]}})",
        R"({"operation":"add","expected_revision":3,"entity":{"id":"wp3","type":"waypoint","command":16,"lat":-35.3620000,"lon":149.1670000,"alt_m":80,"params":[0,2,0,0]}})",
    };
    bool doc_ok = true;
    for (const char* wp : waypoints) {
        std::string r = take(mg_mission_doc_apply(core, wp));
        if (r.find("\"success\":true") == std::string::npos) doc_ok = false;
    }
    CHECK(doc_ok, "three waypoints added to the document");
    std::string doc = take(mg_mission_doc_get(core));
    EVIDENCE("doc_revision", "4");
    EVIDENCE("doc_bytes", std::to_string(doc.size()).c_str());

    std::string validate = take(mg_mission_doc_validate(core));
    CHECK(validate.find("\"valid\":true") != std::string::npos ||
          validate.find("\"success\":true") != std::string::npos,
          "mission document validates");

    // ---- 4. 上传 ----
    std::string upload = take(mg_mission_doc_upload(core, 4));
    CHECK(upload.find("\"success\":true") != std::string::npos,
          "upload accepted (transfer started)");
    EVIDENCE("upload_accepted", "true");

    std::string up_snap = wait_mission_idle(core, 20000);
    std::string up_state = mission_field(core, "state");
    std::string up_result = mission_field(core, "result");
    std::string up_result_text = mission_field(core, "result_text");
    std::string up_retry = mission_field(core, "retry_count");
    CHECK(up_result == "0", "flight controller accepted the mission (MISSION_ACCEPTED)");
    EVIDENCE("upload_state", up_state.c_str());
    EVIDENCE("upload_result", up_result.c_str());
    EVIDENCE("upload_result_text", up_result_text.c_str());
    EVIDENCE("upload_retry_count", up_retry.c_str());

    // ---- 5. 回读核验 ----
    std::string download = take(mg_mission_download(core));
    CHECK(download.find("\"success\":true") != std::string::npos, "download started");
    wait_mission_idle(core, 20000);
    std::string verify = take(mg_mission_doc_verify(core, 1.0));
    const bool verified = verify.find("\"matches\":true") != std::string::npos;
    CHECK(verified, "read-back matches the document (1 m tolerance)");
    EVIDENCE("verify_matches", verified ? "true" : "false");
    EVIDENCE("verify_json", verify.c_str());
    std::string plan = take(mg_mission_doc_get(core));
    EVIDENCE("readback_plan_bytes", std::to_string(plan.size()).c_str());

    // ---- 6. 断链 ----
    take(mg_connection_close(core, link_id));
    const bool went_down = wait_disconnected(core, 8000);
    CHECK(went_down, "link reports disconnected after disconnect");
    EVIDENCE("disconnect_state", went_down ? "disconnected" : "still_connected");

    // ---- 7. 重连 + 再次回读核验（恢复）----
    CHECK(mg_connection_open(core, udp_uri.c_str()) > 0, "reconnect request accepted");
    const bool reconnected = wait_connected(core, 20000);
    CHECK(reconnected, "link recovers: connected again after reconnect");
    EVIDENCE("reconnect_state", reconnected ? "connected" : "failed");

    if (reconnected) {
        std::string dl2 = take(mg_mission_download(core));
        CHECK(dl2.find("\"success\":true") != std::string::npos,
              "download after reconnect started");
        wait_mission_idle(core, 20000);
        EVIDENCE("reconnect_download_state", mission_field(core, "state").c_str());
        EVIDENCE("reconnect_download_result", mission_field(core, "result").c_str());
        CHECK(mission_field(core, "result") == "0",
              "flight controller served the mission again after reconnect");

        // 飞控侧持久化证据：回读计划仍含上传的三条航点。
        std::string readback2 = take(mg_mission_doc_get(core));
        EVIDENCE("reconnect_readback_json", readback2.c_str());
        {
            cJSON* root = cJSON_Parse(readback2.c_str());
            cJSON* document = root
                ? cJSON_GetObjectItemCaseSensitive(root, "document") : nullptr;
            cJSON* items = document
                ? cJSON_GetObjectItemCaseSensitive(document, "entities") : nullptr;
            cJSON* count = document
                ? cJSON_GetObjectItemCaseSensitive(document, "entity_count") : nullptr;
            const bool held = cJSON_IsArray(items) &&
                              array_has_coord(items, -35.3632610, 149.1652300) &&
                              array_has_coord(items, -35.3628000, 149.1660000) &&
                              array_has_coord(items, -35.3620000, 149.1670000);
            CHECK(held, "vehicle still holds the uploaded waypoints after the link drop");
            if (cJSON_IsNumber(count)) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%d", count->valueint);
                EVIDENCE("reconnect_readback_count", buf);
            }
            if (root) cJSON_Delete(root);
        }

        // 编辑文档按连接（vehicle_id）持有，断链即释放；重连后的正式恢复流程
        // 是从飞控回读并导入，再核验，而不是沿用上一会话的内存文档。
        std::string imported = take(mg_mission_doc_import_from_vehicle(core));
        CHECK(imported.find("\"success\":true") != std::string::npos,
              "mission document rebuilt from the vehicle after reconnect");
        {
            cJSON* root = cJSON_Parse(imported.c_str());
            cJSON* document = root ? cJSON_GetObjectItemCaseSensitive(root, "document") : nullptr;
            cJSON* revision = root ? cJSON_GetObjectItemCaseSensitive(root, "revision") : nullptr;
            cJSON* entities = document
                ? cJSON_GetObjectItemCaseSensitive(document, "entities") : nullptr;
            const int waypoints = count_enabled_waypoints(entities);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d", waypoints);
            EVIDENCE("reconnect_document_waypoints", buf);
            if (cJSON_IsNumber(revision)) {
                std::snprintf(buf, sizeof(buf), "%d", revision->valueint);
                EVIDENCE("reconnect_document_revision", buf);
            }
            CHECK(waypoints == 3,
                  "rebuilt document carries the three enabled waypoints");
            CHECK(cJSON_IsArray(entities) &&
                      array_has_coord(entities, -35.3632610, 149.1652300) &&
                      array_has_coord(entities, -35.3628000, 149.1660000) &&
                      array_has_coord(entities, -35.3620000, 149.1670000),
                  "rebuilt document keeps the uploaded coordinates");
            if (root) cJSON_Delete(root);
        }

        std::string verify2 = take(mg_mission_doc_verify(core, 1.0));
        const bool verified2 = verify2.find("\"matches\":true") != std::string::npos;
        CHECK(verified2, "recovered document matches the vehicle read-back");
        EVIDENCE("reconnect_verify_matches", verified2 ? "true" : "false");
        EVIDENCE("reconnect_verify_json", verify2.c_str());
    }

    mg_destroy(core);

    EVIDENCE("checks_passed", std::to_string(g_pass).c_str());
    EVIDENCE("checks_failed", std::to_string(g_fail).c_str());
    fprintf(stderr, "\n=== SITL scenario: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
