#include "core/transport/link_factory.h"
#include "core/transport/serial_link.h"
#include "core/transport/tcp_link.h"
#include "core/transport/udp_link.h"
#include "core/transport/in_memory_link.h"
#include "core/transport/serial_port_discovery.h"
#include "core/log/logging.h"

namespace mg::core::transport {

std::unique_ptr<ILink> LinkFactory::create(const LinkConfig& config) {
    LinkDescriptor desc;
    desc.id = config.name.empty() ? config.endpoint : config.name;
    desc.type = config.type_string();
    desc.endpoint = config.endpoint;
    desc.baud_rate = config.baud_rate;

    switch (config.type) {
        case LinkType::Serial:
            return std::make_unique<SerialLink>(desc);
        case LinkType::TCP:
            return std::make_unique<TcpLink>(desc);
        case LinkType::UDP:
            return std::make_unique<UdpLink>(desc);
        case LinkType::InMemory:
            return std::make_unique<InMemoryLink>();
    }

    MGC_LOG_ERROR("[LinkFactory] Unknown link type for endpoint: %s", config.endpoint.c_str());
    return nullptr;
}

std::vector<DiscoveredDevice> LinkFactory::discover(LinkType type) {
    std::vector<DiscoveredDevice> devices;

    if (type == LinkType::Serial || type == LinkType::InMemory) {
        SerialPortDiscovery discovery;
        auto ports = discovery.enumerate_ports();
        for (auto& p : ports) {
            DiscoveredDevice dev;
            dev.endpoint = p.port_name;
            dev.description = p.description;
            dev.manufacturer = p.manufacturer;
            dev.is_likely_fc = p.is_likely_fc;
            devices.push_back(std::move(dev));
        }
    }

    return devices;
}

std::string LinkFactory::parse_scheme(const std::string& uri) {
    auto pos = uri.find("://");
    if (pos == std::string::npos) return "serial";
    return uri.substr(0, pos);
}

} // namespace mg::core::transport
