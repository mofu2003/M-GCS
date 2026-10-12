#define MGCORE_EXPORTS
/**
 * ffi_rc.cc - RC Input / Servo Output
 *
 * RC 遥控输入通道和舵机输出 PWM 读取（ArduPilot）。
 *
 * Terminology (aligned with QGC/MP):
 *   RC_CHANNELS  msg 65  -> "rc"   (remote control input channels)
 *   SERVO_OUTPUT_RAW msg 36 -> "output" (actuator/servo PWM output)
 *
 * ArduPilot sends RC_CHANNELS and SERVO_OUTPUT_RAW.
 *
 * All data read from active vehicle's mg::core::telemetry::TelemetryCache.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"



// ================================================================
// RC Input (RC_CHANNELS msg 65)
// ArduPilot RC and servo output
// ================================================================
MGCORE_API char* mg_rc_get_channels(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"channel_count":0,"rssi":0,"channels":[],"valid":false})");
    auto rc = cache->snapshot_rc();
    const bool valid = cache->is_fresh(
        mg::core::telemetry::TelemetryType::kRcChannels, 500) &&
        rc.data.chancount > 0;
    return to_cstr(mg::core::telemetry::rc_channels_json(rc.data, valid));
}

// ================================================================
// Servo/Actuator Output (SERVO_OUTPUT_RAW msg 36)
// ArduPilot calls these "servo" or "ch"
// We use "output" as the unified term in JSON
// ================================================================
MGCORE_API char* mg_servo_get_outputs(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"servos":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"valid":false})");
    auto sv = cache->snapshot_servo();
    const bool valid = cache->is_fresh(
        mg::core::telemetry::TelemetryType::kServoOutput, 500);
    return to_cstr(mg::core::telemetry::servo_output_json(sv.data, valid));
}

// ================================================================
// Get individual RC channel value (1-indexed)
// ================================================================

// ================================================================
// Get individual servo output value (1-indexed)
// ================================================================
