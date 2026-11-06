#include "core/mavlink/mavlink_router.h"

#include "core/log/logging.h"
#include "core/platform/platform_mutex.h"

#include "ardupilotmega/mavlink.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace mg::core::mavlink {

namespace {

constexpr uint16_t route_key(uint8_t system_id, uint8_t component_id) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(system_id) << 8) | component_id);
}

HeartbeatInfo heartbeat_info(const mavlink_message_t& message,
                             const mavlink_heartbeat_t& heartbeat) {
    HeartbeatInfo info;
    info.system_id = message.sysid;
    info.component_id = message.compid;
    info.autopilot_type = heartbeat.autopilot;
    info.vehicle_type = heartbeat.type;
    info.base_mode = heartbeat.base_mode;
    info.custom_mode = heartbeat.custom_mode;
    info.system_status = heartbeat.system_status;
    info.armed = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
    return info;
}

} // namespace

struct MavlinkRouter::Impl {
    struct Target {
        int route_id = -1;
        uint8_t system_id = 0;
        uint8_t component_id = 0;
        std::shared_ptr<MavlinkAdapter> adapter;
    };

    mutable mg::core::platform::Mutex mtx;
    std::map<int, Target> by_id;
    std::map<uint16_t, int> by_identity;
    DiscoveryCallback discovery;

    // One parser state per physical link. MavlinkAdapter instances keep their
    // own parser state for the frames they receive after routing.
    mavlink_message_t rx_message{};
    mavlink_status_t rx_status{};
};

MavlinkRouter::MavlinkRouter()
    : impl_(std::make_unique<Impl>()) {}

MavlinkRouter::~MavlinkRouter() = default;

bool MavlinkRouter::add_target(int route_id,
                               uint8_t system_id,
                               uint8_t component_id,
                               std::shared_ptr<MavlinkAdapter> adapter) {
    if (route_id <= 0 || system_id == 0 || component_id == 0 || !adapter) {
        return false;
    }

    const uint16_t identity = route_key(system_id, component_id);
    mg::core::platform::LockGuard lock(impl_->mtx);
    if (const auto existing = impl_->by_identity.find(identity);
        existing != impl_->by_identity.end() && existing->second != route_id) {
        return false;
    }

    if (const auto existing = impl_->by_id.find(route_id);
        existing != impl_->by_id.end()) {
        impl_->by_identity.erase(
            route_key(existing->second.system_id, existing->second.component_id));
    }

    Impl::Target target;
    target.route_id = route_id;
    target.system_id = system_id;
    target.component_id = component_id;
    target.adapter = std::move(adapter);
    impl_->by_identity[identity] = route_id;
    impl_->by_id[route_id] = std::move(target);
    return true;
}

bool MavlinkRouter::update_target_identity(int route_id,
                                           uint8_t system_id,
                                           uint8_t component_id) {
    if (route_id <= 0 || system_id == 0 || component_id == 0) return false;

    const uint16_t identity = route_key(system_id, component_id);
    mg::core::platform::LockGuard lock(impl_->mtx);
    const auto target_it = impl_->by_id.find(route_id);
    if (target_it == impl_->by_id.end()) return false;

    if (const auto existing = impl_->by_identity.find(identity);
        existing != impl_->by_identity.end() && existing->second != route_id) {
        return false;
    }

    impl_->by_identity.erase(route_key(target_it->second.system_id,
                                       target_it->second.component_id));
    target_it->second.system_id = system_id;
    target_it->second.component_id = component_id;
    impl_->by_identity[identity] = route_id;
    return true;
}

void MavlinkRouter::remove_target(int route_id) {
    mg::core::platform::LockGuard lock(impl_->mtx);
    const auto it = impl_->by_id.find(route_id);
    if (it == impl_->by_id.end()) return;
    impl_->by_identity.erase(route_key(it->second.system_id,
                                       it->second.component_id));
    impl_->by_id.erase(it);
}

void MavlinkRouter::set_discovery_callback(DiscoveryCallback callback) {
    mg::core::platform::LockGuard lock(impl_->mtx);
    impl_->discovery = std::move(callback);
}

