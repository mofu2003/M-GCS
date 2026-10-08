/**
 * connection_manager.h - connection lifecycle manager.
 *
 * Owns the transport + protocol layer composition (QGC LinkManager pattern).
 * Three-layer pattern:
 *   Transport layer: SerialLink (raw bytes)
 *   Protocol layer:  MavlinkAdapter (MAVLink parsing)
 *   Lifecycle layer: ConnectionManager (connect/disconnect/watchdog state)
 *
 * State machine:
 *   Disconnected -> Connecting -> WaitingHeartbeat -> Connected
 *                                          |
 *                                          v
 *                                  HeartbeatLost -> Disconnected
 *
 * This is a real seam: two adapters exist (real SerialLink, InMemoryLink for tests).
 * Access via get_mavlink() / get_link() methods, not direct field access.
 */

#ifndef MGCORE_CONNECTION_MANAGER_H
#define MGCORE_CONNECTION_MANAGER_H

#include "core/transport/serial_link.h"
#include "core/transport/serial_port_discovery.h"
#include "core/transport/link_config.h"
#include "core/transport/link_session.h"
#include "core/platform/platform_mutex.h"
#include "core/mavlink/mavlink_adapter.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace mg::core::transport {

/// Connection lifecycle state.
enum class ConnectionState {
    Disconnected,       // No active connection
    Connecting,         // Serial port opening, not yet wired
    WaitingHeartbeat,   // Connected, waiting for first heartbeat
    Connected,          // Heartbeat received, fully operational
    HeartbeatLost,      // Heartbeat timeout detected
    Disconnecting       // Cleanup in progress
};

/// Connection state transition callback.
using ConnectionStateChangedCallback = std::function<void(ConnectionState new_state, const char* reason)>;

// Composes the transport and protocol layers and owns connection state.
class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    // Non-copyable
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // Access the active MAVLink adapter. May be null before a link is wired.
    mg::core::mavlink::MavlinkAdapter* get_mavlink() { return mavlink_ptr.get(); }
    const mg::core::mavlink::MavlinkAdapter* get_mavlink() const { return mavlink_ptr.get(); }
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> shared_mavlink() const;
    // Access the active transport link. May be null before connection.
    mg::core::transport::ILink* get_link() { return link_ptr.get(); }

    /// Connect through a serial port and start heartbeat supervision.
    bool connect(const std::string& port, uint32_t baud);

    /// Connect through TCP. Endpoint format: "host:port".
    bool connect_tcp(const std::string& endpoint);

    /// Connect through UDP. Endpoint format: "host:port".
    bool connect_udp(const std::string& endpoint);

    /// Unified connection entry point for serial, TCP, or UDP links.
    bool connect(LinkType type, const std::string& endpoint, uint32_t baud = 0);

    /// Attach this lifecycle manager to a shared LinkSession. The optional
    /// identity may be zero for a placeholder Vehicle until its first heartbeat.
    bool attach_shared(const std::shared_ptr<LinkSession>& session,
                       int route_id,
                       uint8_t system_id = 0,
                       uint8_t component_id = 0);

    /// Bind/rebind the adapter already attached to a shared session.
    bool bind_shared_identity(uint8_t system_id, uint8_t component_id);

    bool is_shared() const;
    std::shared_ptr<LinkSession> shared_session() const;

    /// Disconnect and release transport/protocol resources.
    void disconnect();

    /// Send one GCS heartbeat without holding link_mtx_ during the write.
    /// The shared adapter reference keeps the callback target alive if a
    /// concurrent reset tears down the link.
    bool send_heartbeat();

    /// Whether the manager is fully connected and usable.
    bool is_connected() const {
        return state_ == ConnectionState::Connected;
    }

    /// Current connection state.
    ConnectionState state() const { return state_.load(); }

    /// Human-readable state name.
    static const char* state_string(ConnectionState s);

    /// Current serial port information.
    const SerialPortInfo& port_info() const { return port; }

    /// Set the state transition callback used by the FFI event layer.
    void set_state_callback(ConnectionStateChangedCallback cb);

    /// Milliseconds since the last heartbeat. Returns 0 if none was received.
    uint64_t heartbeat_age_ms() const;

    /// Whether heartbeat age is inside the timeout window.
    bool heartbeat_alive(uint64_t timeout_ms = 5000) const;

    /// Record one received heartbeat.
    void record_heartbeat() {
        const auto now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        last_heartbeat_ms_ = now;
        last_activity_ms_ = now;
        ConnectionState cur = state_.load();
        if (cur == ConnectionState::WaitingHeartbeat ||
            cur == ConnectionState::HeartbeatLost) {
            transition_to(ConnectionState::Connected, "heartbeat received");
        }
    }

    /// Reset link/protocol resources and return to disconnected state.
    void reset();

    /// Test-only wiring hook for injecting a link and protocol adapter.
    void wire_for_test(std::unique_ptr<ILink> link,
                       std::unique_ptr<mg::core::mavlink::MavlinkAdapter> mav) {
        {
            mg::core::platform::LockGuard lk(link_mtx_);
            link_ptr = std::shared_ptr<ILink>(std::move(link));
            mavlink_ptr = std::shared_ptr<mg::core::mavlink::MavlinkAdapter>(std::move(mav));
            wire_callbacks();
            mavlink_ptr->on_heartbeat([this](const mg::core::mavlink::HeartbeatInfo&) {
                record_heartbeat();
            });
        }
        transition_to(ConnectionState::WaitingHeartbeat, "wire_for_test");
    }

private:
    // Transport and protocol layers owned by the connection manager.
    mutable mg::core::platform::Mutex link_mtx_;
    std::shared_ptr<ILink>            link_ptr;
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mavlink_ptr;
    std::shared_ptr<LinkSession>      shared_session_;
    int                               shared_route_id_ = -1;
    SerialPortInfo                    port;

    // Wire transport callbacks into the protocol layer.
    void wire_callbacks();

    // State machine.
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    mutable mg::core::platform::Mutex state_cb_mtx_;  // protects state callback
    ConnectionStateChangedCallback state_cb_;

    // Heartbeat watchdog.
    std::thread watchdog_thread_;
    mutable mg::core::platform::Mutex watchdog_mtx_;
    mg::core::platform::ConditionVariable watchdog_cv_;
    std::atomic<bool> watchdog_running_{false};
    std::atomic<uint64_t> last_heartbeat_ms_{0};
    std::atomic<uint64_t> last_activity_ms_{0};
    // Start heartbeat watchdog.
    void start_watchdog();
    // Stop heartbeat watchdog.
    void stop_watchdog();
    /// Keep an established connection alive when any MAVLink traffic arrives.
    void record_activity();
    // Apply state transition and fire callback.
    void transition_to(ConnectionState new_state, const char* reason = "");
};

} // namespace mg::core::transport

#endif /* MGCORE_CONNECTION_MANAGER_H */
