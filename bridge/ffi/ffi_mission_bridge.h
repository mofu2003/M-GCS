#ifndef MGCORE_FFI_MISSION_BRIDGE_H
#define MGCORE_FFI_MISSION_BRIDGE_H

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
#include "core/mission/mission_types.h"

struct mg_core_t;

namespace mg::bridge::ffi {

/// ArduPilot 的 mission 列表把 Home 放在第 0 项。
bool mission_uses_home_item(mg_core_t* core);

mg::core::mission::MissionPlan present_mission_plan(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& raw_plan,
    mg::core::mission::cad::MissionHome* home = nullptr,
    bool* home_present = nullptr);

mg::core::mission::MissionPlan prepare_mission_upload(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& client_plan,
    const mg::core::mission::cad::MissionHome* planned_home = nullptr);

mg::core::mission::MissionTransferProgress present_mission_progress(
    mg_core_t* core,
    const mg::core::mission::MissionTransferProgress& raw_progress);

int present_mission_current_index(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& raw_plan);

int prepare_mission_current_index(mg_core_t* core, int client_visible_index);

} // namespace mg::bridge::ffi

#endif /* MGCORE_FFI_MISSION_BRIDGE_H */
