#define MGCORE_EXPORTS

#include "ffi_internal.h"

#include "core/mission/cad/mission_command_catalog.h"

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

} // namespace

MGCORE_API char* mg_mission_get_command_catalog(mg_core_t* core) {
    CHECK_CORE(core);

    const auto& catalog = mg::core::mission::cad::mission_command_catalog();
    mg::bridge::JsonBuilder root;
    root.begin_object();
    root.field("success", true);
    root.field("schema_version", 1);

    mg::bridge::JsonBuilder commands;
    commands.begin_array();
    for (const auto& command : catalog) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("command", static_cast<int>(command.command));
        item.field("name", command.name);
        item.field("category", command.category);
        item.field("description", command.description);
        item.field("requires_position", command.requires_position);
        item.field("requires_altitude", command.requires_altitude);

        mg::bridge::JsonBuilder parameters;
        parameters.begin_array();
        for (const auto& parameter : command.parameters) {
            mg::bridge::JsonBuilder parameter_json;
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
                mg::bridge::JsonBuilder options;
                options.begin_array();
                for (const auto& option : parameter.options) {
                    mg::bridge::JsonBuilder option_json;
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
