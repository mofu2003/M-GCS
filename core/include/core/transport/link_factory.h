#pragma once

// 链路工厂，根据配置创建不同类型的通信链路实例

#include "core/transport/link_config.h"
#include "core/transport/link_interface.h"
#include <memory>
#include <string>
#include <vector>

namespace mg::core::transport {

// 发现的设备信息，包含端点、描述和是否为飞控的标记
struct DiscoveredDevice {
    std::string endpoint;
    std::string description;
    std::string manufacturer;
    bool is_likely_fc = false;
};

// 链路工厂类，提供创建链路、发现设备和解析 URI 的静态方法
class LinkFactory {
public:
    // 根据配置创建对应的链路实例
    static std::unique_ptr<ILink> create(const LinkConfig& config);

    // 发现指定类型的可用设备列表
    static std::vector<DiscoveredDevice> discover(LinkType type);

};

} // namespace mg::core::transport
