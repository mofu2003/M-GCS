#define MGCORE_EXPORTS
/**
 * ffi_telemetry.cc - Telemetry history and log persistence FFI
 *
 * 遥测环形缓冲区、飞行日志记录、数据新鲜度查询。
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/settings/flight_log_writer.h"

#include <algorithm>
#include <chrono>
#include <string>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

// ================================================================
// Telemetry History FFI
// ================================================================







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
    PIN_VEHICLE_SESSION(core);

    auto* waveform = get_active_waveform(core, static_cast<size_t>(instance));
    if (!waveform) return 0;

    const size_t capacity = static_cast<size_t>(out_max_points);
    const size_t target = (target_points > 0)
                              ? std::min(capacity, static_cast<size_t>(target_points))
                              : capacity;
    const uint64_t window = window_ms > 0 ? static_cast<uint64_t>(window_ms) : 0;
    return static_cast<int>(waveform->copy_points(out_points, target, window));
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




// ================================================================
// Parameter File I/O FFI
// ================================================================





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
