#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mg::core::mission::cad {

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
    std::vector<MissionCommandOption> options;
};

struct MissionCommandDescriptor {
    uint16_t command = 0;
    std::string name;
    std::string category;
    std::string description;
    bool requires_position = false;
    bool requires_altitude = false;
    std::vector<MissionCommandParameter> parameters;
};

const std::vector<MissionCommandDescriptor>& mission_command_catalog();

} // namespace mg::core::mission::cad
