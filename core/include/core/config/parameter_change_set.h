/**
 * parameter_change_set.h - Parameter change-set planning
 *
 * Turns imported or staged parameter values into a deterministic, cache-aware
 * plan. Transport dispatch remains outside this module.
 */
#pragma once

#include "core/config/param_file_io.h"
#include "core/mavlink/mavlink_adapter.h"

#include <map>
#include <string>
#include <vector>

namespace mg::core::config {

enum class ParameterChangeDisposition {
    Ready,
    Unchanged,
    Unknown,
    Invalid,
};

struct ParameterChange {
    ParamEntry entry;
    ParameterChangeDisposition disposition = ParameterChangeDisposition::Ready;
    std::string reason;
};

struct ParameterChangeSet {
    std::vector<ParameterChange> changes;
    int ready_count = 0;
    int unchanged_count = 0;
    int unknown_count = 0;
    int invalid_count = 0;
};

class ParameterChangeSetPlanner {
public:
    /// Produces a de-duplicated change-set against the flight controller cache.
    static ParameterChangeSet prepare(
        const std::vector<ParamEntry>& requested,
        const std::vector<mg::core::mavlink::ParamInfo>& current);

    /// Orders ready changes so enabling parameters are written after dependents.
    static std::vector<ParameterChange> ordered_ready(
        const ParameterChangeSet& change_set);
};

} // namespace mg::core::config
