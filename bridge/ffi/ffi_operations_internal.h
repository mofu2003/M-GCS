#pragma once

#include "ffi_internal.h"

#include <string>

namespace mg::bridge::operations_detail {

std::string operations_snapshot_json(mg_core_t* core);

} // namespace mg::bridge::operations_detail
