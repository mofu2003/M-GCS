#define MGCORE_EXPORTS
/**
 * Normalized power, sensor, and health telemetry FFI entrypoints.
 */
#include "ffi_response.h"


MGCORE_API char* mg_vehicle_get_battery(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"voltage_v":0,"current_a":0,"remaining_pct":-1,"valid":false})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto bat_snap = cache.snapshot_battery();
            auto ss_snap = cache.snapshot_sys_status();
            const auto& bat = bat_snap.data;
            const auto& ss = ss_snap.data;
            double voltage_v = (bat.voltage_v > 0) ? bat.voltage_v : ss.battery_voltage / 1000.0;
            double current_a = (bat.current_a != 0) ? bat.current_a : ss.battery_current / 100.0;
            int remaining = (bat.battery_remaining >= 0) ? bat.battery_remaining : ss.battery_remaining;
            data.field("voltage_v", voltage_v);
            data.field("current_a", current_a);
            data.field("remaining_pct", remaining);
            data.field("valid", voltage_v > 0);
        });
}

MGCORE_API char* mg_vehicle_get_rc_channels(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"channel_count":0,"rssi":0,"channels":[],"valid":false})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto rc_snap = cache.snapshot_rc();
            const auto& rc = rc_snap.data;
            data.field("channel_count", (int)rc.chancount);
            data.field("rssi",          (int)rc.rssi);
            mg::core::telemetry::JsonBuilder ch;
            ch.begin_array();
            for (int i = 0; i < (int)rc.chancount && i < 18; i++) {
                ch.append_json(std::to_string((int)rc.chan[i]));
            }
            ch.end_array();
            data.raw_field("channels", ch.str());
            data.field("valid", rc.chancount > 0);
        });
}

MGCORE_API char* mg_vehicle_get_servo_output(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"servos":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"valid":false})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto sv_snap = cache.snapshot_servo();
            const auto& sv = sv_snap.data;
            mg::core::telemetry::JsonBuilder servos;
            servos.begin_array();
            for (int i = 0; i < 16; i++) {
                servos.append_json(std::to_string((int)sv.servo[i]));
            }
            servos.end_array();
            data.raw_field("servos", servos.str());
            bool valid = false;
            for (int i = 0; i < 16; i++) {
                if (sv.servo[i] > 0) {
                    valid = true;
                    break;
                }
            }
            data.field("valid", valid);
        });
}

MGCORE_API char* mg_vehicle_get_imu(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"acc_mg":{"x":0,"y":0,"z":0},"gyro_mrs":{"x":0,"y":0,"z":0},"mag_mg":{"x":0,"y":0,"z":0},"temperature_c":0})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_imu();
            const auto& d = snap.data;
            mg::core::telemetry::JsonBuilder acc; acc.begin_object(); acc.field("x", (int)d.xacc); acc.field("y", (int)d.yacc); acc.field("z", (int)d.zacc); acc.end_object();
            mg::core::telemetry::JsonBuilder gyro; gyro.begin_object(); gyro.field("x", (int)d.xgyro); gyro.field("y", (int)d.ygyro); gyro.field("z", (int)d.zgyro); gyro.end_object();
            mg::core::telemetry::JsonBuilder mag; mag.begin_object(); mag.field("x", (int)d.xmag); mag.field("y", (int)d.ymag); mag.field("z", (int)d.zmag); mag.end_object();
            data.raw_field("acc_mg", acc.str());
            data.raw_field("gyro_mrs", gyro.str());
            data.raw_field("mag_mg", mag.str());
            data.field("temperature_c", d.temperature / 100.0);
        });
}

MGCORE_API char* mg_vehicle_get_pressure(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"abs_pressure_hpa":0,"diff_pressure_hpa":0,"temperature_c":0})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_pressure();
            const auto& d = snap.data;
            data.field("abs_pressure_hpa",  d.press_abs);
            data.field("diff_pressure_hpa", d.press_diff);
            data.field("temperature_c",     d.temperature / 100.0);
        });
}

