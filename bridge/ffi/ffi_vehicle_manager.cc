#define MGCORE_EXPORTS
// ffi_vehicle_manager.cc - 多机管理：打开/关闭连接、切换活跃飞行器、设备发现。
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"
#include "core/transport/link_factory.h"
#include "core/transport/link_config.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>

namespace {

bool is_supported_connection_uri(const char* endpoint_uri) {
    if (!endpoint_uri || endpoint_uri[0] == '\0') return false;
    const std::string uri(endpoint_uri);
    const auto scheme_end = uri.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0 ||
        scheme_end + 3 >= uri.size()) {
        return false;
    }
    const auto query_start = uri.find('?', scheme_end + 3);
    const auto endpoint_end = query_start == std::string::npos
        ? uri.size()
        : query_start;
    if (endpoint_end == scheme_end + 3) return false;

    std::string scheme = uri.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (scheme != "serial" && scheme != "tcp" && scheme != "udp") {
        return false;
    }

    const std::string endpoint = uri.substr(
        scheme_end + 3,
        endpoint_end - (scheme_end + 3));
    if (endpoint.empty()) return false;

    if (scheme == "serial") {
        if (query_start != std::string::npos) {
            const std::string query = uri.substr(query_start + 1);
            const auto baud_pos = query.find("baud=");
            if (baud_pos != std::string::npos) {
                const auto value_start = baud_pos + 5;
                const auto value_end = query.find('&', value_start);
                const std::string value = query.substr(
                    value_start,
                    value_end == std::string::npos
                        ? std::string::npos
                        : value_end - value_start);
                if (value.empty() || !std::all_of(
                        value.begin(), value.end(),
                        [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                    return false;
                }
                try {
                    const auto baud = std::stoull(value);
                    if (baud == 0 || baud > std::numeric_limits<uint32_t>::max()) {
                        return false;
                    }
                } catch (...) {
                    return false;
                }
            }
        }
        return true;
    }

    const auto separator = endpoint.rfind(':');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 >= endpoint.size()) {
        return false;
    }
    const std::string port_text = endpoint.substr(separator + 1);
    if (!std::all_of(port_text.begin(), port_text.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return false;
    }
    try {
        const auto port = std::stoul(port_text);
        return port > 0 && port <= 65535;
    } catch (...) {
        return false;
    }
}

int open_vehicle_session(mg_core_t* core, const char* endpoint_uri) {
    const auto config = mg::core::transport::LinkConfig::from_uri(endpoint_uri);
    auto* vehicle = core->vehicles.open_vehicle(
        config, [core](int event_type, const std::string& json) {
            core->events.fire(static_cast<mg_event_type_t>(event_type), json);
        });
    if (!vehicle) return MG_ERROR_SEND_FAILED;

    if (core->fallback_vehicle && core->fallback_vehicle->sensor_decoder().is_loaded()) {
        const auto db_path =
            mg::core::platform::find_bundled_data_file("SensorDeviceInfo.json");
        if (!db_path.empty()) vehicle->load_sensor_decoder(db_path);
    }
    return vehicle->vehicle_id();
}

} // namespace

MGCORE_API int mg_connection_open(mg_core_t* core, const char* endpoint_uri) {
    if (!core) {
        mg_set_last_error(MG_ERROR_NULL_CORE);
        return MG_ERROR_NULL_CORE;
    }
    if (!is_supported_connection_uri(endpoint_uri)) {
        mg_set_last_error(MG_ERROR_INVALID_PARAM);
        return MG_ERROR_INVALID_PARAM;
    }

    int result = MG_ERROR_INTERNAL;
    try {
        result = open_vehicle_session(core, endpoint_uri);
    } catch (const std::invalid_argument&) {
        result = MG_ERROR_INVALID_PARAM;
    } catch (const std::out_of_range&) {
        result = MG_ERROR_INVALID_PARAM;
    } catch (...) {
        result = MG_ERROR_INTERNAL;
    }
    if (result < 0) {
        mg_set_last_error(static_cast<mg_error_t>(result));
        return result;
    }
    mg_clear_last_error();
    return result;
}

MGCORE_API char* mg_connection_get_session_state(mg_core_t* core, int session_id) {
    CHECK_CORE(core);
    const auto vehicle = core->vehicles.get_vehicle_shared(session_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "connection session not found");

    auto& connection = vehicle->conn();
    mg::bridge::JsonBuilder result;
    result.begin_object();
    result.field("success", true);
    result.field("session_id", session_id);
    result.field("state", mg::core::transport::ConnectionManager::state_string(
                               connection.state()));
    result.field("connected", connection.is_connected());
    result.field("heartbeat_age_ms",
                 static_cast<int64_t>(connection.heartbeat_age_ms()));
    result.field("heartbeat_alive", connection.heartbeat_alive(5000));
    result.field("active_focus", mg_vehicle_get_active(core) == session_id);
    result.end_object();
    return to_cstr(result.str());
}

MGCORE_API char* mg_connection_close(mg_core_t* core, int session_id) {
    CHECK_CORE(core);
    if (session_id <= 0 || !core->vehicles.close_vehicle(session_id)) {
        return fail(MG_ERROR_NOT_FOUND, "connection session not found");
    }
    return ok();
}

// ================================================================
// Compatibility aliases for the former vehicle lifecycle names.
// ================================================================
MGCORE_API int mg_vehicle_open(mg_core_t* core, const char* endpoint) {
    if (!endpoint || endpoint[0] == '\0') {
        mg_set_last_error(MG_ERROR_INVALID_PARAM);
        return -1;
    }
    std::string uri(endpoint);
    if (uri.find("://") == std::string::npos) uri = "serial://" + uri;
    const int result = mg_connection_open(core, uri.c_str());
    return result < 0 ? -1 : result;
}

// ================================================================
// mg_vehicle_close
// ================================================================
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    return mg_connection_close(core, vehicle_id);
}

// ================================================================
// mg_vehicle_select
// ================================================================
MGCORE_API char* mg_vehicle_select(mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    if (!core->vehicles.get_vehicle(vehicle_id)) {
        return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    }
    core->vehicles.set_active_vehicle(vehicle_id);
    return ok();
}

// ================================================================
// mg_vehicle_get_active
// ================================================================
MGCORE_API int mg_vehicle_get_active(mg_core_t* core) {
    if (!core) return -1;
    auto* active = core->vehicles.active_vehicle();
    return active ? active->vehicle_id() : -1;
}

// ================================================================
// mg_discover_devices
// ================================================================
MGCORE_API char* mg_discover_devices(mg_core_t* core, const char* transport_type) {
    CHECK_CORE(core);

    mg::core::transport::LinkType lt = mg::core::transport::LinkType::Serial;
    if (transport_type) {
        std::string s = transport_type;
        if (s == "tcp") lt = mg::core::transport::LinkType::TCP;
        else if (s == "udp") lt = mg::core::transport::LinkType::UDP;
    }

    auto devices = core->vehicles.discover_devices(lt);
    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (auto& d : devices) {
        mg::bridge::JsonBuilder obj;
        obj.begin_object();
        obj.field("endpoint", d.endpoint);
        obj.field("description", d.description);
        obj.field("manufacturer", d.manufacturer);
        obj.field("is_likely_fc", d.is_likely_fc);
        obj.end_object();
        jb.append_json(obj.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}
