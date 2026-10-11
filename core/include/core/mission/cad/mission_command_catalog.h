#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mg::core::mission::cad {

/// ArduPilot 机型类别（与 heartbeat 的 MAV_TYPE 归一化结果一致，见
/// `ffi_ardupilot_vehicle_class` 和 QGC `QGCMAVLink::VehicleClass_t`）。
///
/// 一个命令的 `MissionCommandDescriptor::vehicles` 是该 bit mask 的子集，
/// 表示哪些机型在任务里真正执行这条命令。未建模的机型（例如 Sub、
/// AntennaTracker）不参与判断，避免用猜测拒绝一条本来能跑的任务。
namespace mission_vehicle {
inline constexpr uint16_t kCopter  = 1u << 0;
inline constexpr uint16_t kPlane   = 1u << 1;
inline constexpr uint16_t kVtol    = 1u << 2;
inline constexpr uint16_t kRover   = 1u << 3;
inline constexpr uint16_t kSub     = 1u << 4;
inline constexpr uint16_t kTracker = 1u << 5;

/// 仅用于 `MissionCommandParameter::vehicles`：显式表示「ArduPilot 的任何机型都不
/// 存储这个字段」。例如 `MAV_CMD_NAV_WAYPOINT` 的 param4（yaw）——ArduPilot 的
/// 任务结构里没有航点偏航位，偏航由 `WP_YAW_BEHAVIOR` 之类的机型参数决定；
/// 而 PX4 会存 yaw。机型归一化结果为空（PX4 或未知固件）时仍按「有效」处理，
/// 因此这个位只影响 ArduPilot 机型。
inline constexpr uint16_t kArduPilotNone = 1u << 15;
} // namespace mission_vehicle

enum class MissionParameterType : uint8_t {
    Float = 0,
    Integer = 1,
    Boolean = 2,
    Enum = 3,
};

struct MissionCommandOption {
    double value = 0.0;
    std::string label;
};

struct MissionCommandParameter {
    uint8_t index = 1;
    std::string name;
    std::string label;
    std::string unit;
    MissionParameterType type = MissionParameterType::Float;
    double default_value = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    bool has_minimum = false;
    bool has_maximum = false;
    bool required = false;
    /// 该参数在哪些机型上会被飞控存储并回读（`mission_vehicle::k*` 的并集）。
    ///
    /// - `0`：未标注，所有机型都当成有效——「没有数据就不判」。
    /// - `kArduPilotNone`：ArduPilot 的每个机型都不存储该字段（PX4 等未建模固件
    ///   仍然按有效处理）。
    ///
    /// 用途见 `mission_command_parameter_applies`：核对回读时，连接机型不存储的
    /// 字段只记为 normalization，不再当成差异。
    uint16_t vehicles = 0;
    std::vector<MissionCommandOption> options;
};

struct MissionCommandDescriptor {
    uint16_t command = 0;
    std::string name;
    /// 中文显示名（前端下拉/卡片用）；`name` 保留 MAV_CMD 的符号名。
    std::string label;
    /// 该命令在任务文档里对应的实体类型（waypoint/takeoff/land/rtl/loiter/command）。
    /// 前端新建实体时直接用它，避免自己维护一份命令→类型映射。
    std::string entity_type;
    std::string category;
    std::string description;
    bool requires_position = false;
    bool requires_altitude = false;
    /// 支持该命令的机型 bit mask（`mission_vehicle::k*` 的并集）。
    /// 0 表示“未标注”，调用方不得据此拒绝任务。
    uint16_t vehicles = 0;
    std::vector<MissionCommandParameter> parameters;
};

const std::vector<MissionCommandDescriptor>& mission_command_catalog();

/// 把归一化机型名转成 `mission_vehicle::k*` bit；未知或空名返回 0。
/// 接受 "fixedwing"/"plane"、"multirotor"/"copter" 等别名。
uint16_t mission_vehicle_class_bit(const std::string& vehicle_class);

/// 归一化机型名（供诊断信息使用）；未知时返回原字符串。
std::string mission_vehicle_class_label(const std::string& vehicle_class);

/// 命令是否支持指定机型。
///
/// - `vehicle_class` 为空、无法归一化，或归一到能力表未标注的机型（Sub/Tracker，
///   目录里没有任何命令引用它们的位）时返回 true，即“没有数据就不拦”，由调用方
///   决定是否再用其它信息判断。
/// - 命令不在目录里返回 true（是否支持该命令由 `entity.command_unsupported` 判定）。
bool mission_command_supports_vehicle(
    uint16_t command,
    const std::string& vehicle_class);

/// 参数是否对所连接机型有存储语义（决定回读核对时报 difference 还是
/// normalization）。
///
/// - 命令或参数不在目录里返回 true。
/// - `MissionCommandParameter::vehicles == 0`（未标注）返回 true。
/// - `vehicle_class` 为空或无法归一化（PX4、未知固件）时返回 true，不做判断。
/// - 其余情况按 `vehicles` 位掩码判断；`kArduPilotNone` 只对 ArduPilot 机型生效。
bool mission_command_parameter_applies(
    uint16_t command,
    uint8_t parameter_index,
    const std::string& vehicle_class);

/// 该参数是否应在给定固件的任务编辑器里显示（新建/编辑命令时）。
///
/// 与 `mission_command_parameter_applies` 的分工不同：后者判断「回读核对时该字段
/// 是否应报 difference」（按 ArduPilot 机型），本函数判断「前端是否显示该字段」
/// （按固件）。依据是 QGC 各固件的 `MavCmdInfo` `paramRemove`：
///
/// - PX4（`PX4-MavCmdInfoCommon.json`）隐藏 `NAV_WAYPOINT` 的 param2/3
///   （接受半径/通过半径）、`NAV_LAND` 的 param1（中止高度）、
///   `MAV_CMD_VIDEO_START_CAPTURE` 的 param2（状态频率）；
/// - ArduPilot 及未知固件不隐藏任何字段。
///
/// `firmware_profile` 取任务文档的 `metadata.firmware_profile`（`"px4"`/
/// `"ardupilot"`/空）。隐藏后前端新建的实体不会写入该字段，与 QGC 一致（QGC 只从
/// 编辑器移除，不修改已存在的值）。
bool mission_command_parameter_visible(
    uint16_t command,
    uint8_t parameter_index,
    const std::string& firmware_profile);

} // namespace mg::core::mission::cad
