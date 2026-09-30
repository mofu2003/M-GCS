#include "core/mission/cad/mission_interchange.h"

#include <algorithm>

namespace mg::core::mission::cad {

bool is_supported_interchange_format(const std::string& format) {
    return format == "waypoints" || format == "QGC WPL 110" ||
           format == "plan" || format == "qgc_plan" || format == "qgc.plan";
}

std::string recommended_mission_interchange_format(
    const MissionDocument& document) {
    const bool has_complex_entity = std::any_of(
        document.entities.begin(),
        document.entities.end(),
        [](const MissionEntity& entity) {
            return is_parameterized_survey_type(entity.type);
        });
    if (document.metadata.firmware_profile == "px4" || has_complex_entity) {
        return "plan";
    }
    return "waypoints";
}

} // namespace mg::core::mission::cad
