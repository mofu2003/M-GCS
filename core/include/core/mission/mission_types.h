/**
 * mission_types.h - Mission/Waypoint data structures
 *
 * Defines the mission item model used throughout the codebase.
 * Uses MISSION_ITEM_INT (msg 73) for precision (degE7 for lat/lon).
 *
 * MAVLink protocol flow (QGC/MP compatible):
 *   Download: GCS->FC REQUEST_LIST -> FC->GCS COUNT -> GCS->FC REQUEST_INT 
 *             -> FC->GCS ITEM_INT (repeated) -> GCS->FC ACK
 *   Upload:   GCS->FC COUNT -> FC->GCS REQUEST_INT -> GCS->FC ITEM_INT 
 *             (repeated) -> FC->GCS ACK
 *   Clear:    GCS->FC CLEAR_ALL -> FC->GCS ACK
 */

#ifndef MGCORE_MISSION_TYPES_H
#define MGCORE_MISSION_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace mg::core::mission {

/// MAV_FRAME coordinate frames (subset commonly used)
enum class CoordFrame : uint8_t {
    Global          = 0,   // WGS84, altitude MSL
    GlobalRelAlt    = 3,   // WGS84, altitude relative to home
    GlobalTerrainAlt = 10, // WGS84, altitude above terrain
    Mission         = 2,   // Same as frame at mission upload time
};

/// Common MAV_CMD values for mission items
enum class NavCommand : uint16_t {
    NavWaypoint       = 16,   // MAV_CMD_NAV_WAYPOINT
    NavLoiterUnlim    = 17,   // MAV_CMD_NAV_LOITER_UNLIM
    NavLoiterTurns    = 18,   // MAV_CMD_NAV_LOITER_TURNS
    NavLoiterTime     = 19,   // MAV_CMD_NAV_LOITER_TIME
    NavReturnToLaunch = 20,   // MAV_CMD_NAV_RETURN_TO_LAUNCH
    NavLand           = 21,   // MAV_CMD_NAV_LAND
    NavTakeoff        = 22,   // MAV_CMD_NAV_TAKEOFF
    NavSplineWaypoint = 82,   // MAV_CMD_NAV_SPLINE_WAYPOINT
    NavVtolTakeoff    = 84,   // MAV_CMD_NAV_VTOL_TAKEOFF
    NavVtolLand       = 85,   // MAV_CMD_NAV_VTOL_LAND
    NavDelay          = 93,   // MAV_CMD_NAV_DELAY
    DoSetSpeed        = 178,  // MAV_CMD_DO_CHANGE_SPEED
    DoSetCamTrigDist  = 206,  // MAV_CMD_DO_SET_CAM_TRIGG_DIST
    DoDigicamControl  = 203,  // MAV_CMD_DO_DIGICAM_CONTROL
    DoSetServo        = 183,  // MAV_CMD_DO_SET_SERVO
    DoRepeatServo     = 184,  // MAV_CMD_DO_REPEAT_SERVO
    ConditionDelay    = 112,  // MAV_CMD_CONDITION_DELAY
    ConditionDistance  = 114,  // MAV_CMD_CONDITION_DISTANCE
};

/// Mission transfer state machine
enum class MissionState : uint8_t {
    Idle,
    Downloading,         // Requesting items from FC
    Uploading,           // Sending items to FC
    WaitingAck,          // Waiting for final ACK
};

/// Single mission item (WAYPOINT)
struct MissionItem {
    uint16_t seq = 0;           // Sequence number (0-based)
    uint8_t  frame = 3;         // MAV_FRAME (default: GlobalRelAlt)
    uint16_t command = 0;       // MAV_CMD_xxx
    uint8_t  current = 0;       // 1 = current waypoint (set by FC)
    uint8_t  autocontinue = 1;  // 1 = auto-continue to next
    float    param1 = 0.0f;     // Command-specific param 1
    float    param2 = 0.0f;     // Command-specific param 2
    float    param3 = 0.0f;     // Command-specific param 3
    float    param4 = 0.0f;     // Command-specific param 4
    int32_t  x = 0;             // Latitude (degE7) or local x
    int32_t  y = 0;             // Longitude (degE7) or local y
    float    z = 0.0f;          // Altitude (meters)
    uint8_t  mission_type = 0;  // 0=mission, 1=geofence, 2=rally
};

