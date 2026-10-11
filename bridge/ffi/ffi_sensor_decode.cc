#define MGCORE_EXPORTS
/**
 * Sensor decode and identification FFI entrypoints.
 */
#include "ffi_internal.h"
#include "sensor_decode_json_serializers.h"

namespace {

float get_pval(mg_core_t* core, const char* name) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return 0;
    auto p = mav->get_param_by_name(name);
    return p.has_value() ? p->value : 0;
}

}  // namespace

MGCORE_API char* mg_sensor_decode_device_id(mg_core_t* core, int device_id, const char* category) {
    CHECK_CORE(core);
    if (!category) return to_cstr(R"({"valid":false})");
    auto result = get_active_sensor_decoder(core).decode_device_id(device_id, category);
    return to_cstr(mg::bridge::sensor_decode_device_json(
        result.name, result.sensor_type, result.family,
        result.bus, result.address, result.devtype, result.valid, device_id));
}

MGCORE_API char* mg_sensor_decode_type_param(mg_core_t* core, int type_value, const char* category) {
    CHECK_CORE(core);
    if (!category) return to_cstr(R"({"valid":false})");
    auto result = get_active_sensor_decoder(core).decode_type_param(type_value, category);
    return to_cstr(mg::bridge::sensor_decode_type_json(result.name, result.family, result.valid));
}

MGCORE_API char* mg_sensor_decode_px4_id(mg_core_t* core, int device_id, const char* category) {
    CHECK_CORE(core);
    auto decoded = get_active_sensor_decoder(core).decode_px4_id(device_id, (category && category[0]) ? category : "imu");
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("device_id", device_id);
    jb.field("name", decoded.name);
    jb.field("type", decoded.sensor_type);
    jb.field("family", decoded.family);
    jb.field("devtype", decoded.devtype);
    jb.field("bus_type", decoded.bus_type);
    jb.field("bus", decoded.bus);
    jb.field("address", decoded.address);
    jb.field("valid", decoded.valid);
    jb.field("encoding", "px4");
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_sensor_decode_px4_type(mg_core_t* core, int type_value, const char* category) {
    CHECK_CORE(core);
    auto decoded = get_active_sensor_decoder(core).decode_px4_type_value(type_value, (category && category[0]) ? category : "imu");
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("type_value", type_value);
    jb.field("name", decoded.name);
    jb.field("type", decoded.sensor_type);
    jb.field("family", decoded.family);
    jb.field("valid", decoded.valid);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_sensors_identify_all(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");
    auto hb = mav->latest_heartbeat();
    bool px4 = hb.has_value() && hb->autopilot_type == 12;
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", px4 ? "PX4" : "ArduPilot");
    {
        mg::core::telemetry::JsonBuilder arr;
        arr.begin_array();
        if (!px4) {
            const char* gyro_params[] = {"INS_GYR_ID", "INS_GYR2_ID", "INS_GYR3_ID", nullptr};
            const char* accel_params[] = {"INS_ACC_ID", "INS_ACC2_ID", "INS_ACC3_ID", nullptr};
            for (int i = 0; gyro_params[i]; i++) {
                int device_id = static_cast<int>(get_pval(core, gyro_params[i]));
                if (device_id == 0) continue;
                auto d = get_active_sensor_decoder(core).decode_device_id(device_id, "ins");
                mg::core::telemetry::JsonBuilder obj;
                obj.begin_object();
                obj.field("param", gyro_params[i]);
                obj.field("device_id", device_id);
                obj.field("role", "gyro");
                obj.field("name", d.name);
                obj.field("sensor_type", d.sensor_type);
                obj.field("family", d.family);
                obj.field("valid", d.valid);
                obj.end_object();
                arr.append_json(obj.str());
            }
            for (int i = 0; accel_params[i]; i++) {
                int device_id = static_cast<int>(get_pval(core, accel_params[i]));
                if (device_id == 0) continue;
                auto d = get_active_sensor_decoder(core).decode_device_id(device_id, "ins");
                mg::core::telemetry::JsonBuilder obj;
                obj.begin_object();
                obj.field("param", accel_params[i]);
                obj.field("device_id", device_id);
                obj.field("role", "accel");
                obj.field("name", d.name);
                obj.field("sensor_type", d.sensor_type);
                obj.field("family", d.family);
                obj.field("valid", d.valid);
                obj.end_object();
                arr.append_json(obj.str());
            }
        }
        arr.end_array();
        jb.raw_field("imu", arr.str());
    }
    {
        mg::core::telemetry::JsonBuilder arr;
        arr.begin_array();
        if (!px4) {
            const char* params[] = {"COMPASS_DEV_ID", "COMPASS_DEV_ID2", "COMPASS_DEV_ID3", nullptr};
            for (int i = 0; params[i]; i++) {
                int device_id = static_cast<int>(get_pval(core, params[i]));
                if (device_id == 0) continue;
                auto d = get_active_sensor_decoder(core).decode_device_id(device_id, "compass");
                mg::core::telemetry::JsonBuilder obj;
                obj.begin_object();
                obj.field("param", params[i]);
                obj.field("device_id", device_id);
                obj.field("name", d.name);
                obj.field("sensor_type", d.sensor_type);
                obj.field("family", d.family);
                obj.field("valid", d.valid);
                obj.end_object();
                arr.append_json(obj.str());
            }
        }
        arr.end_array();
        jb.raw_field("compass", arr.str());
    }
    {
        mg::core::telemetry::JsonBuilder arr;
        arr.begin_array();
        if (!px4) {
            const char* params[] = {"BARO1_DEVID", "BARO2_DEVID", "BARO3_DEVID", nullptr};
            for (int i = 0; params[i]; i++) {
                int device_id = static_cast<int>(get_pval(core, params[i]));
                if (device_id == 0) continue;
                auto d = get_active_sensor_decoder(core).decode_device_id(device_id, "baro");
                mg::core::telemetry::JsonBuilder obj;
                obj.begin_object();
                obj.field("param", params[i]);
                obj.field("device_id", device_id);
                obj.field("name", d.name);
                obj.field("sensor_type", d.sensor_type);
                obj.field("family", d.family);
                obj.field("valid", d.valid);
                obj.end_object();
                arr.append_json(obj.str());
            }
        }
        arr.end_array();
        jb.raw_field("baro", arr.str());
    }
    auto* cache = get_active_cache(core);
    if (cache) {
        auto sh = cache->snapshot_sensor_health();
        mg::core::telemetry::JsonBuilder hp;
        hp.begin_object();
        hp.field("present", static_cast<int64_t>(sh.present));
        hp.field("enabled", static_cast<int64_t>(sh.enabled));
        hp.field("healthy", static_cast<int64_t>(sh.healthy));
        hp.field("gyro_healthy", sh.gyro_healthy);
        hp.field("accel_healthy", sh.accel_healthy);
        hp.field("mag_healthy", sh.mag_healthy);
        hp.field("baro_healthy", sh.baro_healthy);
        hp.end_object();
        jb.raw_field("health", hp.str());
    }
    jb.end_object();
    return to_cstr(jb.str());
}
