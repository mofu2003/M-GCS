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
#include "core/log/logging.h"
#include <chrono>

namespace mg::core::transport {

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

    watchdog_thread_ = std::thread([this]() {
        int wait_count = 0;
        while (watchdog_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (!watchdog_running_.load()) break;

            // 已经收到过心跳时，进入超时检查。
            if (last_heartbeat_ms_.load() > 0) {
                if (!heartbeat_alive(5000)) {
                    transition_to(ConnectionState::HeartbeatLost, "heartbeat timeout (5s)");
                    // A disconnected GCS session must return to a clean state. The UI
                    // presents an explicit reconnect action instead of silently reusing
                    // a stale transport.
                    reset();
                    break;
                }
            }
            // 首包心跳等待超过 10 秒时，判定连接失败。
            else if (state_.load() == ConnectionState::WaitingHeartbeat) {
                if (++wait_count >= 10) {
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
    if (watchdog_thread_.joinable()) {
        if (watchdog_thread_.get_id() == std::this_thread::get_id()) {
            // 如果在看门狗线程自身内部调用 stop_watchdog()，
            // 这里不能 self-join，只能 detach。
            // watchdog_running_ 已经置为 false，线程会在下一轮自然退出。
            watchdog_thread_.detach();
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
            SerialPortInfo discovered;
            discovered.port_name = endpoint;
            SerialPortDiscovery discovery;
            const auto ports = discovery.enumerate_ports();
            for (const auto& candidate : ports) {
                if (candidate.port_name == endpoint) {
                    discovered = candidate;
                    break;
                }
            }
            port = discovered;
            link = std::make_unique<SerialLink>(desc);
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
        link_ptr = std::move(link);
        mavlink_ptr = std::move(mavlink);
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
    {
        mg::core::platform::LockGuard lk(link_mtx_);  // 加锁保护链路资源
        if (link_ptr) link_ptr->close();
        link_ptr.reset();
        mavlink_ptr.reset();
    }
    last_heartbeat_ms_ = 0;
    transition_to(ConnectionState::Disconnected, "reset");
}

// 连接传输层和协议层的回调：传输层接收数据 -> 协议层解析，协议层发送 -> 传输层写出
void ConnectionManager::wire_callbacks() {
    // 调用者必须持有 link_mtx_
    if (!link_ptr || !mavlink_ptr) return;

    link_ptr->set_receive_callback(
        [mav = mavlink_ptr.get()](const uint8_t* data, size_t size) {
            mav->feed_raw(data, size);
        });

    mavlink_ptr->set_send_callback(
        [lk = link_ptr.get()](const uint8_t* data, size_t size) -> bool {
            return lk->send(data, size);
        });
}

} // namespace mg::core::transport
