#pragma once

// 通信链路抽象接口，定义所有传输层实现的统一行为

#include "core/core_types.h"

namespace mg::core::transport {

// 链路描述符，包含连接 ID、类型、端点地址和波特率等配置信息
struct LinkDescriptor {
    std::string id;
    std::string type;
    std::string endpoint;
    uint32_t baud_rate = 0;
};

/// A transport peer for links which can address more than one remote.
struct LinkPeer {
    std::string host;
    uint16_t port = 0;

    bool valid() const { return !host.empty() && port > 0; }
};

// 链路接口基类，所有传输层（串口、TCP、UDP、内存）都实现此接口
class ILink {
public:
    virtual ~ILink() = default;

    // 打开链路
    virtual bool open() = 0;
    // 关闭链路
    virtual void close() = 0;
    // 查询链路是否已打开
    virtual bool is_open() const = 0;
    // 发送原始字节数据
    virtual bool send(const uint8_t* data, size_t size) = 0;

    // 数据接收回调类型
    using ReceiveCallback = std::function<void(const uint8_t* data, size_t size)>;
    // peer-aware links use this callback so a shared router can learn which
    // remote sent a datagram. Non-peer links leave it unused.
    using PeerReceiveCallback =
        std::function<void(const LinkPeer& peer, const uint8_t* data, size_t size)>;

    // 设置接收数据的回调
    virtual void set_receive_callback(ReceiveCallback cb) = 0;

    /// Whether this transport preserves sender identity per received payload.
    virtual bool supports_peer_routing() const { return false; }

    /// Set the sender-aware receive callback. Default is a no-op so existing
    /// transports only need to override this when they can preserve peers.
    virtual void set_peer_receive_callback(PeerReceiveCallback cb) {
        (void)cb;
    }

    /// Send to one explicit peer. Non-peer transports fall back to send().
    virtual bool send_to(const LinkPeer& peer,
                         const uint8_t* data,
                         size_t size) {
        (void)peer;
        return send(data, size);
    }
};

} // namespace mg::core::transport
