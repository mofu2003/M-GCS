/**
 * ffi_internal.h - Internal shared state for FFI bridge
 *
 * FFI 内部共享状态、辅助函数和活跃飞行器路由逻辑。
 *
 * All subsystems live in Vehicle. FFI modules use the get_active_* helpers to
 * route through the active vehicle's subsystems.
 */

#ifndef MGCORE_FFI_INTERNAL_H
#define MGCORE_FFI_INTERNAL_H

#include "ffi_core_context.h"
#include "ffi_api.h"
#include "ffi_mission_bridge.h"
#include "ffi_vehicle_session.h"
#include "core/telemetry/json_builder.h"
#include "core/telemetry/telemetry_cache.h"
#include "core/sensor/sensor_id_decoder.h"
#include "core/calibration/calibration_service.h"
#include "core/config/param_descriptors.h"
#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"
#include "core/settings/flight_logging_service.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/telemetry/waveform_buffer.h"
#include "core/flight/flight_mode_manager.h"
#include "core/mission/mission_types.h"
#include "core/settings/mavlink_log_writer.h"
#include "ardupilotmega/mavlink.h"

#include <string>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

/* ================================================================
 * Shared Helpers
 * ================================================================ */

/// Symbolic name for an mg_error_t, used as `error.name` in FFI responses.
const char* ffi_error_name(mg_error_t error);

/// Canonical error object shared by every failing FFI response:
/// {"code":<int mg_error_t>,"name":"<SYMBOL>","message":"<text>"}
/// See docs/constraints/ffi-contract.md (F4).
inline std::string ffi_error_object_json(mg_error_t error, const std::string& message) {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("code", static_cast<int>(error));
    jb.field("name", ffi_error_name(error));
    jb.field("message", message);
    jb.end_object();
    return jb.str();
}

/// Appends `"error":{...}` to a response already marked `success:false`.
/// Also records the thread-local last error, so callers see the same code.
inline void write_error(mg::core::telemetry::JsonBuilder& jb, mg_error_t error,
                        const std::string& message) {
    mg_set_last_error(error);
    jb.raw_field("error", ffi_error_object_json(error, message));
}

/// The exact JSON body returned by ok(); lets a caller distinguish a bare
/// success from a fuller payload without re-parsing the response.
inline const char* ok_json() { return R"({"success":true})"; }
inline char* ok() { return to_cstr(ok_json()); }
inline char* fail(mg_error_t error, const char* reason) {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    write_error(jb, error, reason ? reason : "");
    jb.end_object();
    return to_cstr(jb.str());
}

// Every core-bound FFI entry pins one session snapshot. Nested helpers reuse it;
// explicit-target scopes install and later restore their own snapshot.
#define CHECK_CORE(core) \
    do { if (!(core)) return fail(MG_ERROR_NULL_CORE, "null core"); } while(0); \
    mg::bridge::ffi::VehicleSessionScope _ffi_core_session_scope(core)
#define CHECK_MAVLINK(core) \
    mg::bridge::ffi::VehicleSessionScope _ffi_mavlink_session_scope(core); \
    if (!_ffi_mavlink_session_scope.view().mavlink_lease) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected")
#define CHECK_CONNECTED(core) \
    mg::bridge::ffi::VehicleSessionScope _ffi_connected_session_scope(core); \
    if (!_ffi_connected_session_scope.view().connected) return fail(MG_ERROR_NOT_CONNECTED, "not connected")
#define PIN_VEHICLE_SESSION(core) mg::bridge::ffi::VehicleSessionScope _ffi_pinned_session_scope(core)
#define CHECK_CORE_RET(core, ret) \
    do { if (!(core)) return (ret); } while(0); \
    mg::bridge::ffi::VehicleSessionScope _ffi_core_ret_session_scope(core)

// Max statustext length for truncation
#ifndef MG_STATUSTEXT_MAX_LEN
#define MG_STATUSTEXT_MAX_LEN 128
#endif

/// Sends one COMMAND_LONG and waits for the flight-controller ACK (safety.md S2).
/// Returns `ok()` only on MAV_RESULT_ACCEPTED; rejections and timeouts return a
/// response that must be forwarded to the caller as-is. Defined in
/// ffi_flight_commands.cc.
///
/// `mav` is an owning lease held for the entire send and ACK wait, so a
/// concurrent disconnect cannot free the adapter under the caller.
char* ffi_cmd_result_queued(
                            std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mav,
                            uint16_t cmd_id, uint8_t sys, uint8_t comp,
                            float p1, float p2, float p3, float p4,
                            float p5, float p6, float p7,
                            int max_wait_ms = 5000);

// ================================================================
// Active vehicle core helpers
//
// The active vehicle is preferred; fallback_vehicle covers offline and
// single-session test paths so helpers stay uniform.
// ================================================================

