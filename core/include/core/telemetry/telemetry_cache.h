/**
 * telemetry_cache.h - 遥测缓存（模板化，消除 20 个重复 group struct）
 *
 * 核心设计：
 *   TelemetryGroup<Info, MavlinkAdapter, LatestFn, JsonFn> — 通用模板
 *   SysStatusGroup / StatustextGroup — 特化（有额外方法或不同更新逻辑）
 *   RegistrationTable — 替代 20 case switch
 *
 * 添加新遥测类型 = 1 行注册表条目 + 1 个 Info struct + 1 个 json 函数
 */

#ifndef MGCORE_TELEMETRY_CACHE_H
#define MGCORE_TELEMETRY_CACHE_H

#include "core/mavlink/mavlink_adapter.h"
#include "core/platform/platform_mutex.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/sensor_health_json_serializers.h"
#include "core/telemetry/telemetry_freshness.h"
#include <string>
#include <cctype>
#include <vector>

namespace mg::core::telemetry {

// ================================================================
// 通用遥测 group 模板
//
// 替代 17 个完全相同的 group struct。
// Info:       数据类型（HeartbeatInfo, GpsInfo, ...）
// LatestFn:   MavlinkAdapter 上的 latest_xxx() 成员函数指针
// JsonFn:     序列化函数（heartbeat_json, gps_json, ...）
// ================================================================

template<typename Info, typename Adapter,
         std::optional<Info> (Adapter::*LatestFn)() const,
         std::string (*JsonFn)(const Info&)>
struct TelemetryGroup {
    Info data{};

    void update(const Adapter* mav) {
        auto v = (mav->*LatestFn)();
        if (v) data = *v;
    }
    std::string to_json() const { return JsonFn(data); }
};

// ================================================================
// 特化：SysStatusGroup（额外 health_json 方法）
// ================================================================

struct SysStatusGroup {
    mg::core::mavlink::SysStatusInfo data{};
    uint8_t autopilot_type = 0;

    void update(const mg::core::mavlink::MavlinkAdapter* mav) {
        auto v = mav->latest_sys_status();
        if (v) data = *v;
        auto hb = mav->latest_heartbeat();
        if (hb) autopilot_type = hb->autopilot_type;
    }
    std::string to_json() const { return sysstatus_json(data); }
    std::string health_json() const { return sensor_health_json(data, autopilot_type); }
};

// ================================================================
// 特化：StatustextGroup（文本消息，更新方式不同）
// ================================================================

struct StatustextGroup {
    mg::core::mavlink::StatustextInfo data{};

    void update(const mg::core::mavlink::MavlinkAdapter* mav) {
        auto v = mav->latest_statustext_info();
        if (v) data = *v;
    }
    std::string to_json() const { return statustext_json(data); }
};

// ================================================================
// 遥测类型枚举
// ================================================================

// ================================================================
// 类型别名 — 17 个标准 group（模板实例化）
// ================================================================

using MA = mg::core::mavlink::MavlinkAdapter;

using HeartbeatGroup       = TelemetryGroup<mg::core::mavlink::HeartbeatInfo,       MA, &MA::latest_heartbeat,       heartbeat_json>;
using GpsGroup             = TelemetryGroup<mg::core::mavlink::GpsInfo,             MA, &MA::latest_gps,             gps_json>;
using AttitudeGroup        = TelemetryGroup<mg::core::mavlink::AttitudeInfo,        MA, &MA::latest_attitude,        attitude_json>;
using VfrHudGroup          = TelemetryGroup<mg::core::mavlink::VfrHudInfo,          MA, &MA::latest_vfr_hud,         vfr_hud_json>;
using ImuGroup             = TelemetryGroup<mg::core::mavlink::ImuInfo,             MA, &MA::latest_imu,             imu_json>;
using PressureGroup        = TelemetryGroup<mg::core::mavlink::PressureInfo,        MA, &MA::latest_pressure,        pressure_json>;
using FirmwareVersionGroup = TelemetryGroup<mg::core::mavlink::FirmwareVersionInfo, MA, &MA::latest_firmware_version, firmware_version_json>;
using RcChannelsGroup      = TelemetryGroup<mg::core::mavlink::RcChannelsInfo,      MA, &MA::latest_rc_channels,     rc_channels_json>;
using ServoOutputGroup     = TelemetryGroup<mg::core::mavlink::ServoOutputInfo,     MA, &MA::latest_servo_output,    servo_output_json>;
using GlobalPositionGroup  = TelemetryGroup<mg::core::mavlink::GlobalPositionInfo,  MA, &MA::latest_global_position, global_position_json>;
using HomePositionGroup    = TelemetryGroup<mg::core::mavlink::HomePositionInfo,    MA, &MA::latest_home_position,   home_position_json>;
using BatteryGroup         = TelemetryGroup<mg::core::mavlink::BatteryInfo,         MA, &MA::latest_battery,         battery_json>;
using HighresImuGroup      = TelemetryGroup<mg::core::mavlink::HighresImuInfo,      MA, &MA::latest_highres_imu,     highres_imu_json>;
using ExtendedSysStateGroup = TelemetryGroup<mg::core::mavlink::ExtendedSysStateInfo, MA, &MA::latest_extended_sys_state, extended_sys_state_json>;
using PrearmCheckGroup     = TelemetryGroup<mg::core::mavlink::PrearmCheckInfo,     MA, &MA::latest_prearm_check,    prearm_check_json>;
using VibrationGroup       = TelemetryGroup<mg::core::mavlink::VibrationInfo,       MA, &MA::latest_vibration,       vibration_json>;
using EscStatusGroup       = TelemetryGroup<mg::core::mavlink::EscStatusInfo,       MA, &MA::latest_esc_status,      esc_status_json>;
using WindEstimateGroup    = TelemetryGroup<mg::core::mavlink::WindEstimateInfo,    MA, &MA::latest_wind_estimate,   wind_estimate_json>;

// ================================================================
// 遥测缓存
// ================================================================

struct TelemetryCache {
    // 所有 group 实例
    HeartbeatGroup        heartbeat;
    SysStatusGroup        sys_status;      // 特化：有 health_json()
    GpsGroup              gps;
    AttitudeGroup         attitude;
    VfrHudGroup           vfr_hud;
    ImuGroup              imu;
    PressureGroup         pressure;
    FirmwareVersionGroup  firmware_version;
    RcChannelsGroup       rc_channels;
    ServoOutputGroup      servo_output;
    StatustextGroup       statustext;      // 特化：文本消息
    GlobalPositionGroup   global_position;
    HomePositionGroup     home_position;
    BatteryGroup          battery;
    HighresImuGroup       highres_imu;
    ExtendedSysStateGroup extended_sys_state;
    PrearmCheckGroup      prearm_check;
    VibrationGroup        vibration;
    EscStatusGroup        esc_status;
    WindEstimateGroup     wind_estimate;
    std::string           vehicle_name;
    std::string           board_name;
    std::string           product_name;
    std::string           usb_board_name;

