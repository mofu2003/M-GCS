#pragma once
// 兼容转发：实现已下沉到 core/include/core/telemetry/。
#include "core/telemetry/json_serializers.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;

using mg::core::telemetry::attitude_json;
using mg::core::telemetry::battery_json;
using mg::core::telemetry::command_ack_json;
using mg::core::telemetry::esc_status_json;
using mg::core::telemetry::extended_sys_state_json;
using mg::core::telemetry::firmware_json;
using mg::core::telemetry::firmware_version_json;
using mg::core::telemetry::global_position_json;
using mg::core::telemetry::gps_json;
using mg::core::telemetry::heartbeat_json;
using mg::core::telemetry::highres_imu_json;
using mg::core::telemetry::home_position_json;
using mg::core::telemetry::imu_json;
using mg::core::telemetry::prearm_check_json;
using mg::core::telemetry::pressure_json;
using mg::core::telemetry::rc_channels_json;
using mg::core::telemetry::servo_output_json;
using mg::core::telemetry::statustext_json;
using mg::core::telemetry::sysstatus_json;
using mg::core::telemetry::vfr_hud_json;
using mg::core::telemetry::vibration_json;
using mg::core::telemetry::wind_estimate_json;
} // namespace mg::bridge
