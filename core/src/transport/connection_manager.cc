/**
 * connection_manager.cc - 连接生命周期管理器实现
 *
 * 采用三层组合：
 *   传输层 -> 协议层 -> 生命周期状态机
 *
 * 回调接线方式：
 *   transport.receive -> protocol.feed_raw
 *   protocol.send     -> transport.send
 *
 * v1.2：状态机与心跳看门狗从 bridge 层下沉到后端。
 * v1.3：修复看门狗线程自 join 死锁与静态 wait_count 泄漏。
 * v1.4：补强线程安全；state_cb_mtx_ 保护状态回调，
 *       link_mtx_ 保护 link_ptr / mavlink_ptr。
 */

#include "core/transport/connection_manager.h"
#include "core/transport/tcp_link.h"
#include "core/transport/udp_link.h"
#ifdef __ANDROID__
#include "core/transport/android_usb_serial_link.h"
#endif
#include "core/log/logging.h"
#include <chrono>

namespace mg::core::transport {

namespace {
// A flight-control link should become visibly disconnected quickly enough for
// the operator to react, while allowing normal short serial/UDP jitter.
constexpr auto kEstablishedTrafficTimeout = std::chrono::seconds(5);
constexpr int kWatchdogPeriodSeconds = 1;
constexpr int kInitialHeartbeatTimeoutSeconds = 10;
}

ConnectionManager::ConnectionManager() = default;

ConnectionManager::~ConnectionManager() {
    reset();
}

// ================================================================
// 状态机
// ================================================================

// 将状态枚举转为可读字符串
const char* ConnectionManager::state_string(ConnectionState s) {
    switch (s) {
        case ConnectionState::Disconnected:     return "disconnected";
        case ConnectionState::Connecting:       return "connecting";
        case ConnectionState::WaitingHeartbeat: return "waiting_heartbeat";
        case ConnectionState::Connected:        return "connected";
        case ConnectionState::HeartbeatLost:    return "heartbeat_lost";
        case ConnectionState::Disconnecting:    return "disconnecting";
    }
    return "unknown";
}

// 执行状态转换，若状态发生变化则触发回调
void ConnectionManager::transition_to(ConnectionState new_state, const char* reason) {
    ConnectionState old = state_.exchange(new_state);
    if (old != new_state) {
        MGC_LOG_INFO("[ConnMgr] State: %s -> %s (%s)",
                     state_string(old), state_string(new_state), reason);
        // 在锁内复制回调，在锁外执行，避免回调反入造成死锁。
        ConnectionStateChangedCallback cb_copy;
        {
            mg::core::platform::LockGuard lk(state_cb_mtx_);
            cb_copy = state_cb_;
        }
        if (cb_copy) cb_copy(new_state, reason);
    }
}

// 设置状态变化回调，加锁保护
void ConnectionManager::set_state_callback(ConnectionStateChangedCallback cb) {
    mg::core::platform::LockGuard lk(state_cb_mtx_);
    state_cb_ = std::move(cb);
}

// ================================================================
// 心跳看门狗
// ================================================================

// 启动心跳看门狗线程，周期性检查心跳是否超时
void ConnectionManager::start_watchdog() {
    stop_watchdog();
    watchdog_running_ = true;
    last_heartbeat_ms_ = 0;
    last_activity_ms_ = 0;

    watchdog_thread_ = std::thread([this]() {
        int wait_count = 0;
        while (watchdog_running_.load()) {
            // Wake immediately when disconnect/reset stops the watchdog.
            {
                mg::core::platform::UniqueLock lk(watchdog_mtx_);
                watchdog_cv_.wait_for(lk, std::chrono::seconds(kWatchdogPeriodSeconds), [this] {
                    return !watchdog_running_.load();
                });
            }
            if (!watchdog_running_.load()) break;

            // 首次心跳仍是连接身份确认的必要条件。确认后，只要仍有有效
            // MAVLink 数据流就不应因为心跳短暂抖动断开，例如参数全量同步
            // 期间串口拥塞时仍会持续收到 PARAM_VALUE/遥测数据。
            const auto current_state = state_.load();
            bool link_open = true;
            {
                mg::core::platform::LockGuard lk(link_mtx_);
                link_open = link_ptr && link_ptr->is_open();
            }

            if (!link_open &&
                (current_state == ConnectionState::Connected ||
                 current_state == ConnectionState::WaitingHeartbeat)) {
                transition_to(ConnectionState::HeartbeatLost, "transport closed");
                reset();
                break;
            }

            // Run protocol-level timers even when the link is silent. Mission,
            // Rally, fence and parameter retries must not depend on receiving
            // another MAVLink message before they can time out.
            if (auto mavlink = shared_mavlink()) {
                mavlink->tick();
            }

            if (current_state == ConnectionState::Connected) {
                const uint64_t activity = last_activity_ms_.load();
                const uint64_t now = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                const auto timeout_ms = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        kEstablishedTrafficTimeout).count());
                if (activity == 0 || now - activity > timeout_ms) {
                    transition_to(ConnectionState::HeartbeatLost, "no valid MAVLink traffic (5s)");
                    // A disconnected GCS session must return to a clean state. The UI
                    // presents an explicit reconnect action instead of silently reusing
                    // a stale transport.
                    reset();
                    break;
                }
            }
            // 首包心跳等待超过 10 秒时，判定连接失败。
            else if (current_state == ConnectionState::WaitingHeartbeat) {
                if (++wait_count >= kInitialHeartbeatTimeoutSeconds) {
                    transition_to(ConnectionState::HeartbeatLost, "no heartbeat received (10s)");
                    // No heartbeat was ever received, so automatic reconnect has no
                    // established vehicle session to preserve. Return to a clean state.
                    reset();
                    break;
                }
            }
        }
    });
}

