#pragma once
// 兼容转发：TelemetryCache 已下沉到 core/include/core/telemetry/，
// 这样 core 不再反向依赖 bridge 目录（ARCH-001）。
#include "core/telemetry/telemetry_cache.h"

// 保持原有的传递式可见性：以前包含 telemetry_cache.h 就能拿到这些序列化器。
#include "json_serializers.h"
#include "sensor_health_json_serializers.h"
#include "telemetry_freshness.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;
using mg::core::telemetry::MA;
using mg::core::telemetry::TelemetryCache;
using mg::core::telemetry::TelemetryFreshness;
using mg::core::telemetry::TelemetryGroup;
using mg::core::telemetry::TelemetryType;

using mg::core::telemetry::AttitudeGroup;
using mg::core::telemetry::BatteryGroup;
using mg::core::telemetry::EscStatusGroup;
using mg::core::telemetry::ExtendedSysStateGroup;
using mg::core::telemetry::FirmwareVersionGroup;
using mg::core::telemetry::GlobalPositionGroup;
using mg::core::telemetry::GpsGroup;
using mg::core::telemetry::HeartbeatGroup;
using mg::core::telemetry::HighresImuGroup;
using mg::core::telemetry::HomePositionGroup;
using mg::core::telemetry::ImuGroup;
using mg::core::telemetry::PrearmCheckGroup;
using mg::core::telemetry::PressureGroup;
using mg::core::telemetry::RcChannelsGroup;
using mg::core::telemetry::ServoOutputGroup;
using mg::core::telemetry::StatustextGroup;
using mg::core::telemetry::SysStatusGroup;
using mg::core::telemetry::VfrHudGroup;
using mg::core::telemetry::VibrationGroup;
using mg::core::telemetry::WindEstimateGroup;
} // namespace mg::bridge
