#pragma once
// 兼容转发：实现已下沉到 core/include/core/telemetry/。
#include "core/telemetry/param_json_serializers.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;

using mg::core::telemetry::param_detail_json;
using mg::core::telemetry::param_json;
using mg::core::telemetry::param_not_found_json;
using mg::core::telemetry::param_progress_json;
} // namespace mg::bridge
