#include "mission_document_internal.h"

#include <cmath>

namespace mg::core::mission::cad::detail {

// Survey功能已完全删除，保留存根避免编译错误
bool expand_document_entities(
    const MissionDocument& document,
    std::vector<ExpandedEntity>& out,
    std::string& error) {
    error.clear();
    out.clear();

    // Preserved QGC complex items carry their own computed mission items
    std::vector<size_t> complex_order(document.complex_items.size());
    for (size_t i = 0; i < complex_order.size(); ++i) complex_order[i] = i;
    std::stable_sort(
        complex_order.begin(),
        complex_order.end(),
        [&document](size_t a, size_t b) {
            return document.complex_items[a].entity_insert_index <
                   document.complex_items[b].entity_insert_index;
        });

    size_t next_complex = 0;
    auto emit_complex_before = [&](size_t entity_index) -> bool {
        while (next_complex < complex_order.size() &&
               document.complex_items[complex_order[next_complex]]
                       .entity_insert_index <= entity_index) {
            const MissionComplexItem& complex =
                document.complex_items[complex_order[next_complex]];
            std::vector<MissionEntity> expanded;
            if (!expand_complex_item_entities(complex, expanded, error)) {
                out.clear();
                return false;
            }
            for (MissionEntity entity : expanded) {
                entity.order = static_cast<uint32_t>(out.size());
                out.push_back({std::move(entity), complex.id});
            }
            ++next_complex;
        }
        return true;
    };

    for (size_t index = 0; index < document.entities.size(); ++index) {
        if (!emit_complex_before(index)) return false;
        const MissionEntity& entity = document.entities[index];
        out.push_back({entity, entity.id});
    }
    if (!emit_complex_before(document.entities.size())) return false;
    if (!emit_complex_before(kComplexItemAppendAtEnd)) return false;
    return true;
}

MissionCompileResult compile_document_impl(const MissionDocument& document) {
    MissionCompileResult result;
    result.report = validate_document(document);
    if (result.report.has_errors()) {
        return result;
    }

    std::vector<ExpandedEntity> ordered;
    std::string expand_error;
    if (!expand_document_entities(document, ordered, expand_error)) {
        add_diagnostic(
            result.report,
            DiagnosticSeverity::Error,
            "complex_item.unsupported",
            expand_error.empty() ? "complex item cannot be expanded" : expand_error,
            {},
            "complex_items");
        result.valid = false;
        return result;
    }

    for (const ExpandedEntity& expanded : ordered) {
        const MissionEntity& entity = expanded.entity;
        if (!entity.enabled) {
            continue;
        }

        // Survey功能已删除：如果是Survey类型，直接报错
        if (is_parameterized_survey_type(entity.type)) {
            add_diagnostic(
                result.report,
                DiagnosticSeverity::Error,
                "entity.survey_removed",
                "Survey generation removed in 0.2.x (ArduPilot-only)",
                entity.id,
                "type");
            result.plan.valid = false;
            result.valid = false;
            return result;
        }

        MissionItem item{};
        item.seq = static_cast<uint16_t>(result.plan.items.size());
        item.frame = entity.frame;
        item.command = entity.command;
        item.autocontinue = entity.autocontinue;
        item.param1 = entity.params[0];
        item.param2 = entity.params[1];
        item.param3 = entity.params[2];
        item.param4 = entity.params[3];
        const bool has_geographic_position =
            is_global_coordinate_frame(item.frame) &&
            mission_command_uses_location(item.command);
        if (has_geographic_position && std::isfinite(entity.latitude)) {
            item.x = static_cast<int32_t>(std::llround(entity.latitude * 1e7));
        }
        if (has_geographic_position && std::isfinite(entity.longitude)) {
            item.y = static_cast<int32_t>(std::llround(entity.longitude * 1e7));
        }
        item.z = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        item.mission_type = 0;
        result.plan.items.push_back(item);
        result.source_entity_ids.push_back(expanded.source_id);
    }

    result.plan.valid = !result.plan.items.empty();
    result.valid = result.plan.valid;
    return result;
}

} // namespace mg::core::mission::cad::detail