// 停止看门狗线程，处理自等待死锁的特殊情况
void ConnectionManager::stop_watchdog() {
    watchdog_running_ = false;
    watchdog_cv_.notify_all();
    if (watchdog_thread_.joinable()) {
        if (watchdog_thread_.get_id() == std::this_thread::get_id()) {
            // The watchdog is resetting the connection from its own thread.
            // Leave the std::thread joinable so a concurrent disconnect or
            // the destructor can join it after this turn finishes. Detaching
            // here would let the ConnectionManager be destroyed while the
            // watchdog still owns a raw `this` pointer.
        } else {
            watchdog_thread_.join();
        }
    }
}

// 计算距上次心跳的毫秒数
uint64_t ConnectionManager::heartbeat_age_ms() const {
    uint64_t ts = last_heartbeat_ms_.load();
    if (ts == 0) return 0;
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (now <= ts) return 0;
    return now - ts;
}

// 检查心跳是否在超时范围内
bool ConnectionManager::heartbeat_alive(uint64_t timeout_ms) const {
    uint64_t age = heartbeat_age_ms();
    return age > 0 && age <= timeout_ms;
}

void ConnectionManager::record_activity() {
    last_activity_ms_ = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

// ================================================================
// 连接
// ================================================================

// 通过串口连接（委托给统一 connect 方法）
bool ConnectionManager::connect(const std::string& port_name, uint32_t baud) {
    return connect(LinkType::Serial, port_name, baud);
}

// 通过 TCP 连接
bool ConnectionManager::connect_tcp(const std::string& endpoint) {
    return connect(LinkType::TCP, endpoint, 0);
}

// 通过 UDP 连接
bool ConnectionManager::connect_udp(const std::string& endpoint) {
    return connect(LinkType::UDP, endpoint, 0);
}

bool ConnectionManager::attach_shared(
    const std::shared_ptr<LinkSession>& session,
    int route_id,
    uint8_t system_id,
    uint8_t component_id) {
    if (!session || route_id <= 0) return false;

    reset();

    auto mavlink = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    const std::weak_ptr<LinkSession> weak_session = session;
    mavlink->set_send_callback(
        [weak_session, route_id](const uint8_t* data, size_t size) -> bool {
            const auto locked = weak_session.lock();
            return locked && locked->send(route_id, data, size);
        });

    {
        mg::core::platform::LockGuard lk(link_mtx_);
        shared_session_ = session;
        shared_route_id_ = route_id;
        link_ptr = session->shared_link();
        mavlink_ptr = mavlink;
    }

    mavlink->on_heartbeat([this](const mg::core::mavlink::HeartbeatInfo&) {
        record_heartbeat();
    });
    transition_to(ConnectionState::WaitingHeartbeat, "shared link wired");
    start_watchdog();

    if (system_id > 0 && component_id > 0 &&
        !session->add_vehicle(route_id, system_id, component_id, mavlink)) {
        reset();
        return false;
    }
    return true;
}

bool ConnectionManager::bind_shared_identity(uint8_t system_id,
                                             uint8_t component_id) {
    if (system_id == 0 || component_id == 0) return false;
    std::shared_ptr<LinkSession> session;
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mavlink;
    int route_id = -1;
    {
        mg::core::platform::LockGuard lk(link_mtx_);
        session = shared_session_;
        mavlink = mavlink_ptr;
        route_id = shared_route_id_;
    }
    return session && mavlink && route_id > 0 &&
        session->add_vehicle(route_id, system_id, component_id, mavlink);
}

std::shared_ptr<mg::core::mavlink::MavlinkAdapter>
ConnectionManager::shared_mavlink() const {
    mg::core::platform::LockGuard lk(link_mtx_);
    return mavlink_ptr;
}

bool ConnectionManager::is_shared() const {
    mg::core::platform::LockGuard lk(link_mtx_);
    return shared_session_ != nullptr;
}

std::shared_ptr<LinkSession> ConnectionManager::shared_session() const {
    mg::core::platform::LockGuard lk(link_mtx_);
    return shared_session_;
}

// 统一连接入口：根据类型创建传输层、协议层并接线
bool ConnectionManager::connect(LinkType type, const std::string& endpoint, uint32_t baud) {
    reset();

    transition_to(ConnectionState::Connecting, endpoint.c_str());

    // 根据链路类型创建对应的传输层实例
    std::unique_ptr<ILink> link;
    switch (type) {
        case LinkType::Serial: {
            LinkDescriptor desc;
            desc.id = endpoint;
            desc.type = "serial";
            desc.endpoint = endpoint;
            desc.baud_rate = baud;
#ifdef __ANDROID__
            link = std::make_unique<AndroidUsbSerialLink>(desc);
#else
            link = std::make_unique<SerialLink>(desc);
#endif
            break;
        }
        case LinkType::TCP: {
            LinkDescriptor desc;
            desc.id = "tcp://" + endpoint;
            desc.type = "tcp";
            desc.endpoint = endpoint;
            link = std::make_unique<mg::core::transport::TcpLink>(desc);
            break;
        }
        case LinkType::UDP: {
            LinkDescriptor desc;
            desc.id = "udp://" + endpoint;
            desc.type = "udp";
            desc.endpoint = endpoint;
            link = std::make_unique<mg::core::transport::UdpLink>(desc);
            break;
        }
        default:
            MGC_LOG_ERROR("[ConnMgr] Unsupported link type for connect");
            transition_to(ConnectionState::Disconnected, "unsupported type");
            return false;
    }

    if (!link->open()) {
        MGC_LOG_ERROR("[ConnMgr] Failed to open %s to %s",
                       (type == LinkType::Serial ? "serial" : (type == LinkType::TCP ? "tcp" : "udp")),
                       endpoint.c_str());
        transition_to(ConnectionState::Disconnected, "open failed");
        return false;
    }

    auto mavlink = std::make_unique<mg::core::mavlink::MavlinkAdapter>();

    {
        mg::core::platform::LockGuard lk(link_mtx_);  // 加锁保护链路和协议层
        link_ptr = std::shared_ptr<ILink>(std::move(link));
        mavlink_ptr = std::shared_ptr<mg::core::mavlink::MavlinkAdapter>(
            std::move(mavlink));
        wire_callbacks();
        mavlink_ptr->on_heartbeat([this](const mg::core::mavlink::HeartbeatInfo&) {
            record_heartbeat();
        });
    }

    const char* type_str = (type == LinkType::Serial ? "Serial" : (type == LinkType::TCP ? "TCP" : "UDP"));
    transition_to(ConnectionState::WaitingHeartbeat, "wired, waiting for heartbeat");
    start_watchdog();

    MGC_LOG_INFO("[ConnMgr] %s connected to %s", type_str, endpoint.c_str());
    return true;
}
// 断开连接（用户请求）
void ConnectionManager::disconnect() {
    transition_to(ConnectionState::Disconnecting, "user requested");
    reset();
}

// 重置连接：关闭链路、释放协议层、停止看门狗
void ConnectionManager::reset() {
    stop_watchdog();

    // Do not hold link_mtx_ while close() waits for the transport read
    // thread. In shared mode the LinkSession owns the transport; detach only
    // this vehicle route and leave the socket open for the remaining ones.
    std::shared_ptr<ILink> link;
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mavlink;
    std::shared_ptr<LinkSession> shared;
    int shared_route = -1;
    {
        mg::core::platform::LockGuard lk(link_mtx_);
        link = std::move(link_ptr);
        mavlink = std::move(mavlink_ptr);
        shared = std::move(shared_session_);
        shared_route = shared_route_id_;
        shared_route_id_ = -1;
    }
    if (shared && shared_route > 0) {
        shared->remove_vehicle(shared_route);
    } else if (link) {
        link->close();
    }
    mavlink.reset();
    last_heartbeat_ms_ = 0;
    last_activity_ms_ = 0;
    transition_to(ConnectionState::Disconnected, "reset");
}

bool ConnectionManager::send_heartbeat() {
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mavlink;
    {
        mg::core::platform::LockGuard lk(link_mtx_);
        mavlink = mavlink_ptr;
    }
    return mavlink && mavlink->send_heartbeat();
}

// 连接传输层和协议层的回调：传输层接收数据 -> 协议层解析，协议层发送 -> 传输层写出
void ConnectionManager::wire_callbacks() {
    // 调用者必须持有 link_mtx_
    if (!link_ptr || !mavlink_ptr) return;

    // 使用 weak_ptr 捕获适配器，避免裸指针 use-after-free（ADR-0007，KD-52）
    std::weak_ptr<mg::core::mavlink::MavlinkAdapter> weak_mav = mavlink_ptr;
    link_ptr->set_receive_callback(
        [this, weak_mav](const uint8_t* data, size_t size) {
            auto mav = weak_mav.lock();
            if (!mav) return;  // 适配器已释放，忽略

            const bool parsed_frame = mav->feed_raw(data, size);
            if (parsed_frame && state_.load() == ConnectionState::Connected) {
                record_activity();
            }
        });

    const std::shared_ptr<ILink> link = link_ptr;
    mavlink_ptr->set_send_callback(
        [link](const uint8_t* data, size_t size) -> bool {
            return link->send(data, size);
        });
}

} // namespace mg::core::transport