/// Get the active Vehicle* (multi-vehicle) or nullptr.
inline mg::core::vehicle::Vehicle* get_active_vehicle(mg_core_t* core) {
    return mg::bridge::ffi::require_vehicle(core);
}

// ================================================================
// Active vehicle helpers — used by ALL ffi_*.cc files
// ================================================================

// Get mavlink for current vehicle (or fallback)
/// 命令任务槽位回收（定义在 ffi_flight_commands.cc）。清掉 future 已就绪、
/// 但客户端从未 poll 的条目（ticket 从小到大），直到队列剩余不超过 keep_at_most，
/// 返回回收条数。提交路径在"队列已满"之前先调用它，保证槽位可回收。
size_t ffi_command_jobs_collect(mg_core_t* core, size_t keep_at_most);

/// Owning adapter accessor for paths that block on the adapter (COMMAND_ACK
/// waits, heartbeat polling). The lease keeps the adapter alive for as long as
/// it is held, so a concurrent disconnect cannot free it under the caller;
/// Every adapter access in an FFI operation holds this lease for the duration
/// of the operation, including synchronous waits and file transfers.
inline std::shared_ptr<mg::core::mavlink::MavlinkAdapter>
shared_active_mavlink(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).mavlink_lease;
}

// Get connection manager for current vehicle (or fallback)
inline mg::core::transport::ConnectionManager& get_active_conn(mg_core_t* core) {
    return get_active_vehicle(core)->conn();
}

// Check if current vehicle is connected
inline bool is_active_connected(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).connected;
}

// Get target system/component from active vehicle's heartbeat
inline uint8_t active_target_sys(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).target_sys;
}
inline uint8_t active_target_comp(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).target_comp;
}

// ================================================================
// Active vehicle subsystem helpers — route through Vehicle
// ================================================================

// Accessors route through active vehicle or fallback vehicle.
inline mg::core::telemetry::TelemetryCache* get_active_cache(mg_core_t* core) {
    return mg::bridge::ffi::session_cache(core);
}

inline mg::core::calibration::CalibrationService& get_active_calibration(mg_core_t* core) {
    return get_active_vehicle(core)->calibration();
}
inline mg::core::flight::FlightModeManager& get_active_flight_modes(mg_core_t* core) {
    return get_active_vehicle(core)->flight_modes();
}

/// Appends heartbeat freshness to a state response (safety.md S5).
/// `heartbeat_fresh == false` means armed/mode come from the last frame and must
/// be shown as unknown, never as the current value.
inline void write_heartbeat_freshness(mg::core::telemetry::JsonBuilder& jb,
                                      mg::core::telemetry::TelemetryCache* cache) {
    const bool fresh =
        cache != nullptr && cache->is_fresh(mg::core::telemetry::TelemetryType::kHeartbeat);
    const uint32_t age =
        cache != nullptr ? cache->age_ms(mg::core::telemetry::TelemetryType::kHeartbeat) : 0u;
    // age_ms() 用 UINT32_MAX 表示“从未收到”；对外统一成 -1，前端据此区分未知与过期。
    const bool known_age = cache != nullptr && age != static_cast<uint32_t>(-1);
    jb.field("heartbeat_fresh", fresh);
    jb.field("heartbeat_age_ms",
             known_age ? static_cast<int64_t>(age) : static_cast<int64_t>(-1));
}
inline mg::core::settings::MavlinkLogWriter& get_active_mavlink_log(mg_core_t* core) {
    return get_active_vehicle(core)->mavlink_log();
}
inline mg::core::settings::FlightLoggingService& get_active_flight_log(mg_core_t* core) {
    return get_active_vehicle(core)->flight_log();
}
inline mg::core::sensor::SensorIdDecoder& get_active_sensor_decoder(mg_core_t* core) {
    return get_active_vehicle(core)->sensor_decoder();
}
inline mg::core::telemetry::WaveformBuffer* get_active_waveform(mg_core_t* core,
                                                                size_t instance) {
    auto* vehicle = get_active_vehicle(core);
    return vehicle ? vehicle->waveform(instance) : nullptr;
}

inline mg::core::mission::MissionPlan ffi_present_mission_plan(mg_core_t* core,
                                                               const mg::core::mission::MissionPlan& raw_plan,
                                                               mg::core::mission::cad::MissionHome* home = nullptr,
                                                               bool* home_present = nullptr) {
    return mg::bridge::ffi::present_mission_plan(core, raw_plan, home, home_present);
}

inline mg::core::mission::MissionPlan ffi_prepare_mission_upload(mg_core_t* core,
                                                                 const mg::core::mission::MissionPlan& client_plan,
                                                                 const mg::core::mission::cad::MissionHome* planned_home = nullptr) {
    return mg::bridge::ffi::prepare_mission_upload(core, client_plan, planned_home);
}


