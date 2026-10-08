#include "core/transport/link_session.h"

#include <utility>

namespace mg::core::transport {

LinkSession::LinkSession(std::shared_ptr<ILink> link, std::string endpoint)
    : link_(std::move(link)), endpoint_(std::move(endpoint)) {}

LinkSession::~LinkSession() {
    close();
}

bool LinkSession::open() {
    if (!link_) return false;
    const bool was_open = link_->is_open();
    if (!was_open && !link_->open()) return false;

    if (link_->supports_peer_routing()) {
        link_->set_peer_receive_callback(
            [this](const LinkPeer& peer, const uint8_t* data, size_t size) {
                on_peer_bytes(peer, data, size);
            });
    } else {
        link_->set_receive_callback(
            [this](const uint8_t* data, size_t size) {
                on_bytes(data, size);
            });
    }
    return true;
}

void LinkSession::close() {
    if (link_) {
        if (link_->supports_peer_routing()) {
            link_->set_peer_receive_callback(nullptr);
        } else {
            link_->set_receive_callback(nullptr);
        }
        link_->close();
    }
    {
        mg::core::platform::LockGuard lock(peer_mtx_);
        peers_.clear();
    }
    router_.set_discovery_callback(nullptr);
}

bool LinkSession::is_open() const {
    return link_ && link_->is_open();
}

bool LinkSession::add_vehicle(
    int route_id,
    uint8_t system_id,
    uint8_t component_id,
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> adapter) {
    if (!router_.add_target(route_id, system_id, component_id, adapter)) {
        return false;
    }

    // When the session is owned by a shared_ptr, replies from this adapter are
    // routed back to the peer learned for its route. A stack-only session can
    // still be used for routing, but the caller wires sends explicitly.
    const std::weak_ptr<LinkSession> weak = weak_from_this();
    if (!weak.expired()) {
        adapter->set_send_callback(
            [weak, route_id](const uint8_t* data, size_t size) -> bool {
                const auto self = weak.lock();
                return self && self->send(route_id, data, size);
            });
    }
    return true;
}

bool LinkSession::update_vehicle_identity(int route_id,
                                          uint8_t system_id,
                                          uint8_t component_id) {
    return router_.update_target_identity(route_id, system_id, component_id);
}

void LinkSession::remove_vehicle(int route_id) {
    router_.remove_target(route_id);
    {
        mg::core::platform::LockGuard lock(peer_mtx_);
        peers_.erase(route_id);
    }
}

bool LinkSession::send(int route_id, const uint8_t* data, size_t size) {
    if (!link_ || !data || size == 0 || !link_->is_open()) return false;

    const LinkPeer peer = peer_for(route_id);
    if (link_->supports_peer_routing() && peer.valid()) {
        return link_->send_to(peer, data, size);
    }
    return link_->send(data, size);
}

void LinkSession::set_discovery_callback(
    mg::core::mavlink::MavlinkRouter::DiscoveryCallback callback) {
    router_.set_discovery_callback(std::move(callback));
}

void LinkSession::on_bytes(const uint8_t* data, size_t size) {
    (void)router_.route_bytes(data, size);
}

void LinkSession::on_peer_bytes(const LinkPeer& peer,
                                const uint8_t* data,
                                size_t size) {
    const mg::core::mavlink::MavlinkPeer sender{peer.host, peer.port};
    const auto routed = router_.route_bytes(data, size, &sender);
    for (const int route_id : routed) set_peer_for_route(route_id, peer);
}

void LinkSession::set_peer_for_route(int route_id, const LinkPeer& peer) {
    if (route_id <= 0 || !peer.valid()) return;
    mg::core::platform::LockGuard lock(peer_mtx_);
    peers_[route_id] = peer;
}

LinkPeer LinkSession::peer_for(int route_id) const {
    mg::core::platform::LockGuard lock(peer_mtx_);
    const auto it = peers_.find(route_id);
    return it == peers_.end() ? LinkPeer{} : it->second;
}

} // namespace mg::core::transport
