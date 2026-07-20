#include "core/transport/link_config.h"
#include <sstream>
#include <algorithm>

namespace mg::core::transport {

LinkConfig LinkConfig::from_uri(const std::string& uri) {
    LinkConfig cfg;
    // Format: "type://endpoint?param=val"
    auto scheme_end = uri.find("://");
    if (scheme_end == std::string::npos) {
        cfg.endpoint = uri;
        cfg.type = LinkType::Serial;
        cfg.name = uri;
        return cfg;
    }

    std::string scheme = uri.substr(0, scheme_end);
    std::string rest = uri.substr(scheme_end + 3);

    // Parse query params
    std::string endpoint_str;
    auto qmark = rest.find('?');
    if (qmark != std::string::npos) {
        endpoint_str = rest.substr(0, qmark);
        std::string params = rest.substr(qmark + 1);
        std::istringstream ss(params);
        std::string token;
        while (std::getline(ss, token, '&')) {
            auto eq = token.find('=');
            if (eq != std::string::npos) {
                std::string key = token.substr(0, eq);
                std::string val = token.substr(eq + 1);
                if (key == "baud") {
                    try { cfg.baud_rate = static_cast<uint32_t>(std::stoi(val)); }
                    catch (...) { /* ignore invalid baud, keep default */ }
                }
                else if (key == "auto") cfg.auto_connect = (val == "true" || val == "1");
            }
        }
    } else {
        endpoint_str = rest;
    }

    // Normalize scheme to lowercase
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);

    if (scheme == "serial" || scheme == "com" || scheme == "usb") {
        cfg.type = LinkType::Serial;
    } else if (scheme == "tcp") {
        cfg.type = LinkType::TCP;
    } else if (scheme == "udp") {
        cfg.type = LinkType::UDP;
    } else if (scheme == "mem" || scheme == "memory") {
        cfg.type = LinkType::InMemory;
    } else {
        cfg.type = LinkType::Serial;
    }

    cfg.endpoint = endpoint_str;
    cfg.name = endpoint_str;
    return cfg;
}

std::string LinkConfig::to_uri() const {
    std::ostringstream ss;
    switch (type) {
        case LinkType::Serial:    ss << "serial://"; break;
        case LinkType::TCP:       ss << "tcp://"; break;
        case LinkType::UDP:       ss << "udp://"; break;
        case LinkType::InMemory:  ss << "memory://"; break;
    }
    ss << endpoint;
    if (type == LinkType::Serial && baud_rate != 115200) {
        ss << "?baud=" << baud_rate;
    }
    if (auto_connect) {
        ss << (type == LinkType::Serial && baud_rate != 115200 ? "&" : "?") << "auto=true";
    }
    return ss.str();
}

std::string LinkConfig::type_string() const {
    switch (type) {
        case LinkType::Serial:    return "serial";
        case LinkType::TCP:       return "tcp";
        case LinkType::UDP:       return "udp";
        case LinkType::InMemory:  return "memory";
    }
    return "unknown";
}

} // namespace mg::core::transport
