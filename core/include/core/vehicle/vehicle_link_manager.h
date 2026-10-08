#pragma once

// Per-Vehicle link health and primary-link selection.
//
// The current Vehicle lifecycle still has one active ConnectionManager per
// Vehicle. This module is the state layer needed to support several links per
// Vehicle without pushing socket/lifecycle details into Vehicle itself.

#include "core/core_types.h"
#include "core/platform/platform_mutex.h"

#include <functional>
#include <map>
#include <vector>

namespace mg::core::vehicle {

struct VehicleLinkStatus {
    int route_id = -1;
    bool primary = false;
    bool comm_lost = false;
    uint64_t heartbeat_age_ms = 0;
};

class VehicleLinkManager {
public:
    using NowMs = std::function<uint64_t()>;

    static constexpr uint64_t kDefaultHeartbeatTimeoutMs = 3500;

    explicit VehicleLinkManager(
        NowMs now_ms = {},
        uint64_t heartbeat_timeout_ms = kDefaultHeartbeatTimeoutMs);
    ~VehicleLinkManager() = default;

    VehicleLinkManager(const VehicleLinkManager&) = delete;
    VehicleLinkManager& operator=(const VehicleLinkManager&) = delete;

    bool add_link(int route_id);
    bool remove_link(int route_id);
    bool record_heartbeat(int route_id);

    /// Recomputes communication-lost state and selects a live primary link.
    void tick();
    void clear();

    int primary_route_id() const;
    bool communication_lost() const;
    size_t link_count() const;
    std::vector<VehicleLinkStatus> statuses() const;

private:
    struct LinkInfo {
        uint64_t last_heartbeat_ms = 0;
        bool comm_lost = false;
    };

    uint64_t now_ms() const;
    void update_primary_locked(uint64_t now);
    void update_communication_lost_locked();

    mutable mg::core::platform::Mutex mtx_;
    std::map<int, LinkInfo> links_;
    int primary_route_id_ = -1;
    bool communication_lost_ = false;
    NowMs now_ms_;
    uint64_t heartbeat_timeout_ms_ = kDefaultHeartbeatTimeoutMs;
};

} // namespace mg::core::vehicle
