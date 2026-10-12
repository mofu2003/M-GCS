#include "mission_document_internal.h"

#include "core/mission/cad/mission_command_catalog.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace mg::core::mission::cad::detail {

namespace {

constexpr double kMinLatitude = -90.0;
constexpr double kMaxLatitude = 90.0;
constexpr double kMinLongitude = -180.0;
constexpr double kMaxLongitude = 180.0;
constexpr float kMaxMissionAltitudeM = 500.0f;

bool type_has_expected_command(
    const std::string& type,
    uint16_t command,
    uint16_t& expected) {
    if (type == "waypoint") expected = static_cast<uint16_t>(NavCommand::NavWaypoint);
    else if (type == "takeoff") expected = static_cast<uint16_t>(NavCommand::NavTakeoff);
    else if (type == "vtol_takeoff") expected = static_cast<uint16_t>(NavCommand::NavVtolTakeoff);
    else if (type == "land") expected = static_cast<uint16_t>(NavCommand::NavLand);
    else if (type == "vtol_land") expected = static_cast<uint16_t>(NavCommand::NavVtolLand);
    else if (type == "rtl") expected = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    else if (type == "loiter") expected = static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    else return false;
    return command != expected;
}

const MissionCommandDescriptor* find_command_descriptor(uint16_t command) {
    const auto& catalog = mission_command_catalog();
    auto found = std::find_if(
        catalog.begin(),
        catalog.end(),
        [&](const MissionCommandDescriptor& descriptor) {
            return descriptor.command == command;
        });
    return found == catalog.end() ? nullptr : &*found;
}

/// ArduPilot 机型 capability table 只覆盖 ArduPilot；未声明 firmware 的
/// 文档不做机型判断。
bool capability_table_applies(const MissionDocument& document) {
    return document.metadata.firmware_profile.empty() ||
           document.metadata.firmware_profile == "ardupilot";
}

/// 命令是否支持文档声明的机型；机型未知或非 ArduPilot 时返回 true。
bool command_supported_for_document(
    const MissionDocument& document,
    const MissionCommandDescriptor& descriptor) {
    if (!capability_table_applies(document)) return true;
    return mission_command_supports_vehicle(
        descriptor.command,
        document.metadata.vehicle_type);
}

/// 参数化测绘实体的校验。展开交给 SurveyCalculator，这里只保证参数自洽，
/// 避免把“尺寸为 0”“终点缺失”这类意图错误拖到编译期才炸。
void validate_survey_entity(
    MissionValidationReport& report,
    const MissionEntity& entity) {
    const MissionSurveySpec& spec = entity.survey;
    const bool corridor = entity.type == "corridor";

    if (!finite(entity.latitude) ||
        entity.latitude < kMinLatitude ||
        entity.latitude > kMaxLatitude) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "entity.latitude",
            corridor ? "Corridor start latitude is invalid or missing"
                     : "Survey center latitude is invalid or missing",
            entity.id,
            "lat");
    }
    if (!finite(entity.longitude) ||
        entity.longitude < kMinLongitude ||
        entity.longitude > kMaxLongitude) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "entity.longitude",
            corridor ? "Corridor start longitude is invalid or missing"
                     : "Survey center longitude is invalid or missing",
            entity.id,
            "lon");
    }

    if (corridor) {
        const bool end_lat_ok = finite(spec.end_latitude) &&
                                spec.end_latitude >= kMinLatitude &&
                                spec.end_latitude <= kMaxLatitude;
        const bool end_lon_ok = finite(spec.end_longitude) &&
                                spec.end_longitude >= kMinLongitude &&
                                spec.end_longitude <= kMaxLongitude;
        if (!end_lat_ok) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.end_lat",
                "Corridor end latitude is invalid or missing",
                entity.id,
                "survey.end_lat");
        }
        if (!end_lon_ok) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.end_lon",
                "Corridor end longitude is invalid or missing",
                entity.id,
                "survey.end_lon");
        }
        if (end_lat_ok && end_lon_ok && finite(entity.latitude) &&
            finite(entity.longitude) &&
            spec.end_latitude == entity.latitude &&
            spec.end_longitude == entity.longitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.zero_length",
                "Corridor start and end points are identical",
                entity.id,
                "survey.end_lat");
        }
        if (!(spec.corridor_width_m > 0.0)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.corridor_width",
                "Corridor width must be positive",
                entity.id,
                "survey.corridor_width_m");
        }
    } else {
        if (!(spec.width_m > 0.0)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.width",
                "Survey width must be positive",
                entity.id,
                "survey.width_m");
        }
        if (!(spec.length_m > 0.0)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.length",
                "Survey length must be positive",
                entity.id,
                "survey.length_m");
        }
        if (!finite(spec.angle_deg)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "survey.angle",
                "Survey rotation angle must be finite",
                entity.id,
                "survey.angle_deg");
        }
    }

    if (!finite(spec.altitude_m) || !(spec.altitude_m > 0.0f)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "survey.altitude",
            "Survey altitude must be positive",
            entity.id,
            "survey.alt_m");
    } else if (spec.altitude_m > kMaxMissionAltitudeM) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "survey.altitude_high",
            "Survey altitude is unusually high",
            entity.id,
            "survey.alt_m");
    }
    if (!finite(spec.speed_ms) || !(spec.speed_ms > 0.0f)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "survey.speed",
            "Survey speed must be positive",
            entity.id,
            "survey.speed_ms");
    }
    const auto overlap_ok = [](float value) {
        return std::isfinite(value) && value >= 0.0f && value < 100.0f;
    };
    if (!overlap_ok(spec.forward_overlap_pct)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "survey.forward_overlap",
            "Forward overlap must be in [0, 100)",
            entity.id,
            "survey.forward_overlap_pct");
    }
    if (!overlap_ok(spec.side_overlap_pct)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "survey.side_overlap",
            "Side overlap must be in [0, 100)",
            entity.id,
            "survey.side_overlap_pct");
    }
    if (!(spec.focal_length_mm > 0.0f) || !(spec.sensor_width_mm > 0.0f) ||
        !(spec.sensor_height_mm > 0.0f) || !(spec.image_width_px > 0.0f) ||
        !(spec.image_height_px > 0.0f)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "survey.camera",
            "Camera focal length, sensor size and image size must be positive",
            entity.id,
            "survey");
    }
}

} // namespace

