#pragma once
// 兼容转发：实现已下沉到 core/include/core/telemetry/，桥接层保留原名字。
#include "core/telemetry/json_builder.h"

namespace mg::bridge {
using mg::core::telemetry::JsonBuilder;
} // namespace mg::bridge