    // ================================================================
    // 快照访问器（线程安全，返回拷贝）
    // ================================================================

    HeartbeatGroup         snapshot_heartbeat()   { mg::core::platform::LockGuard lk(mtx); return heartbeat; }
    RcChannelsGroup        snapshot_rc()          { mg::core::platform::LockGuard lk(mtx); return rc_channels; }
    ServoOutputGroup       snapshot_servo()       { mg::core::platform::LockGuard lk(mtx); return servo_output; }
    GlobalPositionGroup    snapshot_global_pos()  { mg::core::platform::LockGuard lk(mtx); return global_position; }
    GpsGroup               snapshot_gps()         { mg::core::platform::LockGuard lk(mtx); return gps; }
    AttitudeGroup          snapshot_attitude()    { mg::core::platform::LockGuard lk(mtx); return attitude; }
    SysStatusGroup         snapshot_sys_status()  { mg::core::platform::LockGuard lk(mtx); return sys_status; }
    BatteryGroup           snapshot_battery()     { mg::core::platform::LockGuard lk(mtx); return battery; }
    VfrHudGroup            snapshot_vfr_hud()     { mg::core::platform::LockGuard lk(mtx); return vfr_hud; }
    HomePositionGroup      snapshot_home()        { mg::core::platform::LockGuard lk(mtx); return home_position; }
    FirmwareVersionGroup   snapshot_firmware()    { mg::core::platform::LockGuard lk(mtx); return firmware_version; }
    ExtendedSysStateGroup  snapshot_ext_sys()     { mg::core::platform::LockGuard lk(mtx); return extended_sys_state; }
    StatustextGroup        snapshot_statustext()  { mg::core::platform::LockGuard lk(mtx); return statustext; }
    std::string            snapshot_vehicle_name(){ mg::core::platform::LockGuard lk(mtx); return vehicle_name; }
    std::string            snapshot_board_name()  { mg::core::platform::LockGuard lk(mtx); return board_name; }
    std::string            snapshot_product_name(){ mg::core::platform::LockGuard lk(mtx); return product_name; }
    std::string            snapshot_usb_board_name(){ mg::core::platform::LockGuard lk(mtx); return usb_board_name; }

    void set_usb_board_name(const std::string& name) {
        mg::core::platform::LockGuard lk(mtx);
        usb_board_name = name;
    }

    // ================================================================
    // Statustext 更新（从回调直接写入，不经过 mavlink cache）
    // ================================================================

    void update_statustext(const std::string& text, uint8_t severity) {
        mg::core::platform::LockGuard lk(mtx);
        size_t n = text.size();
        if (n > 50) n = 50;
        memcpy(statustext.data.text, text.c_str(), n);
        if (n < 51) statustext.data.text[n] = 0;
        else statustext.data.text[50] = 0;
        statustext.data.severity = severity;
        update_vehicle_name_from_statustext_locked(text);
    }

