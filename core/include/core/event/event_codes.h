/**
 * event_codes.h - core 事件码
 *
 * Vehicle 通过 EventCallback(int, json) 把状态变化抛给上层。事件码只此一份：
 * FFI 不再向宿主转发事件（前端用轮询，见 known-deviations.md 的 KD-27），
 * 因此没有 bridge 侧的镜像枚举需要同步。
 *
 * 新增事件时：在这里加一个码，顺序不要插在中间（数值会出现在事件 JSON 里）。
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
