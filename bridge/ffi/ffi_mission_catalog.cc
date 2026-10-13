#define MGCORE_EXPORTS

#include "ffi_internal.h"

#include "core/mission/cad/mission_command_catalog.h"

#include "core/firmware/firmware_adapter.h"

namespace {

const char* parameter_type_name(
    mg::core::mission::cad::MissionParameterType type) {
    using mg::core::mission::cad::MissionParameterType;
    switch (type) {
        case MissionParameterType::Float: return "float";
        case MissionParameterType::Integer: return "integer";
        case MissionParameterType::Boolean: return "boolean";
        case MissionParameterType::Enum: return "enum";
    }
    return "float";
}

std::vector<const char*> vehicle_names(uint16_t vehicles) {
    using namespace mg::core::mission::cad;
    std::vector<const char*> names;
    if (vehicles & mission_vehicle::kCopter) names.push_back("copter");
    if (vehicles & mission_vehicle::kPlane) names.push_back("plane");
    if (vehicles & mission_vehicle::kVtol) names.push_back("vtol");
    if (vehicles & mission_vehicle::kRover) names.push_back("rover");
    if (vehicles & mission_vehicle::kSub) names.push_back("sub");
    if (vehicles & mission_vehicle::kTracker) names.push_back("tracker");
    return names;
}

} // namespace

MGCORE_API char* mg_mission_get_command_catalog(mg_core_t* core) {
    CHECK_CORE(core);

    std::string vehicle_class;
    if (auto* cache = get_active_cache(core)) {
        const auto heartbeat = cache->snapshot_heartbeat().data;
        // 只按 MAV_TYPE 归一化，不读任何参数（ADR-0010 第 4 条）。
        // 命令目录每个会话只取一次并缓存，若这里依赖 Q_ENABLE，则参数到达前
        // 取到的目录会把错误结果锁死整个会话。MAV_TYPE 从第一条心跳起就稳定，
        // 因此这里的结果不会再随时间变化。
        mg::core::firmware::FirmwareAdapter adapter;
        adapter.detect(heartbeat.autopilot_type, heartbeat.vehicle_type);
        if (adapter.firmware_kind() ==
            mg::core::firmware::FirmwareKind::ArduPilot) {
            switch (adapter.vehicle_kind()) {
                case mg::core::firmware::VehicleKind::Quadrotor:
                case mg::core::firmware::VehicleKind::Coaxial:
                case mg::core::firmware::VehicleKind::Helicopter:
                case mg::core::firmware::VehicleKind::Hexarotor:
                case mg::core::firmware::VehicleKind::Octorotor:
                case mg::core::firmware::VehicleKind::Tricopter:
                    vehicle_class = "copter";
                    break;
                case mg::core::firmware::VehicleKind::FixedWing:
                    vehicle_class = "plane";
                    break;
                case mg::core::firmware::VehicleKind::VTOL:
                    vehicle_class = "vtol";
                    break;
                case mg::core::firmware::VehicleKind::GroundRover:
                case mg::core::firmware::VehicleKind::SurfaceBoat:
                    vehicle_class = "rover";
                    break;
                case mg::core::firmware::VehicleKind::Submarine:
                    vehicle_class = "sub";
                    break;
                case mg::core::firmware::VehicleKind::Tracker:
                    vehicle_class = "tracker";
                    break;
                default:
                    break;
            }
        }
    }

    const auto& catalog = mg::core::mission::cad::mission_command_catalog();
    mg::core::telemetry::JsonBuilder root;
    root.begin_object();
    root.field("success", true);
    root.field("schema_version", 1);

    mg::core::telemetry::JsonBuilder commands;
    commands.begin_array();
    for (const auto& command : catalog) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("command", static_cast<int>(command.command));
        item.field("name", command.name);
        item.field("label", command.label);
        item.field("entity_type", command.entity_type);
        item.field("category", command.category);
        item.field("description", command.description);
        item.field("requires_position", command.requires_position);
        item.field("requires_altitude", command.requires_altitude);
        item.field("supported", mg::core::mission::cad::
                                    mission_command_supports_vehicle(
                                        command.command, vehicle_class));

        mg::core::telemetry::JsonBuilder vehicles;
        vehicles.begin_array();
        for (const char* name : vehicle_names(command.vehicles)) {
            vehicles.append_string(name);
        }
        vehicles.end_array();
        item.raw_field("vehicles", vehicles.str());

        mg::core::telemetry::JsonBuilder parameters;
        parameters.begin_array();
        for (const auto& parameter : command.parameters) {
            mg::core::telemetry::JsonBuilder parameter_json;
            parameter_json.begin_object();
            parameter_json.field("index", static_cast<int>(parameter.index));
            parameter_json.field("name", parameter.name);
            parameter_json.field("label", parameter.label);
            parameter_json.field("unit", parameter.unit);
            parameter_json.field(
                "type", parameter_type_name(parameter.type));
            parameter_json.field("default", parameter.default_value);
            if (parameter.has_minimum) {
                parameter_json.field("minimum", parameter.minimum);
            }
            if (parameter.has_maximum) {
                parameter_json.field("maximum", parameter.maximum);
            }
            parameter_json.field("required", parameter.required);
            if (!parameter.options.empty()) {
                mg::core::telemetry::JsonBuilder options;
                options.begin_array();
                for (const auto& option : parameter.options) {
                    mg::core::telemetry::JsonBuilder option_json;
                    option_json.begin_object();
                    option_json.field("value", option.value);
                    option_json.field("label", option.label);
                    option_json.end_object();
                    options.append_json(option_json.str());
                }
                options.end_array();
                parameter_json.raw_field("options", options.str());
            }
            parameter_json.end_object();
            parameters.append_json(parameter_json.str());
        }
        parameters.end_array();
        item.raw_field("parameters", parameters.str());
        item.end_object();
        commands.append_json(item.str());
    }
    commands.end_array();
    root.raw_field("commands", commands.str());
    root.end_object();
    return to_cstr(root.str());
}
