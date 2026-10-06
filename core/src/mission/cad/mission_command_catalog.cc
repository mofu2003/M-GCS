#include "core/mission/cad/mission_command_catalog.h"

#include "core/mission/cad/mission_document.h"

#include "core/mission/mission_types.h"

namespace mg::core::mission::cad {

namespace {

MissionCommandParameter parameter(
    uint8_t index,
    const char* name,
    const char* label,
    const char* unit = "",
    MissionParameterType type = MissionParameterType::Float,
    double default_value = 0.0,
    double minimum = 0.0,
    double maximum = 0.0,
    bool has_minimum = false,
    bool has_maximum = false,
    bool required = false) {
    MissionCommandParameter descriptor;
    descriptor.index = index;
    descriptor.name = name;
    descriptor.label = label;
    descriptor.unit = unit;
    descriptor.type = type;
    descriptor.default_value = default_value;
    descriptor.minimum = minimum;
    descriptor.maximum = maximum;
    descriptor.has_minimum = has_minimum;
    descriptor.has_maximum = has_maximum;
    descriptor.required = required;
    return descriptor;
}

std::vector<MissionCommandDescriptor> build_catalog() {
    std::vector<MissionCommandDescriptor> catalog;

    MissionCommandDescriptor waypoint;
    waypoint.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    waypoint.name = "NAV_WAYPOINT";
    waypoint.label = "航点";
    waypoint.entity_type = mission_entity_type_for_command(waypoint.command);
    waypoint.category = "navigation";
    waypoint.description = "飞往指定位置后继续执行";
    waypoint.requires_position = true;
    waypoint.requires_altitude = true;
    waypoint.parameters = {
        parameter(1, "hold_time", "停留时间", "s", MissionParameterType::Float, 0.0, 0.0, 0.0, true),
        parameter(2, "accept_radius", "接受半径", "m", MissionParameterType::Float, 2.0, 0.0, 0.0, true),
        parameter(3, "pass_radius", "通过半径", "m", MissionParameterType::Float, 0.0, 0.0, 0.0, true),
        parameter(4, "yaw", "偏航角", "deg"),
    };
    catalog.push_back(std::move(waypoint));

    MissionCommandDescriptor takeoff;
    takeoff.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    takeoff.name = "NAV_TAKEOFF";
    takeoff.label = "起飞";
    takeoff.entity_type = mission_entity_type_for_command(takeoff.command);
    takeoff.category = "navigation";
    takeoff.description = "起飞至设定高度";
    takeoff.requires_altitude = true;
    takeoff.parameters = {
        parameter(1, "pitch", "俯仰角", "deg", MissionParameterType::Float, 15.0),
        parameter(4, "yaw", "偏航角", "deg"),
    };
    catalog.push_back(std::move(takeoff));

    // VTOL 起飞/降落：命令来自官方 MAVLink（84/85），参数按 common.xml 的定义，
    // 不是我们自己发明的字段。QGC 也是按机型在 NAV_TAKEOFF / NAV_VTOL_TAKEOFF
    // 之间选（MissionController.cc: TakeoffMissionItem(vtol ? 84 : 22)）。
    MissionCommandDescriptor vtol_takeoff;
    vtol_takeoff.command = static_cast<uint16_t>(NavCommand::NavVtolTakeoff);
    vtol_takeoff.name = "NAV_VTOL_TAKEOFF";
    vtol_takeoff.label = "垂直起飞";
    vtol_takeoff.entity_type = mission_entity_type_for_command(vtol_takeoff.command);
    vtol_takeoff.category = "navigation";
    vtol_takeoff.description =
        "ArduPilot 在当前水平位置垂直起飞到设定高度，再转换为固定翼并飞向后续航点；"
        "Transition Heading 是 MAVLink 兼容字段，当前 ArduPilot 不按该参考坐标水平飞行";
    vtol_takeoff.requires_position = true;
    vtol_takeoff.requires_altitude = true;
    vtol_takeoff.parameters = {
        parameter(2, "transition_heading", "过渡航向（兼容）", "", MissionParameterType::Enum, 0.0, 0.0, 4.0, true, true),
        parameter(4, "yaw", "偏航角", "deg"),
    };
    vtol_takeoff.parameters[0].options = {
        {0.0, "载具默认"},
        {1.0, "朝下一航点"},
        {2.0, "沿起飞航向"},
        {3.0, "指定角度"},
        {4.0, "任意"},
    };
    catalog.push_back(std::move(vtol_takeoff));

    MissionCommandDescriptor vtol_land;
    vtol_land.command = static_cast<uint16_t>(NavCommand::NavVtolLand);
    vtol_land.name = "NAV_VTOL_LAND";
    vtol_land.label = "垂直降落";
    vtol_land.entity_type = mission_entity_type_for_command(vtol_land.command);
    vtol_land.category = "navigation";
    vtol_land.description = "固定翼进场后由多旋翼垂直降落";
    vtol_land.requires_position = true;
    vtol_land.requires_altitude = true;
    vtol_land.parameters = {
        parameter(1, "land_options", "降落方式", "", MissionParameterType::Enum, 0.0, 0.0, 2.0, true, true),
        parameter(3, "approach_altitude", "进场高度", "m"),
        parameter(4, "yaw", "偏航角", "deg"),
    };
    vtol_land.parameters[0].options = {
        {0.0, "飞控默认"},
        {1.0, "固定翼螺旋进场"},
        {2.0, "固定翼直线进场"},
    };
    catalog.push_back(std::move(vtol_land));

    MissionCommandDescriptor land;
    land.command = static_cast<uint16_t>(NavCommand::NavLand);
    land.name = "NAV_LAND";
    land.label = "降落";
    land.entity_type = mission_entity_type_for_command(land.command);
    land.category = "navigation";
    land.description = "在指定位置降落";
    land.requires_position = true;
    land.requires_altitude = true;
    catalog.push_back(std::move(land));

    MissionCommandDescriptor rtl;
    rtl.command = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    rtl.name = "NAV_RETURN_TO_LAUNCH";
    rtl.label = "RTL 返航";
    rtl.entity_type = mission_entity_type_for_command(rtl.command);
    rtl.category = "navigation";
    rtl.description = "返回起飞点";
    catalog.push_back(std::move(rtl));

    MissionCommandDescriptor loiter_turns;
    loiter_turns.command = static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    loiter_turns.name = "NAV_LOITER_TURNS";
    loiter_turns.label = "盘旋圈数";
    loiter_turns.entity_type = mission_entity_type_for_command(loiter_turns.command);
    loiter_turns.category = "navigation";
    loiter_turns.description = "在指定位置盘旋指定圈数";
    loiter_turns.requires_position = true;
    loiter_turns.requires_altitude = true;
    loiter_turns.parameters = {
        parameter(1, "turns", "盘旋圈数", "", MissionParameterType::Float, 3.0, 1.0, 0.0, true, false, true),
        parameter(2, "heading_required", "要求机头朝向", "", MissionParameterType::Boolean, 0.0, 0.0, 1.0, true, true),
        parameter(3, "radius", "盘旋半径", "m", MissionParameterType::Float, 50.0, 0.01, 0.0, true, false, true),
        parameter(4, "xtrack", "横向偏差", "m"),
    };
    catalog.push_back(std::move(loiter_turns));

    MissionCommandDescriptor loiter_time;
    loiter_time.command = static_cast<uint16_t>(NavCommand::NavLoiterTime);
    loiter_time.name = "NAV_LOITER_TIME";
    loiter_time.label = "定时盘旋";
    loiter_time.entity_type = mission_entity_type_for_command(loiter_time.command);
    loiter_time.category = "navigation";
    loiter_time.description = "在指定位置盘旋指定时间";
    loiter_time.requires_position = true;
    loiter_time.requires_altitude = true;
    loiter_time.parameters = {
        parameter(1, "time", "盘旋时间", "s", MissionParameterType::Float, 10.0, 0.0, 0.0, true, false, true),
        parameter(2, "heading_required", "要求机头朝向", "", MissionParameterType::Boolean, 0.0, 0.0, 1.0, true, true),
        parameter(3, "radius", "盘旋半径", "m", MissionParameterType::Float, 50.0, 0.01, 0.0, true, false, true),
        parameter(4, "xtrack", "横向偏差", "m"),
    };
    catalog.push_back(std::move(loiter_time));

    MissionCommandDescriptor speed;
    speed.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    speed.name = "DO_CHANGE_SPEED";
    speed.label = "改变速度";
    speed.entity_type = mission_entity_type_for_command(speed.command);
    speed.category = "command";
    speed.description = "修改任务飞行速度";
    speed.parameters = {
        parameter(1, "speed_type", "速度类型", "", MissionParameterType::Enum, 0.0, 0.0, 3.0, true, true, true),
        parameter(2, "speed", "速度", "m/s", MissionParameterType::Float, 10.0, 0.01, 0.0, true, false, true),
        parameter(3, "throttle", "油门", "%", MissionParameterType::Float, 0.0, 0.0, 100.0, true, true),
    };
    speed.parameters[0].options = {
        {0.0, "空速"},
        {1.0, "地速"},
        {2.0, "爬升率"},
        {3.0, "下降率"},
    };
    catalog.push_back(std::move(speed));

    MissionCommandDescriptor camera;
    camera.command = static_cast<uint16_t>(NavCommand::DoSetCamTrigDist);
    camera.name = "DO_SET_CAM_TRIGG_DIST";
    camera.label = "相机触发距离";
    camera.entity_type = mission_entity_type_for_command(camera.command);
    camera.category = "camera";
    camera.description = "按飞行距离触发相机";
    camera.parameters = {
        parameter(1, "distance", "触发距离", "m", MissionParameterType::Float, 30.0, 0.0, 0.0, true, false, true),
        parameter(2, "shutter", "快门"),
        parameter(3, "trigger", "触发"),
    };
    catalog.push_back(std::move(camera));

    MissionCommandDescriptor servo;
    servo.command = static_cast<uint16_t>(NavCommand::DoSetServo);
    servo.name = "DO_SET_SERVO";
    servo.label = "设置舵机";
    servo.entity_type = mission_entity_type_for_command(servo.command);
    servo.category = "command";
    servo.description = "设置舵机输出";
    servo.parameters = {
        parameter(1, "servo", "舵机通道", "", MissionParameterType::Integer, 1.0, 1.0, 16.0, true, true, true),
        parameter(2, "pwm", "脉宽值", "us", MissionParameterType::Integer, 1500.0, 800.0, 2200.0, true, true, true),
    };
    catalog.push_back(std::move(servo));

    MissionCommandDescriptor delay;
    delay.command = static_cast<uint16_t>(NavCommand::ConditionDelay);
    delay.name = "CONDITION_DELAY";
    delay.label = "延时";
    delay.entity_type = mission_entity_type_for_command(delay.command);
    delay.category = "condition";
    delay.description = "等待指定时间";
    delay.parameters = {
        parameter(1, "delay", "延时", "s", MissionParameterType::Float, 5.0, 0.0, 0.0, true, false, true),
    };
    catalog.push_back(std::move(delay));

    MissionCommandDescriptor distance;
    distance.command = static_cast<uint16_t>(NavCommand::ConditionDistance);
    distance.name = "CONDITION_DISTANCE";
    distance.label = "距离条件";
    distance.entity_type = mission_entity_type_for_command(distance.command);
    distance.category = "condition";
    distance.description = "等待飞行距离达到设定值";
    distance.parameters = {
        parameter(1, "distance", "距离", "m", MissionParameterType::Float, 100.0, 0.0, 0.0, true, false, true),
    };
    catalog.push_back(std::move(distance));

    return catalog;
}

} // namespace

const std::vector<MissionCommandDescriptor>& mission_command_catalog() {
    static const std::vector<MissionCommandDescriptor> catalog = build_catalog();
    return catalog;
}

} // namespace mg::core::mission::cad