// ================================================================
// Shared parameter helpers for ffi_*.cc
// ================================================================

/// 从插件取「QuadPlane 总开关」的参数名，再读它的当前值。参数名由插件持有
/// （S6 / ADR-0010），本层不得出现固件参数字面量。
///
/// 返回值语义：nullopt 表示「无法判定」——非 ArduPilot、未连接、没有心跳，
/// 或该参数尚未进入参数缓存（连接初期 PARAM_VALUE 还没到）。调用方必须把
/// nullopt 与 `0.0f`（明确关闭）区分对待。
inline std::optional<float> ffi_quadplane_enable(mg_core_t* core) {
    auto mav = shared_active_mavlink(core);
    if (!mav) return std::nullopt;
    auto hb = mav->latest_heartbeat();
    if (!hb.has_value() || hb->autopilot_type != MAV_AUTOPILOT_ARDUPILOTMEGA) return std::nullopt;

    const auto plugin = mg::core::firmware::create_firmware_plugin(hb->autopilot_type == 3
                             ? mg::core::firmware::FirmwareKind::ArduPilot
                             : mg::core::firmware::FirmwareKind::Unknown);
    if (!plugin) return std::nullopt;
    const char* param_id = plugin->quadplane_enable_param_id();
    if (!param_id) return std::nullopt;

    const auto param = mav->get_param_by_name(param_id);
    if (!param.has_value()) return std::nullopt;
    return static_cast<float>(param->value);
}

/// 归一化机型名（"Copter"/"FixedWing"/…）到 FFI 机型类别字符串的映射。
/// 两个查询入口共用，避免同一张映射表出现多份拷贝。
inline std::string ffi_vehicle_class_from_name(const std::string& family) {
    if (family == "Copter") return "copter";
    if (family == "FixedWing") return "plane";
    if (family == "VTOL") return "vtol";
    if (family == "Rover") return "rover";
    if (family == "Submarine") return "sub";
    if (family == "AntennaTracker") return "tracker";
    return "";
}

/// ArduPilot 归一化机型（copter/plane/vtol/rover/sub/tracker）。非 ArduPilot
/// 固件、未连接或机型无法归一化时返回空串。任务命令能力门禁按这个结果判断
/// 飞控是否真的存储某个任务字段。
///
/// 注意：QuadPlane 的判定依赖参数缓存，连接初期可能尚未就绪，因此这里的结果
/// 会随时间变化。调用方不得把它缓存成会话级字段（ADR-0010）。
inline std::string ffi_ardupilot_vehicle_class(mg_core_t* core) {
    auto mav = shared_active_mavlink(core);
    if (!mav) return "";
    auto hb = mav->latest_heartbeat();
    if (!hb.has_value() || hb->autopilot_type != MAV_AUTOPILOT_ARDUPILOTMEGA) return "";

    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(hb->autopilot_type, hb->vehicle_type, ffi_quadplane_enable(core));
    return ffi_vehicle_class_from_name(adapter.normalized_vehicle_name());
}

/// 只按 MAV_TYPE 归一化的机型，不看任何参数。用于必须在连接初期就稳定、
/// 且不得随时间变化的结果（任务命令目录的 supported 判定，ADR-0010 第 4 条）。
/// QuadPlane 在这里表现为 "plane"；VTOL 家族的 MAV_TYPE 表现为 "vtol"。
inline std::string ffi_ardupilot_vehicle_class_from_mav_type(mg_core_t* core) {
    auto mav = shared_active_mavlink(core);
    if (!mav) return "";
    auto hb = mav->latest_heartbeat();
    if (!hb.has_value() || hb->autopilot_type != MAV_AUTOPILOT_ARDUPILOTMEGA) return "";

    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(hb->autopilot_type, hb->vehicle_type);
    return ffi_vehicle_class_from_name(adapter.normalized_vehicle_name());
}

/// 参数描述库 profile：ArduPilot 用归一化机型名，其余为空。
inline std::string ffi_param_profile(mg_core_t* core) {
    return ffi_ardupilot_vehicle_class(core);
}

/// Get a parameter value by name (returns 0 if not found or no connection).
inline float ffi_get_param(mg_core_t* core, const char* name) {
    auto mav = shared_active_mavlink(core);
    if (!mav || !name) return 0;
    auto p = mav->get_param_by_name(name);
    return p.has_value() ? p->value : 0;
}

// Global singleton for parameter descriptor database.
// Loaded once from ParameterDescriptors.json on first access.
inline mg::core::config::ParamDescriptors& get_desc_db() {
    static mg::core::config::ParamDescriptors db;
    return db;
}
#endif /* MGCORE_FFI_INTERNAL_H */
