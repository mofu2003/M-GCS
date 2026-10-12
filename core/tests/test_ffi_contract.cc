/**
 * test_ffi_contract.cc - FFI bridge contract tests
 *
 * Tests: create/destroy lifecycle, null safety, subsystem routing,
 *        error handling, version string, free_string
 */
#include "ffi_api.h"
#include "ffi_internal.h"
#include "ffi_core_context.h"
#include "core/log/tlog_format.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


// Helper: check JSON contains a key
static bool json_has(const char* json, const char* key) {
    if (!json) return false;
    return strstr(json, key) != nullptr;
}

// Helper: check JSON starts with { or [
static bool json_valid_envelope(const char* json) {
    if (!json) return false;
    return json[0] == '{' || json[0] == '[';
}

struct CallbackDrainProbe {
    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};
};

static void blocking_event_callback(
    mg_event_type_t,
    char* json,
    void* user_data) {
    auto* probe = static_cast<CallbackDrainProbe*>(user_data);
    probe->entered.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    probe->finished.store(true, std::memory_order_release);
    mg_free_string(json);
}

int main() {
    fprintf(stderr, "=== FFI Contract Tests ===\n\n");

    // 1. Lifecycle
    fprintf(stderr, "[Group] Lifecycle\n");
    {
        mg_core_t* core = mg_create();
        CHECK(core != nullptr, "mg_create returns non-null");
        mg_destroy(core);
    }

    // 1b. Event callback lifetime: clearing the callback drains callbacks
    // already copied by a transport thread before the owner can be destroyed.
    fprintf(stderr, "[Group] Event callback lifetime\n");
    {
        EventDispatcher dispatcher;
        CallbackDrainProbe probe;
        dispatcher.set(blocking_event_callback, &probe);
        std::thread event_thread([&dispatcher] {
            dispatcher.fire(MG_EVENT_ATTITUDE, "{}");
        });

        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(500);
        while (!probe.entered.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        const auto drain_start = std::chrono::steady_clock::now();
        dispatcher.set(nullptr, nullptr);
        const auto drain_elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(std::chrono::steady_clock::now() - drain_start);
        event_thread.join();

        CHECK(probe.entered.load(std::memory_order_acquire),
              "event callback entered on the worker thread");
        CHECK(probe.finished.load(std::memory_order_acquire),
              "event callback finished before callback clear returned");
        CHECK(drain_elapsed.count() >= 50,
              "callback clear waits for in-flight callback completion");
    }

    // 2. Null safety - all functions should not crash with null core
    fprintf(stderr, "[Group] Null safety\n");
    {
        const char* r;
        r = mg_connection_get_session_state(nullptr, 0);
        CHECK(r != nullptr, "get_session_state null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_list(nullptr);
        CHECK(r != nullptr, "vehicle_list null core returns non-null");
        mg_free_string((char*)r);

        r = mg_connection_list(nullptr);
        CHECK(r != nullptr, "connection_list null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_get_summary(nullptr);
        CHECK(r != nullptr, "vehicle_get_summary null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_get_global_position(nullptr);
        CHECK(r != nullptr, "vehicle_get_global_position null core returns non-null");
        mg_free_string((char*)r);

        // disconnect should not crash
        r = mg_connection_close(nullptr, 0);
        CHECK(r == nullptr || json_has(r, "error"), "connection_close null core returns null/error");
        if (r) mg_free_string((char*)r);
    }

    // 4. Error system
    fprintf(stderr, "[Group] Error system\n");
    {
        mg_clear_last_error();
        CHECK(mg_get_last_error() == MG_ERROR_NONE, "clear_last_error resets to NONE");

        // Trigger an error
        char* r = mg_command_arm(nullptr, 0);
        CHECK(json_has(r, "error"), "null core command returns error JSON");
        CHECK(mg_get_last_error() != MG_ERROR_NONE, "error code set after failed connect");
        mg_free_string(r);
    }

    // 5. Disconnected state
    fprintf(stderr, "[Group] Disconnected state\n");
    {
        mg_core_t* core = mg_create();
        char* session = mg_connection_get_session_state(core, 1);
        CHECK(json_valid_envelope(session),
              "get_session_state returns valid JSON envelope");
        mg_free_string(session);

        char* summary = mg_vehicle_get_summary(core);
        CHECK(json_valid_envelope(summary), "response summary returns valid JSON");
        mg_free_string(summary);

        mg_destroy(core);
    }

    // 6. Multi-vehicle API
    fprintf(stderr, "[Group] Multi-vehicle API\n");
    {
        mg_core_t* core = mg_create();

        // List should be empty
        char* list = mg_vehicle_list(core);
        CHECK(json_has(list, "[]") || json_has(list, "count"), "vehicle list is empty array or has count");
        mg_free_string(list);

        // Active should be -1 before any selection
        int active = mg_vehicle_get_active(core);
        CHECK(active == -1, "no active vehicle initially (-1)");

        // Select non-existent vehicle
        char* sel = mg_vehicle_select(core, 999);
        CHECK(sel != nullptr, "select returns non-null response");
        CHECK(json_has(sel, "\"success\":false"), "select non-existent vehicle returns failure");
        CHECK(mg_vehicle_get_active(core) == -1, "select non-existent vehicle leaves active unchanged");
        mg_free_string(sel);

        mg_destroy(core);
    }

    // 6b. Session-oriented connection API validation and focus independence.
    fprintf(stderr, "[Group] Session connection API\n");
    {
        mg_core_t* core = mg_create();
        CHECK(mg_connection_open(nullptr, "serial:///dev/null") == MG_ERROR_NULL_CORE,
              "connection_open rejects null core with explicit error");
        CHECK(mg_connection_open(core, nullptr) == MG_ERROR_INVALID_PARAM,
              "connection_open rejects null endpoint URI");
        CHECK(mg_connection_open(core, "unknown://endpoint") == MG_ERROR_INVALID_PARAM,
              "connection_open rejects unsupported endpoint schemes");
        CHECK(mg_connection_open(core, "/dev/tty.test") == MG_ERROR_INVALID_PARAM,
              "canonical connection_open requires an explicit URI scheme");
        CHECK(mg_connection_open(core, "tcp://127.0.0.1:not-a-port") == MG_ERROR_INVALID_PARAM,
              "connection_open rejects malformed TCP port before opening a session");
        CHECK(mg_connection_open(core, "serial:///dev/tty.test?baud=-1") == MG_ERROR_INVALID_PARAM,
              "connection_open rejects malformed serial baud before opening a session");
        CHECK(mg_get_last_error() == MG_ERROR_INVALID_PARAM,
              "connection_open preserves the exact validation error code");
        CHECK(core->vehicles.vehicle_count() == 0,
              "invalid endpoints do not create a connection session");
        CHECK(mg_vehicle_get_active(core) == -1,
              "invalid session open attempts do not change active focus");

        char* state = mg_connection_get_session_state(core, 999);
        CHECK(json_has(state, "\"success\":false"),
              "session_state rejects an unknown session");
        CHECK(json_has(state, "session not found"),
              "unknown session state includes a useful error");
        mg_free_string(state);

        char* close = mg_connection_close(core, 999);
        CHECK(json_has(close, "\"success\":false"),
              "session_close rejects an unknown session");
        CHECK(json_has(close, "session not found"),
              "unknown session close includes a useful error");
        mg_free_string(close);

        char* route_close = mg_connection_close_route(core, 999);
        CHECK(json_has(route_close, "\"success\":false"),
              "route_close rejects an unknown route");
        CHECK(json_has(route_close, "route not found"),
              "unknown route close includes a useful error");
        mg_free_string(route_close);

        char* links = mg_connection_list(core);
        CHECK(json_valid_envelope(links) && strcmp(links, "[]") == 0,
              "connection_list returns an empty array before any links");
        mg_free_string(links);
        CHECK(mg_vehicle_get_active(core) == -1,
              "session state/close operations do not require or change focus");

        char* telemetry = mg_vehicle_get_session_status(core, 999);
        CHECK(json_has(telemetry, "\"success\":false") &&
                  json_has(telemetry, "\"name\":\"NOT_FOUND\""),
              "session telemetry rejects an unknown session");
        mg_free_string(telemetry);

        auto session_vehicle =
            std::make_shared<mg::core::vehicle::Vehicle>(42);
        core->vehicles.add_for_test(session_vehicle);
        state = mg_connection_get_session_state(core, 42);
        CHECK(json_has(state, "\"success\":true") &&
                  json_has(state, "\"session_id\":42"),
              "session_state reports the requested session");
        CHECK(json_has(state, "\"active_focus\":false"),
              "session_state does not require active focus");
        mg_free_string(state);

        auto& session_cache = session_vehicle->cache();
        {
            mg::core::platform::LockGuard lk(session_cache.mtx);
            session_cache.gps.data.fix_type = 3;
            session_cache.gps.data.satellites_visible = 14;
            session_cache.vfr_hud.data.heading = 271;
            session_cache.battery.data.battery_remaining = 82;
            session_cache.battery.data.voltage_v = 15.7f;
            session_cache.freshness.mark_updated(mg::core::telemetry::TelemetryType::kGps);
            session_cache.freshness.mark_updated(mg::core::telemetry::TelemetryType::kVfrHud);
            session_cache.freshness.mark_updated(mg::core::telemetry::TelemetryType::kBattery);
        }
        telemetry = mg_vehicle_get_session_status(core, 42);
        CHECK(json_has(telemetry, "\"success\":true") &&
                  json_has(telemetry, "\"session_id\":42"),
              "session telemetry identifies the requested connection");
        CHECK(json_has(telemetry, "\"gps_satellites\":14"),
              "session telemetry returns fresh GPS satellite count");
        CHECK(json_has(telemetry, "\"heading_deg\":271"),
              "session telemetry returns fresh heading");
        CHECK(json_has(telemetry, "\"battery_remaining_pct\":82") &&
                  json_has(telemetry, "\"battery_voltage_v\":15.7"),
              "session telemetry returns fresh battery values");
        CHECK(mg_vehicle_get_active(core) == -1,
              "session telemetry does not select or require active focus");
        mg_free_string(telemetry);

        close = mg_connection_close(core, 42);
        CHECK(json_has(close, "\"success\":true"),
              "session_close closes the requested session");
        mg_free_string(close);
        CHECK(mg_vehicle_get_active(core) == -1,
              "closing an unfocused session leaves active focus unchanged");
        mg_destroy(core);
    }

    // 7. Free string safety
    fprintf(stderr, "[Group] Free string safety\n");
    {
        mg_free_string(nullptr); // should not crash

        char* s = mg_vehicle_list(nullptr);
        const std::string first_result = s ? s : "";
        CHECK(!first_result.empty(), "allocation returns a usable value");
        mg_free_string(s);
        s = mg_vehicle_list(nullptr);
        CHECK(s && first_result == s,
              "freeing a result does not alter subsequent allocations");
        mg_free_string(s);
    }

    // 8. Settings APIs are safe while disconnected
    fprintf(stderr, "[Group] Settings API disconnected safety\n");
    {
        mg_core_t* core = mg_create();

        char* rc = mg_rc_get_channels(core);
        CHECK(json_valid_envelope(rc), "rc channels returns JSON");
        mg_free_string(rc);

        char* servo = mg_servo_get_outputs(core);
        CHECK(json_valid_envelope(servo), "servo outputs returns JSON");
        mg_free_string(servo);

        mg_destroy(core);
    }

    // 9b. Semantic parameter roles (S6) resolve per profile and degrade safely.
    fprintf(stderr, "[Group] Semantic parameter roles FFI\n");
    {
        char* null_roles = mg_config_get_param_roles(nullptr);
        CHECK(json_valid_envelope(null_roles), "param roles null core returns JSON");
        mg_free_string((char*)null_roles);

        mg_core_t* core = mg_create();
        char* roles = mg_config_get_param_roles(core);
        CHECK(json_valid_envelope(roles), "param roles returns JSON envelope");
        CHECK(json_has(roles, "profile"), "param roles has profile field");
        CHECK(json_has(roles, "roles"), "param roles has roles field");
        CHECK(json_has(roles, "\"profile\":\"\""),
              "disconnected param roles report an empty profile");
        mg_free_string(roles);

        char* resumed = mg_config_get_param_roles(core);
        CHECK(json_has(resumed, "\"roles\":{}"),
              "disconnected param roles is an empty table, not an error");
        CHECK(!json_has(resumed, "\"error\""),
              "disconnected param roles is not an error envelope");
        mg_free_string(resumed);
        mg_destroy(core);
    }

    // 10. Industrial hardening APIs are safe while disconnected
    fprintf(stderr, "[Group] Backend hardening API disconnected safety\n");
    {
        mg_core_t* core = mg_create();

        char* conn_diag = mg_connection_get_diagnostics(core);
        CHECK(json_valid_envelope(conn_diag), "connection diagnostics returns JSON");
        CHECK(json_has(conn_diag, "state"), "connection diagnostics has state");
        CHECK(json_has(conn_diag, "heartbeat"), "connection diagnostics has heartbeat");
        CHECK(json_has(conn_diag, "link"), "connection diagnostics has link");
        CHECK(json_has(conn_diag, "quality_pct"), "connection diagnostics has link quality");
        mg_free_string(conn_diag);

        char* ops = mg_operations_get_snapshot(core);
        CHECK(json_valid_envelope(ops), "operations snapshot returns JSON");
        CHECK(json_has(ops, "operations"), "operations snapshot has operations");
        CHECK(json_has(ops, "params"), "operations snapshot has params operation");
        CHECK(json_has(ops, "mission"), "operations snapshot has mission operation");
        CHECK(json_has(ops, "calibration"), "operations snapshot has calibration operation");
        CHECK(json_has(ops, "log"), "operations snapshot has log operation");
        mg_free_string(ops);

        char* cancel = mg_operations_cancel(core, "params");
        CHECK(json_valid_envelope(cancel), "operations cancel returns JSON");
        CHECK(json_has(cancel, "success"), "operations cancel has success envelope");
        CHECK(json_has(cancel, "canceled"), "operations cancel reports canceled");
        mg_free_string(cancel);

        char* dashboard = mg_telemetry_get_dashboard_snapshot(core);
        CHECK(json_valid_envelope(dashboard), "dashboard snapshot returns JSON");
        CHECK(json_has(dashboard, "connection"), "dashboard snapshot has connection");
        CHECK(json_has(dashboard, "telemetry"), "dashboard snapshot has telemetry");
        CHECK(json_has(dashboard, "freshness"), "dashboard snapshot has freshness");
        mg_free_string(dashboard);

        char* system = mg_system_get_diagnostics(core);
        CHECK(json_valid_envelope(system), "system diagnostics returns JSON");
        CHECK(json_has(system, "connection"), "system diagnostics has connection");
        CHECK(json_has(system, "operations"), "system diagnostics has operations");
        CHECK(json_has(system, "last_error"), "system diagnostics has last_error");
        mg_free_string(system);

        mg_destroy(core);
    }

    // 11. RC input validity follows cache freshness
    fprintf(stderr, "[Group] RC input freshness\n");
    {
        mg_core_t* core = mg_create();
        auto& cache = core->fallback_vehicle->cache();
        {
            mg::core::platform::LockGuard lk(cache.mtx);
            cache.rc_channels.data.chancount = 8;
            cache.rc_channels.data.chan[0] = 1500;
            cache.freshness.mark_updated(mg::core::telemetry::TelemetryType::kRcChannels);
        }

        char* channels = mg_rc_get_channels(core);
        CHECK(json_has(channels, "\"valid\":true"),
              "fresh RC channel snapshot is valid");
        // 有数据时键名必须是 channel_count（与空缓存兜底、契约测试一致）。
        // 历史缺陷：成功路径写 chancount，同一函数在两种情况下键名不同。
        CHECK(json_has(channels, "\"channel_count\":8"),
              "fresh RC snapshot reports channel_count (not chancount)");
        mg_free_string(channels);

        // 等 RC 快照过期（ffi_rc.cc 的新鲜度窗口是 500 ms）。这里轮询到条件成立，
        // 不用固定 sleep：固定等待在慢机器上可能不够，在快机器上纯属浪费
        // （testing.md 的等待条件建议）。
        bool rc_went_stale = false;
        const auto stale_deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < stale_deadline) {
            char* probe = mg_rc_get_channels(core);
            rc_went_stale = json_has(probe, "\"valid\":false");
            mg_free_string(probe);
            if (rc_went_stale) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        CHECK(rc_went_stale,
              "RC snapshot leaves the validity window within the polling deadline");

        channels = mg_rc_get_channels(core);
        CHECK(json_has(channels, "\"valid\":false"),
              "stale RC channel snapshot is invalid");
        mg_free_string(channels);

        mg_destroy(core);
    }

    // 12. tlog replay FFI: local file, no link required (S2)
    fprintf(stderr, "[Group] tlog replay FFI\n");
    {
        const char* tlog_path = "test_ffi_tlog_replay.tlog";

        // 用录制写入器造一个真实 .tlog：两条 HEARTBEAT + 一条 VFR_HUD。
        {
            mg::core::settings::MavlinkLogWriter writer;
            CHECK(writer.start_recording(tlog_path), "tlog writer starts on a temp file");
            uint8_t frame[MAVLINK_MAX_PACKET_LEN];
            for (int i = 0; i < 2; ++i) {
                mavlink_message_t heartbeat{};
                mavlink_msg_heartbeat_pack(1, 1, &heartbeat, MAV_TYPE_QUADROTOR,
                                           MAV_AUTOPILOT_ARDUPILOTMEGA,
                                           MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0,
                                           MAV_STATE_ACTIVE);
                const uint16_t len = mavlink_msg_to_send_buffer(frame, &heartbeat);
                writer.write_frame(frame, len);
            }
            mavlink_message_t hud{};
            mavlink_msg_vfr_hud_pack(1, 1, &hud, 15.0f, 14.0f, 90, 50, 120.0f, 0.5f);
            const uint16_t hud_len = mavlink_msg_to_send_buffer(frame, &hud);
            writer.write_frame(frame, hud_len);
            writer.stop_recording();
        }

        mg_core_t* core = mg_create();

        // 未打开会话时 step/seek 必须是确定的 INVALID_STATE，而不是空结果。
        char* step = mg_tlog_replay_step(core, 10);
        CHECK(json_has(step, "\"success\":false") && json_has(step, "INVALID_STATE"),
              "step without an open session reports INVALID_STATE");
        mg_free_string(step);

        char* seek = mg_tlog_replay_seek(core, 0.5);
        CHECK(json_has(seek, "\"success\":false") && json_has(seek, "INVALID_STATE"),
              "seek without an open session reports INVALID_STATE");
        mg_free_string(seek);

        // 参数与文件错误。
        char* null_path = mg_tlog_replay_open(core, nullptr);
        CHECK(json_has(null_path, "\"success\":false") && json_has(null_path, "INVALID_PARAM"),
              "open with a null path reports INVALID_PARAM");
        mg_free_string(null_path);

        char* missing = mg_tlog_replay_open(core, "test_ffi_tlog_does_not_exist.tlog");
        CHECK(json_has(missing, "\"success\":false") && json_has(missing, "NOT_FOUND"),
              "open on a missing file reports NOT_FOUND");
        mg_free_string(missing);

        // 正常打开：文件统计来自 open 的扫描。
        char* opened = mg_tlog_replay_open(core, tlog_path);
        CHECK(json_has(opened, "\"success\":true"), "open a recorded tlog succeeds");
        CHECK(json_has(opened, "\"messages\":3"), "open reports the scanned message count");
        CHECK(json_has(opened, "\"truncated\":false"), "a clean recording is not truncated");
        CHECK(json_has(opened, tlog_path), "open echoes the file path");
        mg_free_string(opened);

        // 批量读取：先读两条，再读剩余并到达 eof。
        char* batch = mg_tlog_replay_step(core, 2);
        CHECK(json_has(batch, "\"success\":true"), "step returns data");
        CHECK(json_has(batch, "\"count\":2"), "step honours the batch size");
        CHECK(json_has(batch, "\"name\":\"HEARTBEAT\""), "step resolves the MAVLink message name");
        CHECK(json_has(batch, "\"msgid\":0"), "step reports the raw message id");
        CHECK(json_has(batch, "\"eof\":false"), "the file is not exhausted after two of three messages");
        mg_free_string(batch);

        char* tail = mg_tlog_replay_step(core, 50);
        CHECK(json_has(tail, "\"count\":1"), "step reads the remaining message");
        CHECK(json_has(tail, "\"name\":\"VFR_HUD\""), "the remaining message keeps its name");
        CHECK(json_has(tail, "\"eof\":true"), "step reports eof at the end of the file");
        CHECK(json_has(tail, "\"truncated\":false"), "normal eof is not reported as truncated");
        mg_free_string(tail);

        // 跳转：回到开头仍能读到第一条消息；越界百分比是参数错误。
        char* rewind_seek = mg_tlog_replay_seek(core, 0.0);
        CHECK(json_has(rewind_seek, "\"success\":true"), "seek to the start succeeds");
        mg_free_string(rewind_seek);

        char* after_seek = mg_tlog_replay_step(core, 1);
        CHECK(json_has(after_seek, "\"name\":\"HEARTBEAT\""),
              "after seeking to 0 the next message is the first record");
        mg_free_string(after_seek);

        // 落点记录本身要被产出：从 0 起继续读完，应重新拿到剩下两条且顺序不变，
        // 不能像旧的裸字节扫描那样随机跳过或误判落点。
        char* after_seek_rest = mg_tlog_replay_step(core, 10);
        CHECK(json_has(after_seek_rest, "\"count\":2"),
              "seeking to 0 then stepping yields the remaining records");
        CHECK(json_has(after_seek_rest, "\"name\":\"VFR_HUD\"") &&
                  json_has(after_seek_rest, "\"eof\":true"),
              "the remaining records keep their order and reach eof");
        mg_free_string(after_seek_rest);

        // 文件中部跳转：必须落在一条完整记录上，读出的消息名只能是已知帧。
        char* mid_seek = mg_tlog_replay_seek(core, 0.5);
        CHECK(json_has(mid_seek, "\"success\":true"), "seek to mid-file succeeds");
        mg_free_string(mid_seek);
        char* mid = mg_tlog_replay_step(core, 10);
        CHECK(json_has(mid, "\"name\":\"HEARTBEAT\"") ||
                  json_has(mid, "\"name\":\"VFR_HUD\""),
              "a mid-file seek lands on a real record, not stray bytes");
        CHECK(json_has(mid, "\"count\":1") || json_has(mid, "\"count\":2"),
              "the mid-file landing still yields whole records");
        mg_free_string(mid);

        char* out_of_range = mg_tlog_replay_seek(core, 1.5);
        CHECK(json_has(out_of_range, "\"success\":false") && json_has(out_of_range, "INVALID_PARAM"),
              "seek rejects a percent outside [0, 1]");
        mg_free_string(out_of_range);

        char* past_end = mg_tlog_replay_seek(core, 1.0);
        CHECK(json_has(past_end, "\"success\":false") && json_has(past_end, "NOT_FOUND"),
              "seek past the last record reports NOT_FOUND");
        mg_free_string(past_end);

        char* closed = mg_tlog_replay_close(core);
        CHECK(json_has(closed, "\"success\":true"), "close succeeds");
        mg_free_string(closed);

        char* after_close = mg_tlog_replay_step(core, 1);
        CHECK(json_has(after_close, "\"success\":false") && json_has(after_close, "INVALID_STATE"),
              "step after close reports INVALID_STATE");
        mg_free_string(after_close);

        mg_destroy(core);
        std::remove(tlog_path);
    }

    // 13. 截断文件：open 就能看出不完整，回放读到结尾仍不产出半条消息
    fprintf(stderr, "[Group] tlog replay truncation\n");
    {
        const char* truncated_path = "test_ffi_tlog_truncated.tlog";
        {
            FILE* file = fopen(truncated_path, "wb");
            CHECK(file != nullptr, "create a truncated tlog fixture");
            if (file != nullptr) {
                uint8_t ts[mg::core::log::tlog::kTimestampBytes];
                mg::core::log::tlog::encode_timestamp(1700000000000000ULL, ts);
                fwrite(ts, 1, sizeof(ts), file);
                // 一条只剩半截的 MAVLink 帧（magic + 长度 + 少量负载）。
                const uint8_t partial[] = {0xFD, 0x09, 0x00, 0x00, 0x01};
                fwrite(partial, 1, sizeof(partial), file);
                fclose(file);
            }
        }

        mg_core_t* core = mg_create();
        char* opened = mg_tlog_replay_open(core, truncated_path);
        CHECK(json_has(opened, "\"truncated\":true"),
              "open reports a file with a half-written trailing record");
        mg_free_string(opened);

        char* step = mg_tlog_replay_step(core, 10);
        CHECK(json_has(step, "\"count\":0"), "the partial record yields no message");
        CHECK(json_has(step, "\"eof\":true"), "the partial record ends the file");
        CHECK(json_has(step, "\"truncated\":true"),
              "replay reports truncation instead of a clean eof");
        mg_free_string(step);

        mg_tlog_replay_close(core);
        mg_destroy(core);
        std::remove(truncated_path);
    }

    // 14. Industrial hardening APIs handle null core consistently
    fprintf(stderr, "[Group] Backend hardening API null safety\n");
    {
        char* conn_diag = mg_connection_get_diagnostics(nullptr);
        CHECK(json_valid_envelope(conn_diag), "connection diagnostics null core returns JSON");
        CHECK(json_has(conn_diag, "error"), "connection diagnostics null core reports error");
        mg_free_string(conn_diag);

        char* ops = mg_operations_get_snapshot(nullptr);
        CHECK(json_valid_envelope(ops), "operations snapshot null core returns JSON");
        CHECK(json_has(ops, "error"), "operations snapshot null core reports error");
        mg_free_string(ops);

        char* cancel = mg_operations_cancel(nullptr, "params");
        CHECK(json_valid_envelope(cancel), "operations cancel null core returns JSON");
        CHECK(json_has(cancel, "error"), "operations cancel null core reports error");
        mg_free_string(cancel);

        char* dashboard = mg_telemetry_get_dashboard_snapshot(nullptr);
        CHECK(json_valid_envelope(dashboard), "dashboard snapshot null core returns JSON");
        CHECK(json_has(dashboard, "error"), "dashboard snapshot null core reports error");
        mg_free_string(dashboard);

        char* system = mg_system_get_diagnostics(nullptr);
        CHECK(json_valid_envelope(system), "system diagnostics null core returns JSON");
        CHECK(json_has(system, "error"), "system diagnostics null core reports error");
        mg_free_string(system);
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