/// Mission plan (collection of items)
struct MissionPlan {
    std::vector<MissionItem> items;
    uint16_t current_index = 0;  // Currently active waypoint
    bool     valid = false;      // True after successful download
    
    void clear() {
        items.clear();
        current_index = 0;
        valid = false;
    }
    
    size_t count() const { return items.size(); }
};

/// Transfer progress info
struct MissionTransferProgress {
    MissionState state = MissionState::Idle;
    uint16_t total = 0;         // Total items expected
    uint16_t received = 0;      // Items received so far (download)
    uint16_t sent = 0;          // Items sent so far (upload)
    uint8_t  result = 0;        // MAV_MISSION_RESULT on completion
    std::string error;          // Error message if failed
};

/// Provenance of the cached mission plan used for safe readback verification.
struct MissionPlanProvenance {
    uint64_t plan_revision = 0;
    uint64_t last_completed_download_revision = 0;
    uint64_t last_upload_revision = 0;
    uint64_t completed_download_count = 0;
};

struct MissionPlanSnapshot {
    MissionPlan plan;
    MissionPlanProvenance provenance;
};

/// Helper: get human-readable command name
inline const char* command_name(uint16_t cmd) {
    switch (cmd) {
        case 16:  return "NAV_WAYPOINT";
        case 17:  return "NAV_LOITER_UNLIM";
        case 18:  return "NAV_LOITER_TURNS";
        case 19:  return "NAV_LOITER_TIME";
        case 20:  return "NAV_RETURN_TO_LAUNCH";
        case 21:  return "NAV_LAND";
        case 22:  return "NAV_TAKEOFF";
        case 82:  return "NAV_SPLINE_WAYPOINT";
        case 93:  return "NAV_DELAY";
        case 178: return "DO_CHANGE_SPEED";
        case 183: return "DO_SET_SERVO";
        case 206: return "DO_SET_CAM_TRIGG_DIST";
        default:  return "UNKNOWN";
    }
}

/// QGC PlanManager-compatible text for MAV_MISSION_RESULT.
/// Values follow the MAVLink common enum and are kept local so this public
/// model header does not depend on generated MAVLink headers.
inline const char* mission_result_message(uint8_t result) {
    switch (result) {
        case 0:  return "飞控接受任务";
        case 1:  return "飞控返回未指定错误";
        case 2:  return "飞控不支持该坐标帧";
        case 3:  return "飞控不支持该任务命令";
        case 4:  return "飞控任务存储空间不足";
        case 5:  return "任务参数包含无效值";
        case 6:  return "任务参数 1 无效";
        case 7:  return "任务参数 2 无效";
        case 8:  return "任务参数 3 无效";
        case 9:  return "任务参数 4 无效";
        case 10: return "任务参数 5（X/纬度）无效";
        case 11: return "任务参数 6（Y/经度）无效";
        case 12: return "任务参数 7（Z/高度）无效";
        case 13: return "收到乱序任务项";
        case 14: return "飞控拒绝任务命令";
        case 15: return "任务操作已取消";
        default: return "未知任务错误";
    }
}

/// Helper: get human-readable frame name
inline const char* frame_name(uint8_t frame) {
    switch (frame) {
        case 0:  return "GLOBAL";
        case 1:  return "LOCAL_NED";
        case 2:  return "MISSION";
        case 3:  return "GLOBAL_REL_ALT";
        case 5:  return "GLOBAL_INT";
        case 6:  return "GLOBAL_REL_ALT_INT";
        case 10: return "GLOBAL_TERRAIN_ALT";
        case 11: return "GLOBAL_TERRAIN_ALT_INT";
        default: return "UNKNOWN";
    }
}

