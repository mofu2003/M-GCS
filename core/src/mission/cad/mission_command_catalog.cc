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
    waypoint.description = "Fly to a position and continue";
    waypoint.requires_position = true;
    waypoint.requires_altitude = true;
    waypoint.parameters = {
        parameter(1, "hold_time", "Hold time", "s", MissionParameterType::Float, 0.0, 0.0, 0.0, true),
        parameter(2, "accept_radius", "Acceptance radius", "m", MissionParameterType::Float, 2.0, 0.0, 0.0, true),
        parameter(3, "pass_radius", "Pass radius", "m", MissionParameterType::Float, 0.0, 0.0, 0.0, true),
        parameter(4, "yaw", "Yaw", "deg"),
    };
    catalog.push_back(std::move(waypoint));

    MissionCommandDescriptor takeoff;
    takeoff.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    takeoff.name = "NAV_TAKEOFF";
    takeoff.label = "起飞";
    takeoff.entity_type = mission_entity_type_for_command(takeoff.command);
    takeoff.category = "navigation";
    takeoff.description = "Take off to the configured altitude";
    takeoff.requires_altitude = true;
    takeoff.parameters = {
        parameter(1, "pitch", "Pitch", "deg", MissionParameterType::Float, 15.0),
        parameter(4, "yaw", "Yaw", "deg"),
    };
    catalog.push_back(std::move(takeoff));

    MissionCommandDescriptor land;
    land.command = static_cast<uint16_t>(NavCommand::NavLand);
    land.name = "NAV_LAND";
    land.label = "降落";
    land.entity_type = mission_entity_type_for_command(land.command);
    land.category = "navigation";
    land.description = "Land at a position";
    land.requires_position = true;
    land.requires_altitude = true;
    catalog.push_back(std::move(land));

    MissionCommandDescriptor rtl;
    rtl.command = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    rtl.name = "NAV_RETURN_TO_LAUNCH";
    rtl.label = "RTL 返航";
    rtl.entity_type = mission_entity_type_for_command(rtl.command);
    rtl.category = "navigation";
    rtl.description = "Return to launch";
    catalog.push_back(std::move(rtl));

    MissionCommandDescriptor loiter_turns;
    loiter_turns.command = static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    loiter_turns.name = "NAV_LOITER_TURNS";
    loiter_turns.label = "盘旋圈数";
    loiter_turns.entity_type = mission_entity_type_for_command(loiter_turns.command);
    loiter_turns.category = "navigation";
    loiter_turns.description = "Loiter for a number of turns";
    loiter_turns.requires_position = true;
    loiter_turns.requires_altitude = true;
    loiter_turns.parameters = {
        parameter(1, "turns", "Turns", "", MissionParameterType::Float, 3.0, 1.0, 0.0, true, false, true),
        parameter(2, "heading_required", "Heading required", "", MissionParameterType::Boolean, 0.0, 0.0, 1.0, true, true),
        parameter(3, "radius", "Radius", "m", MissionParameterType::Float, 50.0, 0.01, 0.0, true, false, true),
        parameter(4, "xtrack", "Xtrack", "m"),
    };
    catalog.push_back(std::move(loiter_turns));

    MissionCommandDescriptor loiter_time;
    loiter_time.command = static_cast<uint16_t>(NavCommand::NavLoiterTime);
    loiter_time.name = "NAV_LOITER_TIME";
    loiter_time.label = "定时盘旋";
    loiter_time.entity_type = mission_entity_type_for_command(loiter_time.command);
    loiter_time.category = "navigation";
    loiter_time.description = "Loiter at a position for a duration";
    loiter_time.requires_position = true;
    loiter_time.requires_altitude = true;
    loiter_time.parameters = {
        parameter(1, "time", "Time", "s", MissionParameterType::Float, 10.0, 0.0, 0.0, true, false, true),
        parameter(2, "heading_required", "Heading required", "", MissionParameterType::Boolean, 0.0, 0.0, 1.0, true, true),
        parameter(3, "radius", "Radius", "m", MissionParameterType::Float, 50.0, 0.01, 0.0, true, false, true),
        parameter(4, "xtrack", "Xtrack", "m"),
    };
    catalog.push_back(std::move(loiter_time));

    MissionCommandDescriptor speed;
    speed.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    speed.name = "DO_CHANGE_SPEED";
    speed.label = "改变速度";
    speed.entity_type = mission_entity_type_for_command(speed.command);
    speed.category = "command";
    speed.description = "Change mission speed";
    speed.parameters = {
        parameter(1, "speed_type", "Speed type", "", MissionParameterType::Enum, 0.0, 0.0, 3.0, true, true, true),
        parameter(2, "speed", "Speed", "m/s", MissionParameterType::Float, 10.0, 0.01, 0.0, true, false, true),
        parameter(3, "throttle", "Throttle", "%", MissionParameterType::Float, 0.0, 0.0, 100.0, true, true),
    };
    speed.parameters[0].options = {
        {0.0, "Airspeed"},
        {1.0, "Ground speed"},
        {2.0, "Climb rate"},
        {3.0, "Descent rate"},
    };
    catalog.push_back(std::move(speed));

    MissionCommandDescriptor camera;
    camera.command = static_cast<uint16_t>(NavCommand::DoSetCamTrigDist);
    camera.name = "DO_SET_CAM_TRIGG_DIST";
    camera.label = "相机触发距离";
    camera.entity_type = mission_entity_type_for_command(camera.command);
    camera.category = "camera";
    camera.description = "Trigger camera by distance";
    camera.parameters = {
        parameter(1, "distance", "Trigger distance", "m", MissionParameterType::Float, 30.0, 0.0, 0.0, true, false, true),
        parameter(2, "shutter", "Shutter"),
        parameter(3, "trigger", "Trigger"),
    };
    catalog.push_back(std::move(camera));

    MissionCommandDescriptor servo;
    servo.command = static_cast<uint16_t>(NavCommand::DoSetServo);
    servo.name = "DO_SET_SERVO";
    servo.label = "设置舵机";
    servo.entity_type = mission_entity_type_for_command(servo.command);
    servo.category = "command";
    servo.description = "Set a servo output";
    servo.parameters = {
        parameter(1, "servo", "Servo", "", MissionParameterType::Integer, 1.0, 1.0, 16.0, true, true, true),
        parameter(2, "pwm", "PWM", "us", MissionParameterType::Integer, 1500.0, 800.0, 2200.0, true, true, true),
    };
    catalog.push_back(std::move(servo));

    MissionCommandDescriptor delay;
    delay.command = static_cast<uint16_t>(NavCommand::ConditionDelay);
    delay.name = "CONDITION_DELAY";
    delay.label = "延时";
    delay.entity_type = mission_entity_type_for_command(delay.command);
    delay.category = "condition";
    delay.description = "Wait for a fixed duration";
    delay.parameters = {
        parameter(1, "delay", "Delay", "s", MissionParameterType::Float, 5.0, 0.0, 0.0, true, false, true),
    };
    catalog.push_back(std::move(delay));

    MissionCommandDescriptor distance;
    distance.command = static_cast<uint16_t>(NavCommand::ConditionDistance);
    distance.name = "CONDITION_DISTANCE";
    distance.label = "距离条件";
    distance.entity_type = mission_entity_type_for_command(distance.command);
    distance.category = "condition";
    distance.description = "Wait until a distance has been travelled";
    distance.parameters = {
        parameter(1, "distance", "Distance", "m", MissionParameterType::Float, 100.0, 0.0, 0.0, true, false, true),
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
