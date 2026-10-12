#pragma once

// 链路配置类型，定义传输层连接参数

#include <string>
#include <cstdint>

namespace mg::core::transport {

// 链路类型枚举
enum class LinkType {
    Serial,
    TCP,
    UDP,
    InMemory,
};

// 链路配置结构体，包含类型、端点、波特率等连接参数
struct LinkConfig {
    LinkType type = LinkType::Serial;
    std::string name;
    std::string endpoint;
    uint32_t baud_rate = 115200;
    bool auto_connect = false;

    // 从 URI 字符串解析配置
    static LinkConfig from_uri(const std::string& uri);
    // 序列化为 URI 字符串
    std::string to_uri() const;
    // 获取类型名称的字符串表示
    std::string type_string() const;
};

} // namespace mg::core::transport