void add_diagnostic(
    MissionValidationReport& report,
    DiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string entity_id,
    std::string field) {
    report.diagnostics.push_back(
        MissionDiagnostic{
            std::move(code),
            severity,
            std::move(message),
            std::move(entity_id),
            std::move(field),
        });
    if (severity == DiagnosticSeverity::Error) {
        report.valid = false;
    }
}

bool finite(double value) {
    return std::isfinite(value);
}

bool position_command(uint16_t command) {
    switch (command) {
        case static_cast<uint16_t>(NavCommand::NavWaypoint):
        case static_cast<uint16_t>(NavCommand::NavLoiterUnlim):
        case static_cast<uint16_t>(NavCommand::NavLoiterTurns):
        case static_cast<uint16_t>(NavCommand::NavLoiterTime):
        case static_cast<uint16_t>(NavCommand::NavLand):
        case static_cast<uint16_t>(NavCommand::NavVtolLand):
        case static_cast<uint16_t>(NavCommand::NavVtolTakeoff):
        case static_cast<uint16_t>(NavCommand::NavSplineWaypoint):
            return true;
        default:
            return false;
    }
}

MissionValidationReport validate_plan_for_vehicle(
    const MissionPlan& plan,
    const std::string& vehicle_class) {
    MissionValidationReport report;
    if (mission_vehicle_class_bit(vehicle_class) == 0) return report;
    for (const MissionItem& item : plan.items) {
        const MissionCommandDescriptor* descriptor =
            find_command_descriptor(item.command);
        if (!descriptor) continue;
        if (descriptor->vehicles == 0) continue;
        if (mission_command_supports_vehicle(item.command, vehicle_class)) {
            continue;
        }
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "mission.command_vehicle_unsupported",
            descriptor->name + " is not supported on " +
                mission_vehicle_class_label(vehicle_class),
            std::to_string(item.seq),
            "command");
    }
    return report;
}

