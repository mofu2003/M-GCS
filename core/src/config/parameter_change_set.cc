/**
 * parameter_change_set.cc - Parameter change-set planning implementation
 */
#include "core/config/parameter_change_set.h"

#include <algorithm>
#include <cmath>

namespace mg::core::config {

namespace {

bool writes_last(const std::string& name) {
    constexpr const char* suffix = "_ENABLE";
    const auto suffix_length = std::char_traits<char>::length(suffix);
    return name.size() >= suffix_length &&
           name.compare(name.size() - suffix_length, suffix_length, suffix) == 0;
}

} // namespace

ParameterChangeSet ParameterChangeSetPlanner::prepare(
    const std::vector<ParamEntry>& requested,
    const std::vector<mg::core::mavlink::ParamInfo>& current) {
    std::map<std::string, mg::core::mavlink::ParamInfo> current_by_name;
    for (const auto& parameter : current) {
        current_by_name[parameter.name] = parameter;
    }

    std::map<std::string, ParamEntry> requested_by_name;
    for (const auto& entry : requested) {
        if (entry.name.empty() || !std::isfinite(entry.value)) continue;
        requested_by_name[entry.name] = entry;
    }

    ParameterChangeSet result;
    result.changes.reserve(requested_by_name.size());
    for (const auto& [name, entry] : requested_by_name) {
        ParameterChange change;
        change.entry = entry;
        const auto found = current_by_name.find(name);
        if (found == current_by_name.end()) {
            change.disposition = ParameterChangeDisposition::Unknown;
            change.reason = "parameter not reported by flight controller";
            result.unknown_count++;
        } else if (found->second.value == entry.value) {
            change.disposition = ParameterChangeDisposition::Unchanged;
            change.entry.type = found->second.type;
            result.unchanged_count++;
        } else {
            change.entry.type = found->second.type;
            result.ready_count++;
        }
        result.changes.push_back(std::move(change));
    }
    return result;
}

std::vector<ParameterChange> ParameterChangeSetPlanner::ordered_ready(
    const ParameterChangeSet& change_set) {
    std::vector<ParameterChange> result;
    result.reserve(change_set.ready_count);
    for (const auto& change : change_set.changes) {
        if (change.disposition == ParameterChangeDisposition::Ready) {
            result.push_back(change);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        const auto left_last = writes_last(left.entry.name);
        const auto right_last = writes_last(right.entry.name);
        if (left_last != right_last) return !left_last;
        return left.entry.name < right.entry.name;
    });
    return result;
}

} // namespace mg::core::config
