#ifndef MGCORE_SENSOR_DECODE_JSON_SERIALIZERS_H
#define MGCORE_SENSOR_DECODE_JSON_SERIALIZERS_H

#include "json_builder.h"

namespace mg::bridge {

inline std::string sensor_decode_device_json(
    const std::string& name, const std::string& sensor_type,
    const std::string& family, int bus,
    int address, int devtype, bool valid, int device_id = 0)
{
    JsonBuilder jb;
    jb.begin_object();
    if (!valid) {
        jb.field("device_id", device_id);
        jb.field("hint", "Unknown sensor. Report device_id to update database.");
    }
    jb.field("name",        name);
    jb.field("sensor_type", sensor_type);
    jb.field("family",      family);
    jb.field("bus",         bus);
    jb.field("address",     address);
    jb.field("devtype",     devtype);
    jb.field("valid",       valid);
    jb.end_object();
    return jb.str();
}

inline std::string sensor_decode_type_json(
    const std::string& name, const std::string& family, bool valid)
{
    JsonBuilder jb;
    jb.begin_object();
    jb.field("name",   name);
    jb.field("family", family);
    jb.field("valid",  valid);
    jb.end_object();
    return jb.str();
}

} // namespace mg::bridge

#endif /* MGCORE_SENSOR_DECODE_JSON_SERIALIZERS_H */
