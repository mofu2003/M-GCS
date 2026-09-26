/**
 * test_waveform_instances.cc - 多 IMU / 多磁力计波形实例测试（无硬件）
 *
 * 覆盖：SCALED_IMU/2/3 分派到各自的波形缓冲、实例清单的识别结果、
 * mg_waveform_poll 的 instance 参数，以及 mg_waveform_clear 的批量清空。
 */
#define MGCORE_EXPORTS
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include "ardupilotmega/mavlink.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

/// 注入一帧 SCALED_IMU/2/3，instance 对应 IMU1/2/3。
static void inject_scaled_imu(MockVehicleTestEnv& env, int instance, int16_t xacc, int16_t xmag) {
    mavlink_message_t msg{};
    if (instance == 0) {
        mavlink_scaled_imu_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = xacc;
        imu.xmag = xmag;
        mavlink_msg_scaled_imu_encode(1, 1, &msg, &imu);
    } else if (instance == 1) {
        mavlink_scaled_imu2_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = xacc;
        imu.xmag = xmag;
        mavlink_msg_scaled_imu2_encode(1, 1, &msg, &imu);
    } else {
        mavlink_scaled_imu3_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = xacc;
        imu.xmag = xmag;
        mavlink_msg_scaled_imu3_encode(1, 1, &msg, &imu);
    }

    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    env.link->inject_incoming(buf, len);
}

/// 取出实例清单里某个类别的 index 列表，参数无效时返回 false。
static bool instance_indices(const char* json, const char* category, std::vector<int>& out) {
    out.clear();
    if (!json) return false;
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, category);
    bool ok = cJSON_IsArray(arr);
    if (ok) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, arr) {
            cJSON* index = cJSON_GetObjectItemCaseSensitive(item, "index");
            if (cJSON_IsNumber(index)) out.push_back(index->valueint);
        }
    }
    cJSON_Delete(root);
    return ok;
}

static bool instance_is_active(const char* json, const char* category, int index) {
    if (!json) return false;
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, category);
    bool found = false;
    if (cJSON_IsArray(arr)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, arr) {
            cJSON* entry_index = cJSON_GetObjectItemCaseSensitive(item, "index");
            cJSON* active = cJSON_GetObjectItemCaseSensitive(item, "active");
            if (cJSON_IsNumber(entry_index) && entry_index->valueint == index &&
                cJSON_IsBool(active)) {
                found = cJSON_IsTrue(active);
                break;
            }
        }
    }
    cJSON_Delete(root);
    return found;
}

/// 取某个实例的采样点，返回写入的点数。
static int poll_instance(MockVehicleTestEnv& env, int instance, float* out, int max_points) {
    return mg_waveform_poll(env.core, out, max_points, max_points, 0, instance);
}

static void test_instance_list_before_any_data() {
    fprintf(stderr, "\n[Group] 实例清单 - 未收到数据\n");
    MockVehicleTestEnv env;

    char* json = mg_waveform_get_instances(env.core);
    std::vector<int> imu;
    std::vector<int> mag;
    CHECK(json != nullptr && json[0] == '{', "instances json is an object");
    CHECK(instance_indices(json, "imu", imu), "imu list readable");
    CHECK(instance_indices(json, "mag", mag), "mag list readable");
    CHECK(imu.size() == 1 && imu[0] == 0, "只有 IMU1 占位");
    CHECK(mag.size() == 1 && mag[0] == 0, "只有磁力计 1 占位");
    CHECK(!instance_is_active(json, "imu", 0), "未收到数据时实例不活跃");
    CHECK(!instance_is_active(json, "mag", 0), "未收到数据时磁力计不活跃");
    mg_free_string(json);
}

static void test_instances_follow_received_messages() {
    fprintf(stderr, "\n[Group] 实例清单 - 按收到的报文识别\n");
    MockVehicleTestEnv env;

    inject_scaled_imu(env, 0, 100, 250);
    inject_scaled_imu(env, 1, 200, 0);
    inject_scaled_imu(env, 2, 300, 0);

    char* json = mg_waveform_get_instances(env.core);
    std::vector<int> imu;
    std::vector<int> mag;
    instance_indices(json, "imu", imu);
    instance_indices(json, "mag", mag);

    CHECK(imu.size() == 3, "三个 IMU 实例都被识别");
    CHECK(imu.size() == 3 && imu[0] == 0 && imu[1] == 1 && imu[2] == 2, "实例号按 0/1/2 排列");
    CHECK(mag.size() == 1 && mag[0] == 0, "只有第一路带磁力计数据");
    CHECK(instance_is_active(json, "imu", 0), "IMU1 活跃");
    CHECK(instance_is_active(json, "imu", 2), "IMU3 活跃");
    CHECK(instance_is_active(json, "mag", 0), "磁力计 1 活跃");
    mg_free_string(json);
}

static void test_poll_returns_per_instance_data() {
    fprintf(stderr, "\n[Group] 取数 - 按实例返回各自的缓冲\n");
    MockVehicleTestEnv env;

    inject_scaled_imu(env, 0, 100, 250);
    inject_scaled_imu(env, 1, 200, 0);

    float points[10 * 4] = {};
    const int first = poll_instance(env, 0, points, 4);
    CHECK(first == 1, "IMU1 有 1 个点");
    CHECK(std::fabs(points[1] - 100.0f * 9.80665f / 1000.0f) < 1e-3f, "IMU1 加速度换算正确");
    CHECK(std::fabs(points[7] - 0.25f) < 1e-4f, "IMU1 磁力计换算正确");

    float second[10 * 4] = {};
    const int count = poll_instance(env, 1, second, 4);
    CHECK(count == 1, "IMU2 有 1 个点");
    CHECK(std::fabs(second[1] - 200.0f * 9.80665f / 1000.0f) < 1e-3f, "IMU2 拿到的是自己的数据");
    CHECK(std::fabs(second[7]) < 1e-6f, "IMU2 的磁力计是 0");

    float third[10 * 4] = {};
    CHECK(poll_instance(env, 2, third, 4) == 0, "没有 IMU3 数据");
    CHECK(poll_instance(env, 9, third, 4) == 0, "越界实例号返回 0");
    CHECK(mg_waveform_poll(env.core, third, 4, 4, 0, -1) == 0, "负实例号返回 0");
}

static void test_clear_drops_every_instance() {
    fprintf(stderr, "\n[Group] 清空 - 覆盖所有实例\n");
    MockVehicleTestEnv env;

    inject_scaled_imu(env, 0, 100, 250);
    inject_scaled_imu(env, 1, 200, 0);

    char* cleared = mg_waveform_clear(env.core);
    CHECK(cleared != nullptr && strstr(cleared, "true") != nullptr, "clear succeeds");
    mg_free_string(cleared);

    float points[10 * 4] = {};
    CHECK(poll_instance(env, 0, points, 4) == 0, "IMU1 已清空");
    CHECK(poll_instance(env, 1, points, 4) == 0, "IMU2 已清空");

    char* json = mg_waveform_get_instances(env.core);
    std::vector<int> imu;
    instance_indices(json, "imu", imu);
    CHECK(imu.size() == 2, "清空数据不会抹掉已识别的实例");
    mg_free_string(json);
}

int main() {
    fprintf(stderr, "=== 波形实例测试（多 IMU / 多磁力计） ===\n");

    test_instance_list_before_any_data();
    test_instances_follow_received_messages();
    test_poll_returns_per_instance_data();
    test_clear_drops_every_instance();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_pass, g_pass + g_fail);
    return (g_fail > 0) ? 1 : 0;
}