MGCORE_API char* mg_vehicle_get_highres_imu(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"acc_m_s2":{"x":0,"y":0,"z":0},"gyro_rps":{"x":0,"y":0,"z":0},"mag_gauss":{"x":0,"y":0,"z":0},"abs_pressure_hpa":0,"diff_pressure_hpa":0,"pressure_alt_m":0,"temperature_c":0,"fields_updated":0})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_highres_imu();
            const auto& d = snap.data;
            mg::core::telemetry::JsonBuilder hacc; hacc.begin_object(); hacc.field("x", (double)d.xacc); hacc.field("y", (double)d.yacc); hacc.field("z", (double)d.zacc); hacc.end_object();
            mg::core::telemetry::JsonBuilder hgyro; hgyro.begin_object(); hgyro.field("x", (double)d.xgyro); hgyro.field("y", (double)d.ygyro); hgyro.field("z", (double)d.zgyro); hgyro.end_object();
            mg::core::telemetry::JsonBuilder hmag; hmag.begin_object(); hmag.field("x", (double)d.xmag); hmag.field("y", (double)d.ymag); hmag.field("z", (double)d.zmag); hmag.end_object();
            data.raw_field("acc_m_s2", hacc.str());
            data.raw_field("gyro_rps", hgyro.str());
            data.raw_field("mag_gauss", hmag.str());
            data.field("abs_pressure_hpa", d.abs_pressure);
            data.field("diff_pressure_hpa", d.diff_pressure);
            data.field("pressure_alt_m", d.pressure_alt);
            data.field("temperature_c", d.temperature);
            data.field("fields_updated", (int)d.fields_updated);
        });
}

MGCORE_API char* mg_vehicle_get_firmware_version(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"received":false,"major":0,"minor":0,"patch":0,"vendor_id":0,"product_id":0,"capabilities":0,"git_hash":""})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_firmware();
            const auto& d = snap.data;
            data.field("received",     d.received);
            data.field("major",        (int)d.firmware_major);
            data.field("minor",        (int)d.firmware_minor);
            data.field("patch",        (int)d.firmware_patch);
            data.field("vendor_id",    (int)d.vendor_id);
            data.field("product_id",   (int)d.product_id);
            data.field("capabilities", (int64_t)d.capabilities);
            data.field("git_hash",     std::string(d.git_hash));
        });
}

MGCORE_API char* mg_vehicle_get_vibration(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"vibration_x":0,"vibration_y":0,"vibration_z":0,"clipping_0":0,"clipping_1":0,"clipping_2":0})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_vibration();
            const auto& d = snap.data;
            data.field("vibration_x", d.vibration_x);
            data.field("vibration_y", d.vibration_y);
            data.field("vibration_z", d.vibration_z);
            data.field("clipping_0", (int64_t)d.clipping_0);
            data.field("clipping_1", (int64_t)d.clipping_1);
            data.field("clipping_2", (int64_t)d.clipping_2);
        });
}

MGCORE_API char* mg_vehicle_get_esc_status(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"index":0,"count":0,"esc":{"rpm":0,"voltage":0,"current":0,"temperature_c":0}})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_esc();
            const auto& d = snap.data;
            data.field("index", (int)d.index);
            data.field("count", (int)d.count);
            mg::core::telemetry::JsonBuilder esc;
            esc.begin_object();
            esc.field("rpm",       (int)d.rpm);
            esc.field("voltage",    d.voltage);
            esc.field("current",    d.current);
            esc.field("temperature_c", d.temperature);
            esc.end_object();
            data.raw_field("esc", esc.str());
        });
}

MGCORE_API char* mg_vehicle_get_prearm_check(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"check_mask":0,"ready":true})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_prearm();
            const auto& d = snap.data;
            data.field("check_mask", (int64_t)d.check_mask);
            data.field("ready",      d.check_mask == 0);
        });
}