namespace mg::core::mission::cad {

namespace {

// MAV_CMD_NAV_FENCE_* / MAV_FRAME_* 常量
constexpr uint16_t kFenceReturnPoint = 5000;
constexpr uint16_t kFencePolygonVertexInclusion = 5001;
constexpr uint16_t kFencePolygonVertexExclusion = 5002;
constexpr uint16_t kFenceCircleInclusion = 5003;
constexpr uint16_t kFenceCircleExclusion = 5004;
constexpr uint8_t kFrameGlobal = 0;
constexpr uint8_t kFrameGlobalRelAlt = 3;
constexpr uint8_t kMissionTypeFence = 1;
constexpr uint16_t kNavRallyPoint = 5100;
constexpr uint8_t kMissionTypeRally = 2;

bool fence_coordinate_valid(const MissionGeoPoint& point) {
    return std::isfinite(point.latitude) &&
           point.latitude >= -90.0 && point.latitude <= 90.0 &&
           std::isfinite(point.longitude) &&
           point.longitude >= -180.0 && point.longitude <= 180.0;
}

MissionItem fence_item(uint8_t frame, uint16_t command, double latitude,
                       double longitude) {
    MissionItem item{};
    item.frame = frame;
    item.command = command;
    item.autocontinue = 0;
    item.x = static_cast<int32_t>(std::llround(latitude * 1e7));
    item.y = static_cast<int32_t>(std::llround(longitude * 1e7));
    item.mission_type = kMissionTypeFence;
    return item;
}

} // namespace

bool compile_geo_fence_plan(
    const MissionGeoFence& fence,
    MissionPlan& plan,
    std::string& error) {
    error.clear();
    plan.clear();

    const bool has_breach = fence.breach_return_set;
    if (fence.polygons.empty() && fence.circles.empty() && !has_breach) {
        error = "geo fence is empty";
        return false;
    }

    for (size_t polygon_index = 0; polygon_index < fence.polygons.size();
         ++polygon_index) {
        const MissionGeoFencePolygon& polygon = fence.polygons[polygon_index];
        if (polygon.vertices.size() < 3) {
            error = "geo fence polygon " + std::to_string(polygon_index) +
                    " requires at least three vertices";
            plan.clear();
            return false;
        }
        const uint16_t command = polygon.inclusion
            ? kFencePolygonVertexInclusion
            : kFencePolygonVertexExclusion;
        for (size_t vertex_index = 0; vertex_index < polygon.vertices.size();
             ++vertex_index) {
            const MissionGeoPoint& vertex = polygon.vertices[vertex_index];
            if (!fence_coordinate_valid(vertex)) {
                error = "geo fence polygon " + std::to_string(polygon_index) +
                        " vertex " + std::to_string(vertex_index) +
                        " is invalid";
                plan.clear();
                return false;
            }
            MissionItem item =
                fence_item(kFrameGlobal, command, vertex.latitude,
                           vertex.longitude);
            item.param1 = static_cast<float>(polygon.vertices.size());
            item.seq = static_cast<uint16_t>(plan.items.size());
            plan.items.push_back(item);
        }
    }

    for (size_t circle_index = 0; circle_index < fence.circles.size();
         ++circle_index) {
        const MissionGeoFenceCircle& circle = fence.circles[circle_index];
        if (!fence_coordinate_valid(circle.center)) {
            error = "geo fence circle " + std::to_string(circle_index) +
                    " center is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(circle.radius_m) || circle.radius_m <= 0.0) {
            error = "geo fence circle " + std::to_string(circle_index) +
                    " radius must be positive";
            plan.clear();
            return false;
        }
        MissionItem item = fence_item(
            kFrameGlobal,
            circle.inclusion ? kFenceCircleInclusion : kFenceCircleExclusion,
            circle.center.latitude,
            circle.center.longitude);
        item.param1 = static_cast<float>(circle.radius_m);
        item.seq = static_cast<uint16_t>(plan.items.size());
        plan.items.push_back(item);
    }

    if (has_breach) {
        if (!fence_coordinate_valid(fence.breach_return)) {
            error = "geo fence breach-return point is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(
                static_cast<double>(fence.breach_return_altitude_m))) {
            error = "geo fence breach-return altitude is invalid";
            plan.clear();
            return false;
        }
        MissionItem item = fence_item(
            kFrameGlobalRelAlt,
            kFenceReturnPoint,
            fence.breach_return.latitude,
            fence.breach_return.longitude);
        item.z = fence.breach_return_altitude_m;
        item.seq = static_cast<uint16_t>(plan.items.size());
        plan.items.push_back(item);
    }

    plan.valid = !plan.items.empty();
    return plan.valid;
}

