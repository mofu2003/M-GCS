#define MGCORE_EXPORTS
// ffi_vehicle_manager.cc - 多机管理：打开/关闭连接、切换活跃飞行器、设备发现。
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"
#include "core/transport/link_factory.h"
#include "core/transport/link_config.h"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <cstring>
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

int open_managed_link(mg_core_t* core, const char* endpoint_uri) {
    const auto config = mg::core::transport::LinkConfig::from_uri(endpoint_uri);
    auto event_cb = [core](int event_type, const std::string& json) {
        core->events.fire(static_cast<mg_event_type_t>(event_type), json);
    };
    const int link_id = core->vehicles.open_shared_link(config, event_cb);
    return link_id > 0 ? link_id : MG_ERROR_SEND_FAILED;
}

int open_legacy_vehicle_session(mg_core_t* core, const char* endpoint_uri) {
    const auto config = mg::core::transport::LinkConfig::from_uri(endpoint_uri);
    auto event_cb = [core](int event_type, const std::string& json) {
        core->events.fire(static_cast<mg_event_type_t>(event_type), json);
    };
    auto* vehicle = core->vehicles.open_vehicle(config, event_cb);
    if (!vehicle) return MG_ERROR_SEND_FAILED;
    return vehicle->vehicle_id();
}

bool decode_signing_key_hex(const char* text, uint8_t key[32]) {
    if (!text || std::strlen(text) != 64) return false;
    auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    for (size_t index = 0; index < 32; ++index) {
        const int high = nibble(text[index * 2]);
        const int low = nibble(text[index * 2 + 1]);
        if (high < 0 || low < 0) return false;
        key[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

const char* signing_status_name(int status) {
    switch (static_cast<mavlink_signing_status_t>(status)) {
        case MAVLINK_SIGNING_STATUS_OK: return "ok";
        case MAVLINK_SIGNING_STATUS_BAD_SIGNATURE: return "bad_signature";
        case MAVLINK_SIGNING_STATUS_NO_STREAMS: return "no_streams";
        case MAVLINK_SIGNING_STATUS_TOO_MANY_STREAMS: return "too_many_streams";
        case MAVLINK_SIGNING_STATUS_OLD_TIMESTAMP: return "old_timestamp";
        case MAVLINK_SIGNING_STATUS_REPLAY: return "replay";
        case MAVLINK_SIGNING_STATUS_NONE:
        default: return "none";
    }
}

} // namespace

MGCORE_API char* mg_vehicle_set_signing_key(mg_core_t* core,
                                            int vehicle_id,
                                            const char* key_hex,
                                            int sign_outgoing,
                                            uint64_t persisted_timestamp) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    uint8_t key[32]{};
    if (!decode_signing_key_hex(key_hex, key)) {
        return fail(MG_ERROR_INVALID_PARAM,
                    "signing key must be 64 hexadecimal characters");
    }
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    if (!vehicle->set_signing_key(key, sizeof(key), sign_outgoing != 0,
                                  persisted_timestamp)) {
        return fail(MG_ERROR_NOT_CONNECTED,
                    "vehicle has no MAVLink route for signing");
    }
    return ok();
}

MGCORE_API char* mg_vehicle_set_signing_candidates(
    mg_core_t* core, int vehicle_id, const char* candidates_json) {
    CHECK_CORE(core);
    if (vehicle_id <= 0 || !candidates_json || !candidates_json[0]) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid signing candidates");
    }
    cJSON* root = cJSON_Parse(candidates_json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM,
                    "signing candidates must be a JSON array");
    }
    std::vector<mg::core::mavlink::SigningCandidate> candidates;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        if (!cJSON_IsObject(item)) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM,
                        "signing candidate must be an object");
        }
        const cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON* key_hex = cJSON_GetObjectItemCaseSensitive(item, "key_hex");
        const cJSON* timestamp =
            cJSON_GetObjectItemCaseSensitive(item, "timestamp_ticks");
        if (!cJSON_IsString(name) || !name->valuestring ||
            name->valuestring[0] == '\0' ||
            !cJSON_IsString(key_hex) || !key_hex->valuestring) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM,
                        "signing candidate requires name and key_hex");
        }
        mg::core::mavlink::SigningCandidate candidate;
        candidate.name = name->valuestring;
        if (!decode_signing_key_hex(key_hex->valuestring, candidate.key.data())) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM,
                        "signing candidate key must be 64 hexadecimal characters");
        }
        if (cJSON_IsNumber(timestamp) && timestamp->valuedouble >= 0) {
            candidate.persisted_timestamp =
                static_cast<uint64_t>(timestamp->valuedouble);
        }
        candidates.push_back(std::move(candidate));
    }
    cJSON_Delete(root);
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    if (!vehicle->set_signing_candidates(candidates)) {
        return fail(MG_ERROR_NOT_CONNECTED,
                    "vehicle has no MAVLink route for signing candidates");
    }
    return ok();
}

MGCORE_API char* mg_vehicle_clear_signing_candidates(mg_core_t* core,
                                                     int vehicle_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    vehicle->clear_signing_candidates();
    return ok();
}

MGCORE_API char* mg_vehicle_clear_signing_key(mg_core_t* core,
                                              int vehicle_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    vehicle->clear_signing_key();
    return ok();
}

