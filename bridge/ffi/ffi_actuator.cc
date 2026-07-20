#define MGCORE_EXPORTS
/**
 * ffi_actuator.cc - Actuator / Motor / Servo channel configuration
 *
 * 舵机/电机通道配置：功能映射、限位参数、电机测试。
 *
 * AP uses SERVO{n}_FUNCTION/MIN/MAX/TRIM for channel mapping.
 * PX4 uses ACTUATOR{n}_FUNCTION (will be added when PX4 actuator config is needed).
 *
 * AP SERVO_FUNCTION values (from AP_Motors):
 *   0=Disabled, 33=Throttle, 74=Motor1, 75=Motor2, ...
 *   34=Aileron, 35=Elevator, 36=Rudder, ...
 */
#include "ffi_internal.h"
#include "json_serializers.h"

#define MAX_SERVO_CHANNELS 16

static const char* servo_func_param_fmt = "SERVO%d_FUNCTION";
static const char* servo_min_param_fmt  = "SERVO%d_MIN";
static const char* servo_max_param_fmt  = "SERVO%d_MAX";
static const char* servo_trim_param_fmt = "SERVO%d_TRIM";
static const char* servo_rev_param_fmt  = "SERVO%d_REVERSED";







MGCORE_API char* mg_actuator_get_mapping(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("[]");

    mg::bridge::JsonBuilder jb;
    jb.begin_array();

    for (int ch = 1; ch <= MAX_SERVO_CHANNELS; ch++) {
        float func = ffi_get_param(core, ffi_fmt_param(servo_func_param_fmt, ch).c_str());
        float min_val = ffi_get_param(core, ffi_fmt_param(servo_min_param_fmt, ch).c_str());
        float max_val = ffi_get_param(core, ffi_fmt_param(servo_max_param_fmt, ch).c_str());
        float trim = ffi_get_param(core, ffi_fmt_param(servo_trim_param_fmt, ch).c_str());
        float rev = ffi_get_param(core, ffi_fmt_param(servo_rev_param_fmt, ch).c_str());

        // Skip channels with no config (function=0 and no min/max)
        if (func == 0 && min_val == 0 && max_val == 0) continue;

        mg::bridge::JsonBuilder obj;
        obj.begin_object();
        obj.field("channel", ch);
        obj.field("function_id", (int)func);
        obj.field("min", (int)min_val);
        obj.field("max", (int)max_val);
        obj.field("trim", (int)trim);
        obj.field("reversed", rev != 0);
        obj.end_object();
        jb.append_json(obj.str());
    }

    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_actuator_set_function(mg_core_t* core, int channel, int function_id) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (channel < 1 || channel > MAX_SERVO_CHANNELS)
        return fail(MG_ERROR_INVALID_PARAM, "channel must be 1-16");

    std::string param = ffi_fmt_param(servo_func_param_fmt, channel);
    auto* mav = get_active_mavlink(core);
    auto p = mav->get_param_by_name(param.c_str());
    if (!p.has_value()) return fail(MG_ERROR_NOT_FOUND, "channel param not found");

    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       param.c_str(), (float)function_id, p->type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "set function failed");
}

MGCORE_API char* mg_actuator_get_limits(mg_core_t* core, int channel) {
    CHECK_CORE(core);
    if (channel < 1 || channel > MAX_SERVO_CHANNELS) return to_cstr("{}");

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("channel", channel);
    jb.field("min", (int)ffi_get_param(core, ffi_fmt_param(servo_min_param_fmt, channel).c_str()));
    jb.field("max", (int)ffi_get_param(core, ffi_fmt_param(servo_max_param_fmt, channel).c_str()));
    jb.field("trim", (int)ffi_get_param(core, ffi_fmt_param(servo_trim_param_fmt, channel).c_str()));
    jb.field("reversed", ffi_get_param(core, ffi_fmt_param(servo_rev_param_fmt, channel).c_str()) != 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_actuator_set_limits(mg_core_t* core, int channel, int min_pwm, int max_pwm, int trim_pwm) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (channel < 1 || channel > MAX_SERVO_CHANNELS)
        return fail(MG_ERROR_INVALID_PARAM, "channel must be 1-16");

    bool all_ok = true;
    all_ok = all_ok && ffi_set_param(core, ffi_fmt_param(servo_min_param_fmt, channel).c_str(), (float)min_pwm);
    all_ok = all_ok && ffi_set_param(core, ffi_fmt_param(servo_max_param_fmt, channel).c_str(), (float)max_pwm);
    all_ok = all_ok && ffi_set_param(core, ffi_fmt_param(servo_trim_param_fmt, channel).c_str(), (float)trim_pwm);
    return all_ok ? ok() : fail(MG_ERROR_SEND_FAILED, "set limits failed");
}

MGCORE_API char* mg_actuator_motor_test(mg_core_t* core, int motor, float throttle_pct, int duration_ms) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (motor < 1 || motor > MAX_SERVO_CHANNELS)
        return fail(MG_ERROR_INVALID_PARAM, "motor must be 1-16");
    if (throttle_pct < 0 || throttle_pct > 100)
        return fail(MG_ERROR_INVALID_PARAM, "throttle must be 0-100");

    auto* mav = get_active_mavlink(core);
    // MAV_CMD_DO_MOTOR_TEST (209)
    // p1: motor instance (1-indexed)
    // p2: throttle type (0=percent)
    // p3: throttle value (percent)
    // p4: timeout (seconds)
    // p5: motor count (0=all)
    // p6: test order (0=default)
    bool sent = mav->send_command_long(
        active_target_sys(core), active_target_comp(core),
        209, 0,
        (float)motor,      // motor instance
        0,                  // throttle type (percent)
        throttle_pct,       // throttle value
        duration_ms / 1000.0f,  // timeout seconds
        0, 0, 0);
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "motor test send failed");
}
