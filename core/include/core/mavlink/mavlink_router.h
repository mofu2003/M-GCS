#pragma once

// Shared-link MAVLink demultiplexer.
//
// A physical link may carry several MAVLink systems (for example two SITL
// instances behind one UDP socket). The router parses the byte stream once
// and forwards each complete frame to the adapter registered for its
// (system_id, component_id). It deliberately does not own a transport; the
// LinkSession/ConnectionManager layer owns transport and lifecycle.

#include "core/mavlink/mavlink_adapter.h"
#include "core/core_types.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace mg::core::mavlink {

struct MavlinkDiscoveredTarget {
    int route_id = -1;
    std::shared_ptr<MavlinkAdapter> adapter;
};

/// Transport-neutral sender identity passed to discovery callbacks.
struct MavlinkPeer {
    std::string host;
    uint16_t port = 0;

    bool valid() const { return !host.empty() && port > 0; }
};

class MavlinkRouter {
public:
    /// Called for a heartbeat whose (sysid, compid) has no registered target.
    using DiscoveryCallback = std::function<std::optional<MavlinkDiscoveredTarget>(
        const HeartbeatInfo& heartbeat,
        const MavlinkPeer& sender)>;

    MavlinkRouter();
    ~MavlinkRouter();

    MavlinkRouter(const MavlinkRouter&) = delete;
    MavlinkRouter& operator=(const MavlinkRouter&) = delete;

    /// Registers one exact MAVLink identity. A duplicate identity is rejected
    /// rather than silently stealing traffic from another vehicle.
    bool add_target(int route_id,
                    uint8_t system_id,
                    uint8_t component_id,
                    std::shared_ptr<MavlinkAdapter> adapter);

    /// Updates identity after the first heartbeat. Returns false if the route
    /// is unknown or the new identity is already registered.
    bool update_target_identity(int route_id,
                                uint8_t system_id,
                                uint8_t component_id);

    void remove_target(int route_id);
    void set_discovery_callback(DiscoveryCallback callback);

    /// Feeds raw bytes, returns the route IDs that received at least one frame.
    /// The first parse uses caller-owned state, so links do not share MAVLink
    /// parser state with each other.
    std::vector<int> route_bytes(const uint8_t* data,
                                 size_t size,
                                 const MavlinkPeer* sender = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mg::core::mavlink