std::vector<int> MavlinkRouter::route_bytes(const uint8_t* data,
                                            size_t size,
                                            const MavlinkPeer* sender) {
    if (!data || size == 0) return {};

    std::vector<mavlink_message_t> frames;
    std::vector<int> routed;
    {
        mg::core::platform::LockGuard lock(impl_->mtx);
        mavlink_message_t decoded{};
        mavlink_status_t status{};
        for (size_t i = 0; i < size; ++i) {
            const auto framing = mavlink_frame_char_buffer(
                &impl_->rx_message, &impl_->rx_status, data[i], &decoded, &status);
            if (framing == MAVLINK_FRAMING_OK) {
                frames.push_back(decoded);
            } else if (framing == MAVLINK_FRAMING_BAD_CRC ||
                       framing == MAVLINK_FRAMING_BAD_SIGNATURE) {
                impl_->rx_status.parse_state = MAVLINK_PARSE_STATE_IDLE;
                if (data[i] == MAVLINK_STX) {
                    impl_->rx_status.parse_state = MAVLINK_PARSE_STATE_GOT_STX;
                    impl_->rx_message.len = 0;
                    mavlink_start_checksum(&impl_->rx_message);
                }
            }
        }
    }

    for (const auto& message : frames) {
        if (message.sysid == 0 || message.compid == 0) continue;

        std::shared_ptr<MavlinkAdapter> adapter;
        int route_id = -1;
        const uint16_t identity = route_key(message.sysid, message.compid);
        {
            mg::core::platform::LockGuard lock(impl_->mtx);
            if (const auto it = impl_->by_identity.find(identity);
                it != impl_->by_identity.end()) {
                if (const auto target = impl_->by_id.find(it->second);
                    target != impl_->by_id.end()) {
                    route_id = target->second.route_id;
                    adapter = target->second.adapter;
                }
            }
            // Vehicle peripherals (camera, gimbal, storage) use their own
            // component IDs but share the vehicle system ID. They must reach
            // the vehicle adapter even though only AUTOPILOT1 is registered
            // as the primary route.
            if (!adapter) {
                for (const auto& [candidate_id, candidate] : impl_->by_id) {
                    if (candidate.system_id != message.sysid || !candidate.adapter) {
                        continue;
                    }
                    route_id = candidate_id;
                    adapter = candidate.adapter;
                    break;
                }
            }
        }

        bool is_heartbeat = message.msgid == MAVLINK_MSG_ID_HEARTBEAT;
        mavlink_heartbeat_t heartbeat{};
        if (is_heartbeat) {
            mavlink_msg_heartbeat_decode(&message, &heartbeat);
        }

        if (!adapter && is_heartbeat) {
            DiscoveryCallback callback;
            {
                mg::core::platform::LockGuard lock(impl_->mtx);
                callback = impl_->discovery;
            }
            if (callback) {
                const MavlinkPeer empty_peer;
                const auto discovered = callback(
                    heartbeat_info(message, heartbeat),
                    sender ? *sender : empty_peer);
                if (discovered && discovered->route_id > 0 &&
                    discovered->adapter) {
                    // 注册失败意味着该目标的报文不会再有归属；静默丢弃会让
                    // “发现了飞机但一直没有数据”变得无法诊断（KD-06）。
                    if (!add_target(discovered->route_id,
                                    message.sysid,
                                    message.compid,
                                    discovered->adapter)) {
                        MGC_LOG_ERROR(
                            "[MavlinkRouter] Failed to register discovered target "
                            "route=%d sysid=%u compid=%u",
                            discovered->route_id,
                            static_cast<unsigned>(message.sysid),
                            static_cast<unsigned>(message.compid));
                    }
                }
            }

            // A lifecycle callback may bind an existing placeholder instead
            // of returning a new adapter. Re-resolve after the callback.
            if (!adapter) {
                mg::core::platform::LockGuard lock(impl_->mtx);
                if (const auto it = impl_->by_identity.find(identity);
                    it != impl_->by_identity.end()) {
                    if (const auto target = impl_->by_id.find(it->second);
                        target != impl_->by_id.end()) {
                        route_id = target->second.route_id;
                        adapter = target->second.adapter;
                    }
                }
            }
        }

        if (!adapter) continue;

        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
        if (length == 0) continue;
        if (adapter->feed_raw(buffer, length)) {
            routed.push_back(route_id);
        }
    }
    return routed;
}

} // namespace mg::core::mavlink
