#pragma once

// One physical link shared by one or more Vehicle instances.
//
// QGC keeps links independent from vehicles; MP addresses a specific
// sysid/compid on a MAVLinkInterface. LinkSession combines those ideas for
// M-GCS: it owns the transport and one caller-owned MAVLink parser, while the
// router dispatches complete frames to per-vehicle adapters.

#include "core/mavlink/mavlink_router.h"
#include "core/platform/platform_mutex.h"
#include "core/transport/link_interface.h"

#include <map>
#include <memory>

namespace mg::core::transport {

class LinkSession : public std::enable_shared_from_this<LinkSession> {
public:
    explicit LinkSession(std::shared_ptr<ILink> link,
                         std::string endpoint = {});
    ~LinkSession();

    LinkSession(const LinkSession&) = delete;
    LinkSession& operator=(const LinkSession&) = delete;

    bool open();
    void close();
    bool is_open() const;

    ILink* link() const { return link_.get(); }
    const std::string& endpoint() const { return endpoint_; }
    std::shared_ptr<ILink> shared_link() const { return link_; }
    mg::core::mavlink::MavlinkRouter& router() { return router_; }

    /// Registers one vehicle adapter for exact sysid/compid routing.
    bool add_vehicle(int route_id,
                     uint8_t system_id,
                     uint8_t component_id,
                     std::shared_ptr<mg::core::mavlink::MavlinkAdapter> adapter);

    bool update_vehicle_identity(int route_id,
                                 uint8_t system_id,
                                 uint8_t component_id);
    void remove_vehicle(int route_id);

    /// Sends with the peer learned for route_id when the transport supports
    /// peer routing; otherwise falls back to the link's single remote.
    bool send(int route_id, const uint8_t* data, size_t size);

    void set_discovery_callback(
        mg::core::mavlink::MavlinkRouter::DiscoveryCallback callback);

    /// Record the sender for a route before discovery callbacks send a reply.
    void set_peer_for_route(int route_id, const LinkPeer& peer);

private:
    void on_bytes(const uint8_t* data, size_t size);
    void on_peer_bytes(const LinkPeer& peer,
                       const uint8_t* data,
                       size_t size);
    LinkPeer peer_for(int route_id) const;

    std::shared_ptr<ILink> link_;
    std::string endpoint_;
    mg::core::mavlink::MavlinkRouter router_;
    mutable mg::core::platform::Mutex peer_mtx_;
    std::map<int, LinkPeer> peers_;
};

} // namespace mg::core::transport
