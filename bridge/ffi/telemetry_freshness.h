#pragma once
// 兼容转发：实现已下沉到 core/include/core/telemetry/。
#include "core/telemetry/telemetry_freshness.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;
using mg::core::telemetry::TelemetryFreshness;
using mg::core::telemetry::TelemetryType;
} // namespace mg::bridge