MGCORE_API char* mg_vehicle_get_signing_status(mg_core_t* core,
                                               int vehicle_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    const int status = vehicle->signing_status();
    mg::bridge::JsonBuilder json;
    json.begin_object();
    json.field("success", true);
    json.field("vehicle_id", vehicle_id);
    json.field("enabled", vehicle->signing_enabled());
    json.field("status", signing_status_name(status));
    json.field("status_code", status);
    json.field("key_name", vehicle->signing_key_name());
    json.field("auto_detected", vehicle->signing_auto_detected());
    json.field("candidate_count",
               static_cast<int>(vehicle->signing_candidate_count()));
    json.field("timestamp_ticks", vehicle->signing_timestamp());
    json.end_object();
    return to_cstr(json.str());
}

MGCORE_API char* mg_vehicle_send_setup_signing(mg_core_t* core,
                                                  int vehicle_id,
                                                  const char* key_hex,
                                                  uint64_t initial_timestamp) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    uint8_t key[32]{};
    if (!decode_signing_key_hex(key_hex, key)) {
        return fail(MG_ERROR_INVALID_PARAM,
                    "signing key must be 64 hexadecimal characters");
    }
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    if (!vehicle->send_setup_signing(
            vehicle->info().system_id, vehicle->info().component_id,
            key, sizeof(key), initial_timestamp)) {
        return fail(MG_ERROR_SEND_FAILED,
                    "failed to send SETUP_SIGNING");
    }
    return ok();
}

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
        result = open_managed_link(core, endpoint_uri);
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

    bool is_link = false;
    bool connected = false;
    bool heartbeat_alive = false;
    uint64_t heartbeat_age_ms = 0;
    bool active_focus = false;
    std::string state = "waiting_heartbeat";
    int vehicle_id = -1;
    for (const auto& link : core->vehicles.managed_links()) {
        if (link.link_id != session_id) continue;
        is_link = true;
        if (link.route_id < 0) continue;
        vehicle_id = link.vehicle_id;
        connected = connected || link.connected;
        heartbeat_alive = heartbeat_alive || link.heartbeat_alive;
        heartbeat_age_ms = std::max(heartbeat_age_ms, link.heartbeat_age_ms);
        active_focus = active_focus || link.active_focus;
        if (link.connected) state = link.state;
        else if (state == "waiting_heartbeat") state = link.state;
    }

    if (!is_link) {
        const auto vehicle = core->vehicles.get_vehicle_shared(session_id);
        if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "connection session not found");
        auto& connection = vehicle->conn();
        connected = connection.is_connected();
        heartbeat_alive = connection.heartbeat_alive(5000);
        heartbeat_age_ms = connection.heartbeat_age_ms();
        active_focus = mg_vehicle_get_active(core) == session_id;
        state = mg::core::transport::ConnectionManager::state_string(
            connection.state());
        vehicle_id = session_id;
    }

    mg::bridge::JsonBuilder result;
    result.begin_object();
    result.field("success", true);
    result.field("session_id", session_id);
    result.field("link_id", is_link ? session_id : -1);
    result.field("vehicle_id", vehicle_id);
    result.field("state", state);
    result.field("connected", connected);
    result.field("heartbeat_age_ms", static_cast<int64_t>(heartbeat_age_ms));
    result.field("heartbeat_alive", heartbeat_alive);
    result.field("active_focus", active_focus);
    result.end_object();
    return to_cstr(result.str());
}

MGCORE_API char* mg_connection_list(mg_core_t* core) {
    CHECK_CORE(core);
    mg::bridge::JsonBuilder array;
    array.begin_array();
    for (const auto& link : core->vehicles.managed_links()) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("link_id", link.link_id);
        item.field("route_id", link.route_id);
        item.field("vehicle_id", link.vehicle_id);
        item.field("endpoint", link.endpoint);
        item.field("system_id", static_cast<int>(link.system_id));
        item.field("connected", link.connected);
        item.field("heartbeat_alive", link.heartbeat_alive);
        item.field("heartbeat_age_ms", static_cast<int64_t>(link.heartbeat_age_ms));
        item.field("active_focus", link.active_focus);
        item.field("primary", link.primary);
        item.field("state", link.state);
        item.end_object();
        array.append_json(item.str());
    }
    array.end_array();
    return to_cstr(array.str());
}

MGCORE_API char* mg_connection_close_route(mg_core_t* core, int route_id) {
    CHECK_CORE(core);
    if (route_id < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid route");
    if (!core->vehicles.close_route(route_id, true)) {
        return fail(MG_ERROR_NOT_FOUND, "connection route not found");
    }
    return ok();
}

MGCORE_API char* mg_connection_close(mg_core_t* core, int session_id) {
    CHECK_CORE(core);
    if (session_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid session");

    if (core->vehicles.close_link(session_id, true)) return ok();

    if (core->vehicles.close_vehicle(session_id, true)) {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->vehicle_mission_documents.erase(session_id);
        return ok();
    }
    return fail(MG_ERROR_NOT_FOUND, "connection session not found");
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
    const int result = open_legacy_vehicle_session(core, uri.c_str());
    return result < 0 ? -1 : result;
}

// ================================================================
// mg_vehicle_close
// ================================================================
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0 || !core->vehicles.close_vehicle(vehicle_id, true)) {
        return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    }
    return ok();
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