MissionValidationReport validate_document(const MissionDocument& document) {
    MissionValidationReport report;
    if (document.schema_version <= 0 ||
        document.schema_version > kMissionDocumentSchemaVersion) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "schema.unsupported",
            "Unsupported mission document schema version",
            {},
            "schema_version");
    }
    if (document.metadata.id.empty()) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "document.id_missing",
            "Mission document id is empty",
            {},
            "metadata.id");
    }
    if (!(document.defaults.altitude_m > 0.0f) ||
        !std::isfinite(document.defaults.altitude_m)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "defaults.altitude",
            "Default altitude must be positive",
            {},
            "defaults.altitude_m");
    }
    if (!(document.defaults.speed_ms > 0.0f) ||
        !std::isfinite(document.defaults.speed_ms)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "defaults.speed",
            "Default speed must be positive",
            {},
            "defaults.speed_ms");
    }
    if (document.home.set) {
        if (!finite(document.home.latitude) ||
            document.home.latitude < kMinLatitude ||
            document.home.latitude > kMaxLatitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "home.latitude",
                "Home latitude is invalid",
                {},
                "home.latitude");
        }
        if (!finite(document.home.longitude) ||
            document.home.longitude < kMinLongitude ||
            document.home.longitude > kMaxLongitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "home.longitude",
                "Home longitude is invalid",
                {},
                "home.longitude");
        }
    } else {
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "home.missing",
            "Home position is not set; relative altitudes cannot be previewed accurately",
            {},
            "home");
    }

    std::unordered_set<std::string> ids;
    size_t enabled_count = 0;
    std::vector<const MissionEntity*> enabled;

    // QGC complex items (Survey, CorridorScan, ...) carry their own computed
    // mission items, so they can be expanded and uploaded. An item that cannot be
    // expanded stays an error so upload never silently drops it.
    for (const MissionComplexItem& complex : document.complex_items) {
        std::vector<MissionEntity> expanded;
        std::string expand_error;
        if (!expand_complex_item_entities(complex, expanded, expand_error)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "complex_item.unsupported",
                expand_error.empty()
                    ? "QGC complex item cannot be expanded into mission items"
                    : expand_error,
                complex.id,
                "complex_items");
            continue;
        }
        for (const MissionEntity& entity : expanded) {
            const MissionCommandDescriptor* expanded_descriptor =
                find_command_descriptor(entity.command);
            if (entity.command == 0 || expanded_descriptor == nullptr) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "complex_item.unsupported_command",
                    "Expanded complex item contains a command that is not in "
                    "the supported catalog",
                    entity.id,
                    "command");
                continue;
            }
            if (!command_supported_for_document(document, *expanded_descriptor)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "complex_item.command_vehicle_unsupported",
                    std::string("Expanded complex item uses ") +
                        expanded_descriptor->name + " which is not supported on " +
                        mission_vehicle_class_label(
                            document.metadata.vehicle_type),
                    entity.id,
                    "command");
            }
        }
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "complex_item.expanded",
            "Complex item geometry is expanded from the stored QGroundControl "
            "mission items and is read-only in M-GCS",
            complex.id,
            "complex_items");
    }

    for (size_t index = 0; index < document.rally_points.size(); ++index) {
        const MissionRallyPoint& rally = document.rally_points[index];
        const std::string field =
            "rally_points[" + std::to_string(index) + "]";
        if (!finite(rally.latitude) || rally.latitude < kMinLatitude ||
            rally.latitude > kMaxLatitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "rally.latitude",
                "Rally point latitude is invalid",
                {},
                field + ".latitude");
        }
        if (!finite(rally.longitude) || rally.longitude < kMinLongitude ||
            rally.longitude > kMaxLongitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "rally.longitude",
                "Rally point longitude is invalid",
                {},
                field + ".longitude");
        }
        if (!finite(static_cast<double>(rally.altitude_m))) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "rally.altitude",
                "Rally point altitude must be finite",
                {},
                field + ".altitude_m");
        }
    }

    for (size_t index = 0; index < document.geo_fence.polygons.size(); ++index) {
        const MissionGeoFencePolygon& polygon =
            document.geo_fence.polygons[index];
        const std::string field =
            "geo_fence.polygons[" + std::to_string(index) + "]";
        if (polygon.vertices.size() < 3) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "geo_fence.polygon_vertices",
                "GeoFence polygon requires at least three vertices",
                {},
                field + ".vertices");
        }
        for (size_t vertex_index = 0; vertex_index < polygon.vertices.size();
             ++vertex_index) {
            const MissionGeoPoint& vertex = polygon.vertices[vertex_index];
            if (!finite(vertex.latitude) || vertex.latitude < kMinLatitude ||
                vertex.latitude > kMaxLatitude ||
                !finite(vertex.longitude) ||
                vertex.longitude < kMinLongitude ||
                vertex.longitude > kMaxLongitude) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "geo_fence.vertex",
                    "GeoFence polygon vertex is invalid",
                    {},
                    field + ".vertices[" + std::to_string(vertex_index) + "]");
            }
        }
    }

    for (size_t index = 0; index < document.geo_fence.circles.size(); ++index) {
        const MissionGeoFenceCircle& circle = document.geo_fence.circles[index];
        const std::string field =
            "geo_fence.circles[" + std::to_string(index) + "]";
        if (!finite(circle.center.latitude) ||
            circle.center.latitude < kMinLatitude ||
            circle.center.latitude > kMaxLatitude ||
            !finite(circle.center.longitude) ||
            circle.center.longitude < kMinLongitude ||
            circle.center.longitude > kMaxLongitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "geo_fence.circle_center",
                "GeoFence circle center is invalid",
                {},
                field + ".center");
        }
        if (!finite(circle.radius_m) || circle.radius_m <= 0.0) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "geo_fence.circle_radius",
                "GeoFence circle radius must be positive",
                {},
                field + ".radius_m");
        }
    }

    if (document.geo_fence.breach_return_set) {
        const MissionGeoPoint& breach = document.geo_fence.breach_return;
        if (!finite(breach.latitude) || breach.latitude < kMinLatitude ||
            breach.latitude > kMaxLatitude || !finite(breach.longitude) ||
            breach.longitude < kMinLongitude ||
            breach.longitude > kMaxLongitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "geo_fence.breach_return",
                "GeoFence breach-return point is invalid",
                {},
                "geo_fence.breach_return");
        }
        if (!finite(
                static_cast<double>(document.geo_fence.breach_return_altitude_m))) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "geo_fence.breach_return_altitude",
                "GeoFence breach-return altitude must be finite",
                {},
                "geo_fence.breach_return_altitude_m");
        }
    }

    for (size_t index = 0; index < document.entities.size(); ++index) {
        const MissionEntity& entity = document.entities[index];
        if (entity.id.empty()) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.id_missing",
                "Mission entity id is empty",
                entity.id,
                "id");
        } else if (!ids.insert(entity.id).second) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.id_duplicate",
                "Mission entity id is duplicated",
                entity.id,
                "id");
        }
        if (entity.order != index) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "entity.order",
                "Mission entity order does not match document order",
                entity.id,
                "order");
        }
        if (!entity.enabled) {
            continue;
        }
        ++enabled_count;
        enabled.push_back(&entity);

        if (entity.command == 0) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.command_missing",
                "Mission command is required",
                entity.id,
                "command");
            continue;
        }
        if (entity.frame != 0 && entity.frame != 2 &&
            entity.frame != 3 && entity.frame != 10) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.frame_unsupported",
                "Mission frame is not supported",
                entity.id,
                "frame");
        }
        if (entity.autocontinue > 1) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.autocontinue",
                "Autocontinue must be 0 or 1",
                entity.id,
                "autocontinue");
        }
        for (size_t parameter_index = 0;
             parameter_index < entity.params.size();
             ++parameter_index) {
            if (!std::isfinite(entity.params[parameter_index])) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_non_finite",
                    "Mission parameter must be finite",
                    entity.id,
                    "params[" + std::to_string(parameter_index) + "]");
            }
        }
        if (is_parameterized_survey_type(entity.type)) {
            validate_survey_entity(report, entity);
            continue;
        }

        const MissionCommandDescriptor* descriptor =
            find_command_descriptor(entity.command);
        if (!descriptor) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.command_unsupported",
                "Mission command is not present in the supported command catalog",
                entity.id,
                "command");
            continue;
        }
        if (!command_supported_for_document(document, *descriptor)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.command_vehicle_unsupported",
                descriptor->name + " is not supported on " +
                    mission_vehicle_class_label(document.metadata.vehicle_type),
                entity.id,
                "command");
        }

        uint16_t expected_command = 0;
        if (type_has_expected_command(entity.type, entity.command, expected_command)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "entity.type_command_mismatch",
                "Entity type does not match its command",
                entity.id,
                "command");
        }

        for (const MissionCommandParameter& parameter : descriptor->parameters) {
            if (parameter.index < 1 || parameter.index > 4) {
                continue;
            }
            const float value = entity.params[parameter.index - 1];
            const std::string field =
                "params[" + std::to_string(parameter.index - 1) + "]";
            if (parameter.has_minimum && value < parameter.minimum) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_minimum",
                    parameter.label + " is below the supported minimum",
                    entity.id,
                    field);
            }
            if (parameter.has_maximum && value > parameter.maximum) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_maximum",
                    parameter.label + " is above the supported maximum",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Integer &&
                std::abs(value - std::round(value)) > 1e-6f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_integer",
                    parameter.label + " must be an integer",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Boolean &&
                std::abs(value) > 1e-6f &&
                std::abs(value - 1.0f) > 1e-6f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_boolean",
                    parameter.label + " must be 0 or 1",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Enum &&
                !parameter.options.empty()) {
                const bool option_matches = std::any_of(
                    parameter.options.begin(),
                    parameter.options.end(),
                    [&](const MissionCommandOption& option) {
                        return std::abs(value - option.value) < 1e-6;
                    });
                if (!option_matches) {
                    add_diagnostic(
                        report,
                        DiagnosticSeverity::Error,
                        "entity.parameter_enum",
                        parameter.label + " is not a supported option",
                        entity.id,
                        field);
                }
            }
        }

        // RTL 的高度与坐标语义和普通航点不同，单独说明：
        //   * 高度：mission_compile.cc:188-190 无条件把 entity.altitude_m 写进
        //     任务项（非有限值才回落默认高度），所以有限但非法的高度（负数）会
        //     原样下发，必须在这里拦下；
        //   * 坐标：AP_Mission::stored_in_location() 不含
        //     MAV_CMD_NAV_RETURN_TO_LAUNCH(20)，飞控不用它、回读时也不还原
        //     （mission_document.cc 从飞控下载的 RTL 坐标就是 NaN），所以
        //     "没给坐标"是合法状态，只有给了却越界才报错。
        const bool is_return_to_launch =
            entity.command == static_cast<uint16_t>(NavCommand::NavReturnToLaunch);

        if (position_command(entity.command)) {
            if (!finite(entity.latitude) ||
                entity.latitude < kMinLatitude ||
                entity.latitude > kMaxLatitude) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.latitude",
                    "Mission latitude is invalid or missing",
                    entity.id,
                    "lat");
            }
            if (!finite(entity.longitude) ||
                entity.longitude < kMinLongitude ||
                entity.longitude > kMaxLongitude) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.longitude",
                    "Mission longitude is invalid or missing",
                    entity.id,
                    "lon");
            }
        } else if (is_return_to_launch) {
            if (finite(entity.latitude) &&
                (entity.latitude < kMinLatitude ||
                 entity.latitude > kMaxLatitude)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.latitude",
                    "Mission latitude is out of range",
                    entity.id,
                    "lat");
            }
            if (finite(entity.longitude) &&
                (entity.longitude < kMinLongitude ||
                 entity.longitude > kMaxLongitude)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.longitude",
                    "Mission longitude is out of range",
                    entity.id,
                    "lon");
            }
        }

        const float altitude = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        if (position_command(entity.command) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavTakeoff) ||
            is_return_to_launch) {
            if (!(altitude >= 0.0f)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.altitude",
                    "Mission altitude must be non-negative",
                    entity.id,
                    "alt_m");
            } else if (altitude > kMaxMissionAltitudeM) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Warning,
                    "entity.altitude_high",
                    "Mission altitude is unusually high",
                    entity.id,
                    "alt_m");
            }
        }

        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            if (entity.params[0] < 0.0f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.hold_time",
                    "Waypoint hold time must be non-negative",
                    entity.id,
                    "params[0]");
            }
            if (entity.params[1] < 0.0f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.accept_radius",
                    "Waypoint acceptance radius must be non-negative",
                    entity.id,
                    "params[1]");
            }
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterUnlim) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            // 半径的合法区间由命令目录的 parameter(3, "radius", ...) 描述符给出
            // （对齐 QGC MavCmdInfoCommon.json 的 userMin -100 / userMax 100），
            // 命令目录检查在下面统一执行。这里只补一条目录无法表达的约束：
            // 半径必须是有限值——NaN 会原样写进任务项传给飞控。
            // 注意 NAV_LOITER_UNLIM 不在命令目录里，会在上面被
            // entity.command_unsupported 拦下并 continue，到不了这里。
            if (!std::isfinite(entity.params[2])) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.loiter_radius",
                    "Loiter radius must be a finite number",
                    entity.id,
                    "params[2]");
            }
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::DoSetSpeed) &&
            !(entity.params[1] > 0.0f)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.speed",
                "Speed change must be positive",
                entity.id,
                "params[1]");
        }
        if (entity.command ==
                static_cast<uint16_t>(NavCommand::DoSetCamTrigDist) &&
            entity.params[0] < 0.0f) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.trigger_distance",
                "Camera trigger distance must be non-negative",
                entity.id,
                "params[0]");
        }
    }

    if (enabled_count == 0) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "mission.empty",
            "Mission document has no enabled entities",
            {},
            "entities");
    } else {
        const MissionEntity& first = *enabled.front();
        if (first.command != static_cast<uint16_t>(NavCommand::NavTakeoff) &&
            first.command != static_cast<uint16_t>(NavCommand::NavVtolTakeoff) &&
            first.command != static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "mission.first_item",
                "First mission item should be takeoff or waypoint",
                first.id,
                "command");
        }
        const MissionEntity& last = *enabled.back();
        if (last.command != static_cast<uint16_t>(NavCommand::NavLand) &&
            last.command != static_cast<uint16_t>(NavCommand::NavVtolLand) &&
            last.command != static_cast<uint16_t>(NavCommand::NavReturnToLaunch)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "mission.last_item",
                "Last mission item should be land or return-to-launch",
                last.id,
                "command");
        }
    }

    return report;
}

} // namespace mg::core::mission::cad::detail

namespace mg::core::mission::cad {

MissionValidationReport validate_plan_for_vehicle(
    const MissionPlan& plan,
    const std::string& vehicle_class) {
    return detail::validate_plan_for_vehicle(plan, vehicle_class);
}

} // namespace mg::core::mission::cad
