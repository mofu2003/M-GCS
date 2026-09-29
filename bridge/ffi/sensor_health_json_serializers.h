#pragma once
// 兼容转发：实现已下沉到 core/include/core/telemetry/。
#include "core/telemetry/sensor_health_json_serializers.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;
using mg::core::telemetry::SensorHealthBitDef;

using mg::core::telemetry::extended_sensor_health_bit_defs;
using mg::core::telemetry::sensor_health_bit_defs;
using mg::core::telemetry::sensor_health_item_json;
using mg::core::telemetry::sensor_health_json;
using mg::core::telemetry::sensor_health_status_name;
} // namespace mg::bridge
