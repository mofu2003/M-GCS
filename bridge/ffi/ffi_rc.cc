#define MGCORE_EXPORTS
/**
 * ffi_rc.cc - RC Input / Servo Output
 *
 * RC 遥控输入通道和舵机输出 PWM 读取，AP/PX4 通用。
 *
 * Terminology (aligned with QGC/MP):
 *   RC_CHANNELS  msg 65  -> "rc"   (remote control input channels)
 *   SERVO_OUTPUT_RAW msg 36 -> "output" (actuator/servo PWM output)
 *
 * Both AP and PX4 send RC_CHANNELS. SERVO_OUTPUT_RAW is mainly AP;
 * PX4 uses ACTUATOR_OUTPUT (not yet parsed) but also emits SERVO_OUTPUT_RAW in compat mode.
 *
 * All data read from active vehicle's TelemetryCache.
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "telemetry_bindings.h"



// ================================================================
// RC Input (RC_CHANNELS msg 65)
// Unified for both AP and PX4
// ================================================================
MGCORE_API char* mg_rc_get_channels(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"channel_count":0,"rssi":0,"channels":[],"valid":false})");
    auto rc = cache->snapshot_rc();
    return to_cstr(mg::bridge::rc_channels_json(rc.data));
}

// ================================================================
// Servo/Actuator Output (SERVO_OUTPUT_RAW msg 36)
// AP calls these "servo", PX4 calls them "actuator" or "ch"
// We use "output" as the unified term in JSON
// ================================================================
MGCORE_API char* mg_servo_get_outputs(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"servos":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"valid":false})");
    auto sv = cache->snapshot_servo();
    return to_cstr(mg::bridge::servo_output_json(sv.data));
}

// ================================================================
// Get individual RC channel value (1-indexed)
// ================================================================
MGCORE_API char* mg_rc_get_channel(mg_core_t* core, int channel) {
    CHECK_CORE(core);
    if (channel < 1 || channel > 18) return to_cstr(R"({"value":0,"valid":false})");
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"value":0,"valid":false})");
    auto rc_snap = cache->snapshot_rc();
    const auto& rc = rc_snap.data;
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("channel", channel);
    jb.field("value", (int)rc.chan[channel - 1]);
    jb.field("valid", rc.chancount >= channel);
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// Get individual servo output value (1-indexed)
// ================================================================
MGCORE_API char* mg_servo_get_output(mg_core_t* core, int channel) {
    CHECK_CORE(core);
    if (channel < 1 || channel > 16) return to_cstr(R"({"value":0,"valid":false})");
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"value":0,"valid":false})");
    auto sv_snap = cache->snapshot_servo();
    const auto& so = sv_snap.data;
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("channel", channel);
    jb.field("value", (int)so.servo[channel - 1]);
    jb.field("valid", so.servo[channel - 1] > 0);
    jb.end_object();
    return to_cstr(jb.str());
}

