/**
 * event_codes.h - core 事件码
 *
 * Vehicle 通过 EventCallback(int, json) 把状态变化抛给上层。core 不能依赖
 * bridge（A1），所以事件码在这里定义一份；bridge 侧的 mg_event_type_t 必须与
 * 之数值一致，由 bridge/ffi/ffi_connection.cc 的 static_assert 钉死。
 *
 * 新增事件时：这里加一个码，ffi_api_core.h 的 mg_event_type_t 加同名项，
 * 并在那组 static_assert 里补一行；顺序不要插在中间。
 */
#pragma once

namespace mg::core::event {

enum Code : int {
    kHeartbeat          = 0,
    kSysStatus          = 1,
    kGps                = 2,
    kAttitude           = 3,
    kConnection         = 4,
    kParameters         = 5,
    kMission            = 6,
    kLog                = 7,
    kTelemetry          = 8,
    kFirmware           = 9,
    kSensorHealth       = 10,
    kImu                = 11,
    kPressure           = 12,
    kGlobalPosition     = 13,
    kHomePosition       = 14,
    kRcChannels         = 15,
    kServoOutput        = 16,
    kStatustext         = 17,
    kBattery            = 18,
    kHighresImu         = 19,
    kExtendedSysState   = 20,
    kInitialConnect     = 21,
    kVibration          = 22,
    kEscStatus          = 23,
    kWindEstimate       = 24,
    kPrearmCheck        = 25,
    kWaypointReached    = 26,
    kDistanceSensor     = 27,
    kBatteryFailsafe    = 28,
    kAdsb               = 29,
};

} // namespace mg::core::event