bool compile_rally_plan(
    const std::vector<MissionRallyPoint>& rally_points,
    MissionPlan& plan,
    std::string& error) {
    error.clear();
    plan.clear();

    if (rally_points.empty()) {
        error = "rally point list is empty";
        return false;
    }

    for (size_t index = 0; index < rally_points.size(); ++index) {
        const MissionRallyPoint& rally = rally_points[index];
        const MissionGeoPoint coordinate{rally.latitude, rally.longitude};
        if (!fence_coordinate_valid(coordinate)) {
            error = "rally point " + std::to_string(index) + " is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(static_cast<double>(rally.altitude_m))) {
            error = "rally point " + std::to_string(index) +
                    " altitude is invalid";
            plan.clear();
            return false;
        }
        MissionItem item{};
        item.seq = static_cast<uint16_t>(plan.items.size());
        item.frame = kFrameGlobalRelAlt;
        item.command = kNavRallyPoint;
        item.autocontinue = 0;
        item.x = static_cast<int32_t>(std::llround(rally.latitude * 1e7));
        item.y = static_cast<int32_t>(std::llround(rally.longitude * 1e7));
        item.z = rally.altitude_m;
        item.mission_type = kMissionTypeRally;
        plan.items.push_back(item);
    }

    plan.valid = !plan.items.empty();
    return plan.valid;
}

namespace {

double fence_latitude(const MissionItem& item) {
    return static_cast<double>(item.x) / 1e7;
}

double fence_longitude(const MissionItem& item) {
    return static_cast<double>(item.y) / 1e7;
}

} // namespace

bool geo_fence_from_plan(
    const MissionPlan& plan,
    MissionGeoFence& fence,
    std::string& error) {
    error.clear();
    fence = MissionGeoFence{};

    std::vector<MissionGeoPoint> pending_vertices;
    uint16_t pending_command = 0;
    int expected_vertices = 0;

    auto flush_polygon = [&]() {
        if (pending_vertices.empty()) return;
        MissionGeoFencePolygon polygon;
        polygon.inclusion = pending_command == kFencePolygonVertexInclusion;
        polygon.vertices = pending_vertices;
        fence.polygons.push_back(std::move(polygon));
        pending_vertices.clear();
        pending_command = 0;
        expected_vertices = 0;
    };

    for (size_t index = 0; index < plan.items.size(); ++index) {
        const MissionItem& item = plan.items[index];
        switch (item.command) {
            case kFencePolygonVertexInclusion:
            case kFencePolygonVertexExclusion: {
                const int vertex_count =
                    static_cast<int>(std::lround(item.param1));
                if (pending_vertices.empty()) {
                    pending_command = item.command;
                    expected_vertices = vertex_count;
                } else if (expected_vertices != vertex_count) {
                    error = "fence polygon vertex count changed mid-polygon";
                    return false;
                } else if (pending_command != item.command) {
                    error = "fence polygon inclusion flag changed mid-polygon";
                    return false;
                }
                pending_vertices.push_back(
                    {fence_latitude(item), fence_longitude(item)});
                if (static_cast<int>(pending_vertices.size()) ==
                    expected_vertices) {
                    flush_polygon();
                }
                break;
            }
            case kFenceCircleInclusion:
            case kFenceCircleExclusion: {
                if (!pending_vertices.empty()) {
                    error = "fence polygon is incomplete before a circle item";
                    return false;
                }
                MissionGeoFenceCircle circle;
                circle.inclusion = item.command == kFenceCircleInclusion;
                circle.center = {fence_latitude(item), fence_longitude(item)};
                circle.radius_m = static_cast<double>(item.param1);
                fence.circles.push_back(circle);
                break;
            }
            case kFenceReturnPoint: {
                if (!pending_vertices.empty()) {
                    error =
                        "fence polygon is incomplete before the return point";
                    return false;
                }
                fence.breach_return_set = true;
                fence.breach_return = {fence_latitude(item),
                                       fence_longitude(item)};
                fence.breach_return_altitude_m = item.z;
                break;
            }
            default:
                error = "unsupported fence mission command " +
                        std::to_string(item.command) + " at index " +
                        std::to_string(index);
                return false;
        }
    }

    if (!pending_vertices.empty()) {
        error = "fence polygon is incomplete at the end of the item list";
        return false;
    }
    return true;
}

bool rally_points_from_plan(
    const MissionPlan& plan,
    std::vector<MissionRallyPoint>& rally_points,
    std::string& error) {
    error.clear();
    rally_points.clear();

    for (size_t index = 0; index < plan.items.size(); ++index) {
        const MissionItem& item = plan.items[index];
        if (item.command != kNavRallyPoint) {
            error = "unsupported rally mission command " +
                    std::to_string(item.command) + " at index " +
                    std::to_string(index);
            rally_points.clear();
            return false;
        }
        MissionRallyPoint point;
        point.latitude = fence_latitude(item);
        point.longitude = fence_longitude(item);
        point.altitude_m = item.z;
        rally_points.push_back(point);
    }
    return true;
}

} // namespace mg::core::mission::cad