/// 该 MAV_FRAME 是否使用 x/y 表示全球经纬度（degE7）。
///
/// MAV_FRAME_MISSION（2）是例外：QGC/MAVLink 约定其 x/y 对应 param5/param6，
/// 不是坐标。把这类任务的 x/y 除以 1e7 会在回读航点和导入 QGC plan 时产生
/// 假航点/航线漂移。INT 系列（5/6/11）与普通帧的坐标语义相同。
inline bool is_global_coordinate_frame(uint8_t frame) {
    return frame == 0 || frame == 3 || frame == 5 ||
           frame == 6 || frame == 10 || frame == 11;
}

/// 该任务命令的 x/y 是否承载地理位置。
///
/// 对齐 ArduPilot `AP_Mission::stored_in_location()`：DO_/CONDITION_ 一类
/// 命令没有地理位置；飞控回读时即使把 frame 规范成 0，x/y 也不能当经纬度。
/// 固定翼 `NAV_TAKEOFF`（22）在 ArduPilot 里使用位置，必须保留坐标。
inline bool mission_command_uses_location(uint16_t command) {
    switch (command) {
        case 16:  // NAV_WAYPOINT
        case 17:  // NAV_LOITER_UNLIM
        case 18:  // NAV_LOITER_TURNS
        case 19:  // NAV_LOITER_TIME
        case 21:  // NAV_LAND
        case 22:  // NAV_TAKEOFF
        case 30:  // NAV_CONTINUE_AND_CHANGE_ALT
        case 31:  // NAV_LOITER_TO_ALT
        case 36:  // NAV_ARC_WAYPOINT
        case 82:  // NAV_SPLINE_WAYPOINT
        case 84:  // NAV_VTOL_TAKEOFF
        case 85:  // NAV_VTOL_LAND
        case 92:  // NAV_GUIDED_ENABLE
        case 94:  // NAV_PAYLOAD_PLACE
        case 179: // DO_SET_HOME
        case 188: // DO_RETURN_PATH_START
        case 189: // DO_LAND_START
        case 191: // DO_GO_AROUND
        case 195: // DO_SET_ROI_LOCATION
        case 201: // DO_SET_ROI
        case 5000: // NAV_FENCE_RETURN_POINT
        case 5001: // NAV_FENCE_POLYGON_VERTEX_INCLUSION
        case 5002: // NAV_FENCE_POLYGON_VERTEX_EXCLUSION
        case 5003: // NAV_FENCE_CIRCLE_INCLUSION
        case 5004: // NAV_FENCE_CIRCLE_EXCLUSION
        case 5100: // NAV_RALLY_POINT
            return true;
        default:
            return false;
    }
}

/// `mission_command_uses_location` 的固件覆盖版。
///
/// 上面那张表镜像 ArduPilot `AP_Mission::stored_in_location()`，是「飞控实际
/// 怎么存这个字节」的物理语义。QGC 另有一层编辑器语义——`MavCmdInfo*.json`
/// 的 `specifiesCoordinate` / `standaloneCoordinate`——PX4 覆盖了其中一条：
/// `MAV_CMD_DO_LAND_START`(189) 在 `PX4-MavCmdInfoCommon.json` 里两者都是
/// false，即 PX4 的 land start 是纯标记，x/y 不承载经纬度。若不按固件区分，
/// 从 PX4 回读 x=y=0 的 land start 会被当成 (0, 0) 的坐标画到地图上。
///
/// `firmware_profile` 为 `"px4"` 时应用该覆盖；空、`"ardupilot"` 或未知一律
/// 与 ArduPilot 语义一致（没有数据就不改判）。
inline bool mission_command_uses_location(uint16_t command,
                                          const std::string& firmware_profile) {
    if (firmware_profile == "px4" && command == 189) return false;
    return mission_command_uses_location(command);
}

} // namespace mg::core::mission

#endif // MGCORE_MISSION_TYPES_H