    static bool is_upper_hex_token(const std::string& token) {
        if (token.size() != 8) return false;
        for (char ch : token) {
            const auto uch = static_cast<unsigned char>(ch);
            if (!std::isxdigit(uch)) return false;
            if (std::isalpha(uch) && !std::isupper(uch)) return false;
        }
        return true;
    }

    static bool looks_like_product_name(const std::string& token) {
        if (token.empty()) return false;
        bool has_alpha = false;
        bool has_digit = false;
        for (char ch : token) {
            const auto uch = static_cast<unsigned char>(ch);
            if (std::isalpha(uch)) has_alpha = true;
            else if (std::isdigit(uch)) has_digit = true;
            else if (ch != '-' && ch != '_' && ch != '.') return false;
        }
        return has_alpha && has_digit;
    }

    void update_vehicle_name_from_statustext_locked(const std::string& text) {
        std::vector<std::string> tokens;
        std::string token;
        for (char ch : text) {
            const auto uch = static_cast<unsigned char>(ch);
            if (std::isspace(uch) || ch == '\0') {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                token.push_back(ch);
            }
        }
        if (!token.empty()) tokens.push_back(token);

        if (tokens.size() < 4) return;
        if (!is_upper_hex_token(tokens[1]) ||
            !is_upper_hex_token(tokens[2]) ||
            !is_upper_hex_token(tokens[3])) {
            return;
        }

        if (vehicle_name.empty()) {
            vehicle_name = tokens[0];
        }
        if (board_name.empty()) {
            board_name = tokens[0];
        }
        if (product_name.empty() && looks_like_product_name(tokens[0])) {
            product_name = tokens[0];
        }
    }

    // ================================================================
    // 遥测数据新鲜度检测
    // ================================================================

    mg::core::platform::Mutex mtx;
    TelemetryFreshness freshness;

    bool is_fresh(TelemetryType type, uint32_t stale_ms = 3000) {
        mg::core::platform::LockGuard lk(mtx);
        return freshness.is_fresh(type, stale_ms);
    }

    uint32_t age_ms(TelemetryType type) {
        mg::core::platform::LockGuard lk(mtx);
        return freshness.age_ms(type);
    }

    std::string freshness_json() {
        mg::core::platform::LockGuard lk(mtx);
        return freshness.to_json();
    }

    // ================================================================
    // 更新分发 — 注册表替代 20 case switch
    //
    // fire_event=true 的 group 返回 JSON（触发前端事件）
    // fire_event=false 的 group 返回 ""（轮询更新，不触发事件）
    // ================================================================

    std::string update(TelemetryType type,
                       const mg::core::mavlink::MavlinkAdapter* mav) {
        mg::core::platform::LockGuard lk(mtx);

        freshness.mark_updated(type);

        switch (type) {
            case TelemetryType::kHeartbeat:        heartbeat.update(mav);        return heartbeat.to_json();
            case TelemetryType::kSysStatus:         sys_status.update(mav);       return sys_status.to_json();
            case TelemetryType::kGps:               gps.update(mav);              return gps.to_json();
            case TelemetryType::kAttitude:          attitude.update(mav);         return attitude.to_json();
            case TelemetryType::kVfrHud:            vfr_hud.update(mav);          return vfr_hud.to_json();
            case TelemetryType::kImu:               imu.update(mav);              return "";  // polled
            case TelemetryType::kPressure:          pressure.update(mav);         return "";  // polled
            case TelemetryType::kFirmwareVersion:   firmware_version.update(mav); return firmware_version.to_json();
            case TelemetryType::kRcChannels:        rc_channels.update(mav);      return "";  // polled
            case TelemetryType::kServoOutput:       servo_output.update(mav);     return "";  // polled
            case TelemetryType::kStatustext:        statustext.update(mav);       return statustext.to_json();
            case TelemetryType::kGlobalPosition:    global_position.update(mav);  return global_position.to_json();
            case TelemetryType::kHomePosition:      home_position.update(mav);    return "";  // polled
            case TelemetryType::kBattery:           battery.update(mav);          return battery.to_json();
            case TelemetryType::kHighresImu:        highres_imu.update(mav);      return "";  // polled
            case TelemetryType::kExtendedSysState:  extended_sys_state.update(mav); return extended_sys_state.to_json();
            case TelemetryType::kPrearmCheck:       prearm_check.update(mav);     return prearm_check.to_json();
            case TelemetryType::kVibration:         vibration.update(mav);        return vibration.to_json();
            case TelemetryType::kEscStatus:         esc_status.update(mav);       return esc_status.to_json();
            case TelemetryType::kWindEstimate:      wind_estimate.update(mav);    return wind_estimate.to_json();
            case TelemetryType::kCount:             break;
        }
        return "";
    }
};

} // namespace mg::core::telemetry

#endif /* MGCORE_TELEMETRY_CACHE_H */
