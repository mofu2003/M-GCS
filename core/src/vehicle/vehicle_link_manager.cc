#include "core/vehicle/vehicle_link_manager.h"

#include <algorithm>
#include <chrono>

namespace mg::core::vehicle {

namespace {

uint64_t steady_now_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

VehicleLinkManager::VehicleLinkManager(NowMs now_ms,
                                       uint64_t heartbeat_timeout_ms)
    : now_ms_(std::move(now_ms)),
      heartbeat_timeout_ms_(heartbeat_timeout_ms == 0
                                ? kDefaultHeartbeatTimeoutMs
                                : heartbeat_timeout_ms) {}

uint64_t VehicleLinkManager::now_ms() const {
    return now_ms_ ? now_ms_() : steady_now_ms();
}

bool VehicleLinkManager::add_link(int route_id) {
    if (route_id <= 0) return false;
    const uint64_t now = now_ms();
    mg::core::platform::LockGuard lock(mtx_);
    if (links_.count(route_id) != 0) return false;
    LinkInfo info;
    info.last_heartbeat_ms = now;
    links_.emplace(route_id, info);
    if (primary_route_id_ <= 0 || links_.find(primary_route_id_) == links_.end()) {
        primary_route_id_ = route_id;
    }
    communication_lost_ = false;
    return true;
}

bool VehicleLinkManager::remove_link(int route_id) {
    const uint64_t now = now_ms();
    mg::core::platform::LockGuard lock(mtx_);
    const auto erased = links_.erase(route_id);
    if (erased == 0) return false;
    update_primary_locked(now);
    update_communication_lost_locked();
    return true;
}

bool VehicleLinkManager::record_heartbeat(int route_id) {
    const uint64_t now = now_ms();
    mg::core::platform::LockGuard lock(mtx_);
    const auto it = links_.find(route_id);
    if (it == links_.end()) return false;
    it->second.last_heartbeat_ms = now;
    it->second.comm_lost = false;
    if (primary_route_id_ <= 0 ||
        links_.find(primary_route_id_) == links_.end() ||
        links_.at(primary_route_id_).comm_lost) {
        primary_route_id_ = route_id;
    }
    communication_lost_ = false;
    return true;
}

void VehicleLinkManager::tick() {
    const uint64_t now = now_ms();
    mg::core::platform::LockGuard lock(mtx_);
    for (auto& [route_id, info] : links_) {
        const uint64_t age = now >= info.last_heartbeat_ms
            ? now - info.last_heartbeat_ms
            : 0;
        info.comm_lost = age > heartbeat_timeout_ms_;
    }
    update_primary_locked(now);
    update_communication_lost_locked();
}

void VehicleLinkManager::clear() {
    mg::core::platform::LockGuard lock(mtx_);
    links_.clear();
    primary_route_id_ = -1;
    communication_lost_ = false;
}

int VehicleLinkManager::primary_route_id() const {
    mg::core::platform::LockGuard lock(mtx_);
    return primary_route_id_;
}

bool VehicleLinkManager::communication_lost() const {
    mg::core::platform::LockGuard lock(mtx_);
    return communication_lost_;
}

size_t VehicleLinkManager::link_count() const {
    mg::core::platform::LockGuard lock(mtx_);
    return links_.size();
}

std::vector<VehicleLinkStatus> VehicleLinkManager::statuses() const {
    const uint64_t now = now_ms();
    mg::core::platform::LockGuard lock(mtx_);
    std::vector<VehicleLinkStatus> result;
    result.reserve(links_.size());
    for (const auto& [route_id, info] : links_) {
        result.push_back(VehicleLinkStatus{
            route_id,
            route_id == primary_route_id_,
            info.comm_lost,
            now >= info.last_heartbeat_ms ? now - info.last_heartbeat_ms : 0,
        });
    }
    return result;
}

void VehicleLinkManager::update_primary_locked(uint64_t now) {
    if (primary_route_id_ > 0) {
        const auto primary = links_.find(primary_route_id_);
        if (primary != links_.end()) {
            const uint64_t age = now >= primary->second.last_heartbeat_ms
                ? now - primary->second.last_heartbeat_ms
                : 0;
            if (!primary->second.comm_lost && age <= heartbeat_timeout_ms_) {
                return;
            }
        }
    }

    primary_route_id_ = -1;
    uint64_t best_heartbeat = 0;
    for (const auto& [route_id, info] : links_) {
        if (info.comm_lost) continue;
        if (primary_route_id_ < 0 || info.last_heartbeat_ms >= best_heartbeat) {
            primary_route_id_ = route_id;
            best_heartbeat = info.last_heartbeat_ms;
        }
    }
}

void VehicleLinkManager::update_communication_lost_locked() {
    communication_lost_ = !links_.empty() &&
        std::all_of(links_.begin(), links_.end(), [](const auto& entry) {
            return entry.second.comm_lost;
        });
}

} // namespace mg::core::vehicle
