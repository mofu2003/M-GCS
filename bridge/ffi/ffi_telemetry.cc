#define MGCORE_EXPORTS
/**
 * ffi_telemetry.cc - Telemetry history and log persistence FFI
 *
 * 遥测环形缓冲区、飞行日志记录、参数文件 I/O、数据新鲜度查询。
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/settings/flight_log_writer.h"
#include "core/config/param_file_io.h"

#include <algorithm>
#include <chrono>
#include <string>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

// ================================================================
// Telemetry History FFI
// ================================================================

MGCORE_API char* mg_telemetry_register_series(mg_core_t* core, const char* name, const char* unit) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null name");

    get_active_ring_buffer(core).register_series(name, unit ? unit : "");
    return ok();
}

MGCORE_API char* mg_telemetry_append(mg_core_t* core, const char* name, float value) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null name");

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    get_active_ring_buffer(core).append(name, static_cast<uint64_t>(ms), value);
    return ok();
}

MGCORE_API char* mg_telemetry_get_series(mg_core_t* core, const char* name) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return to_cstr("[]");

    return to_cstr(get_active_ring_buffer(core).to_json(name));
}

MGCORE_API char* mg_telemetry_get_stats(mg_core_t* core, const char* name) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return to_cstr("{}");

    return to_cstr(get_active_ring_buffer(core).stats_json(name));
}

MGCORE_API char* mg_telemetry_get_series_names(mg_core_t* core) {
    CHECK_CORE(core);

    auto names = get_active_ring_buffer(core).get_series_names();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();
    for (const auto& n : names) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("name", n);
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_telemetry_clear(mg_core_t* core) {
    CHECK_CORE(core);
    get_active_ring_buffer(core).clear();
    return ok();
}

// ================================================================
// Real-time Waveform FFI
//
// 波形数据量比普通遥测大得多，走 JSON 的话前端每帧都要解析上千个数。
// 这里直接写调用方提供的 float 缓冲区，零拷贝、零分配。
// ================================================================

namespace {

/// 实例多久没数据就不再认为它存在（毫秒）。
/// 取值比波形页面 1Hz 的取数间隔宽裕，短暂丢包不会让选择器闪掉。
constexpr uint64_t kInstanceActiveWindowMs = 3000;

uint64_t steady_now_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/// 设备 ID 参数名按实例排列。ArduPilot 把实例号插在 _ID 之前
/// （INS_ACC2_ID），磁力计则跟在后面（COMPASS_DEV_ID2），所以直接列出来。
struct InstanceDeviceParams {
    const char* first;
    const char* second;
    const char* third;
};

constexpr InstanceDeviceParams kAccelDeviceParams{"INS_ACC_ID", "INS_ACC2_ID",
                                                  "INS_ACC3_ID"};
constexpr InstanceDeviceParams kGyroDeviceParams{"INS_GYR_ID", "INS_GYR2_ID",
                                                 "INS_GYR3_ID"};
constexpr InstanceDeviceParams kCompassDeviceParams{"COMPASS_DEV_ID", "COMPASS_DEV_ID2",
                                                    "COMPASS_DEV_ID3"};

const char* instance_device_param(const InstanceDeviceParams& params, int index) {
    switch (index) {
        case 1: return params.second;
        case 2: return params.third;
        default: return params.first;
    }
}

/// 用设备 ID 参数解码出型号名，参数没下发或解码失败时返回空串。
std::string instance_sensor_name(mg_core_t* core, bool magnetometer, int index) {
    if (magnetometer) {
        const int device_id = static_cast<int>(
            ffi_get_param(core, instance_device_param(kCompassDeviceParams, index)));
        if (device_id == 0) return "";
        return get_active_sensor_decoder(core)
            .decode_device_id(device_id, "compass")
            .name;
    }
    // 加速度计和陀螺仪通常在同一颗芯片上，加速度计参数缺失时退回陀螺仪型号。
    int device_id = static_cast<int>(
        ffi_get_param(core, instance_device_param(kAccelDeviceParams, index)));
    if (device_id == 0) {
        device_id = static_cast<int>(
            ffi_get_param(core, instance_device_param(kGyroDeviceParams, index)));
    }
    if (device_id == 0) return "";
    return get_active_sensor_decoder(core).decode_device_id(device_id, "ins").name;
}

/// 组装一类实例的 JSON 数组。index 0 始终保留，其余实例只在活跃时出现。
std::string instances_json(mg_core_t* core, mg::core::vehicle::Vehicle* vehicle,
                          bool magnetometer, uint64_t now_ms) {
    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (size_t index = 0; index < mg::core::vehicle::Vehicle::kMaxImuInstances; index++) {
        const uint64_t seen_ms = magnetometer ? vehicle->mag_last_seen_ms(index)
                                              : vehicle->imu_last_seen_ms(index);
        const bool active = seen_ms != 0 && now_ms >= seen_ms &&
                            (now_ms - seen_ms) <= kInstanceActiveWindowMs;
        // 磁力计实例不能只靠最近是否收到波形来判断：有些飞控只在
        // 罗盘校准/低速率遥测时上报 MAG 字段。COMPASS_DEV_ID* 非零
        // 已经足够证明该实例存在，因此即使暂时没有波形也要列出来。
        const bool configured = magnetometer &&
                                ffi_get_param(
                                    core,
                                    instance_device_param(kCompassDeviceParams,
                                                          static_cast<int>(index))) != 0;
        if (!active && !configured && index != 0) continue;

        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("index", static_cast<int>(index));
        item.field("active", active);
        item.field("age_ms", active ? static_cast<long long>(now_ms - seen_ms) : -1LL);
        item.field("name", instance_sensor_name(core, magnetometer, static_cast<int>(index)));
        item.end_object();
        arr.append_json(item.str());
    }
    arr.end_array();
    return arr.str();
}

}  // namespace

MGCORE_API int mg_waveform_poll(mg_core_t* core, float* out_points, int out_max_points,
                                int target_points, int window_ms, int instance) {
    if (!core || !out_points || out_max_points <= 0 || instance < 0) return 0;

    auto* waveform = get_active_waveform(core, static_cast<size_t>(instance));
    if (!waveform) return 0;

    const size_t capacity = static_cast<size_t>(out_max_points);
    const size_t target = (target_points > 0)
                              ? std::min(capacity, static_cast<size_t>(target_points))
                              : capacity;
    const uint64_t window = window_ms > 0 ? static_cast<uint64_t>(window_ms) : 0;
    return static_cast<int>(waveform->copy_points(out_points, target, window));
}

MGCORE_API char* mg_waveform_clear(mg_core_t* core) {
    CHECK_CORE(core);
    for (size_t instance = 0; instance < mg::core::vehicle::Vehicle::kMaxImuInstances; instance++) {
        auto* waveform = get_active_waveform(core, instance);
        if (waveform) waveform->clear();
    }
    return ok();
}

MGCORE_API char* mg_waveform_get_instances(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    auto* vehicle = get_active_vehicle(core);
    if (!vehicle) {
        jb.raw_field("imu", "[]");
        jb.raw_field("mag", "[]");
        jb.end_object();
        return to_cstr(jb.str());
    }

    const uint64_t now_ms = steady_now_ms();
    jb.raw_field("imu", instances_json(core, vehicle, false, now_ms));
    jb.raw_field("mag", instances_json(core, vehicle, true, now_ms));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// Flight Log Persistence FFI
// ================================================================

MGCORE_API char* mg_session_log_start(mg_core_t* core, const char* session_name) {
    CHECK_CORE(core);

    std::string path = core->log_writer.start_session(session_name ? session_name : "");

    if (path.empty()) {
        return fail(MG_ERROR_INTERNAL, "failed to create log file");
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("file", path);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_session_log_stop(mg_core_t* core) {
    CHECK_CORE(core);
    core->log_writer.stop_session();
    return ok();
}

MGCORE_API char* mg_session_log_status(mg_core_t* core) {
    CHECK_CORE(core);

    auto& writer = core->log_writer;

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("active", writer.is_active());
    jb.field("file", writer.current_file_path());
    jb.field("events", static_cast<int64_t>(writer.event_count()));
    jb.field("bytes", static_cast<int64_t>(writer.bytes_written()));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// Parameter File I/O FFI
// ================================================================

MGCORE_API char* mg_param_file_export(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    // 成功路径返回的是 .param 文件正文（CSV 文本），不是 JSON 信封；但失败路径
    // 必须是合法 JSON（F4）：空串会被调用方当成"导出成功但没有参数"，也让
    // "返回值一定是 JSON" 的契约失效。失败一律回统一的 error 对象。
    if (!json || json[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty parameter list");
    }

    cJSON* root = mg::core::json::parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "parameter list must be a JSON array");
    }

    std::vector<mg::core::config::ParamEntry> params;
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (name && cJSON_IsString(name) && value && cJSON_IsNumber(value)) {
            params.push_back({name->valuestring, static_cast<float>(value->valuedouble)});
        }
    }
    cJSON_Delete(root);

    return to_cstr(mg::core::config::ParamFileIO::export_param(params));
}

MGCORE_API char* mg_param_file_import(mg_core_t* core, const char* content) {
    CHECK_CORE(core);
    if (!content || content[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty content");

    auto params = mg::core::config::ParamFileIO::import_param(content);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", !params.empty());
    if (params.empty()) {
        write_error(jb, MG_ERROR_INVALID_PARAM,
                    "no parameters were parsed from the .param content");
    }
    jb.field("count", static_cast<int>(params.size()));
    mg::core::telemetry::JsonBuilder items;
    items.begin_array();
    for (const auto& param : params) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("name", param.name);
        item.field("value", param.value);
        item.field("type", static_cast<int>(param.type));
        item.end_object();
        items.append_json(item.str());
    }
    items.end_array();
    jb.raw_field("items", items.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_param_file_export_csv(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    // 与 mg_param_file_export 相同：成功返回 CSV 正文，失败返回 F4 error 对象。
    if (!json || json[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty parameter list");
    }

    cJSON* root = mg::core::json::parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "parameter list must be a JSON array");
    }

    std::vector<mg::core::config::ParamEntry> params;
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (name && cJSON_IsString(name) && value && cJSON_IsNumber(value)) {
            params.push_back({name->valuestring, static_cast<float>(value->valuedouble)});
        }
    }
    cJSON_Delete(root);

    return to_cstr(mg::core::config::ParamFileIO::export_csv(params));
}

MGCORE_API char* mg_param_file_import_csv(mg_core_t* core, const char* content) {
    CHECK_CORE(core);
    if (!content || content[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty content");

    auto params = mg::core::config::ParamFileIO::import_csv(content);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", !params.empty());
    if (params.empty()) {
        write_error(jb, MG_ERROR_INVALID_PARAM,
                    "no parameters were parsed from the CSV content");
    }
    jb.field("count", static_cast<int>(params.size()));
    mg::core::telemetry::JsonBuilder items;
    items.begin_array();
    for (const auto& param : params) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("name", param.name);
        item.field("value", param.value);
        item.field("type", static_cast<int>(param.type));
        item.end_object();
        items.append_json(item.str());
    }
    items.end_array();
    jb.raw_field("items", items.str());
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// ffi_api.h: mg_telemetry_get_freshness
// ================================================================
MGCORE_API char* mg_telemetry_get_freshness(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"connected":false,"groups":{}})");
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("connected", is_active_connected(core));
    jb.raw_field("groups", cache->freshness_json());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_telemetry_get_messages(mg_core_t* core,
                                            unsigned long long since_seq) {
    CHECK_CORE(core);
    auto& log = get_active_vehicle(core)->message_log();
    const auto entries = log.messages_since(static_cast<uint64_t>(since_seq));

    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (const auto& entry : entries) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("seq", static_cast<unsigned long long>(entry.seq));
        item.field("timestamp_ms",
                   static_cast<unsigned long long>(entry.timestamp_ms));
        item.field("severity", static_cast<int>(entry.severity));
        item.field("severity_name",
                   mg::core::telemetry::statustext_severity_label(entry.severity));
        item.field("text", entry.text);
        item.end_object();
        arr.append_json(item.str());
    }
    arr.end_array();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("total",
             static_cast<unsigned long long>(log.count()));
    jb.raw_field("messages", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}
