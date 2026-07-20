#ifndef MGCORE_PARAM_JSON_SERIALIZERS_H
#define MGCORE_PARAM_JSON_SERIALIZERS_H

#include "json_builder.h"
#include "core/mavlink/mavlink_adapter.h"

namespace mg::bridge {

inline std::string param_json(const mg::core::mavlink::ParamInfo& p) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("name",  p.name);
    jb.field("value", p.value);
    jb.field("type",  static_cast<int>(p.type));
    jb.field("index", static_cast<int>(p.index));
    jb.field("count", static_cast<int>(p.count));
    jb.end_object();
    return jb.str();
}

inline std::string param_detail_json(const mg::core::mavlink::ParamInfo& p, bool found = true) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("found", found);
    if (found) {
        jb.field("name",  p.name);
        jb.field("value", p.value);
        jb.field("type",  static_cast<int>(p.type));
        jb.field("index", static_cast<int>(p.index));
        jb.field("count", static_cast<int>(p.count));
    }
    jb.end_object();
    return jb.str();
}

inline std::string param_not_found_json() {
    return R"({"found":false})";
}

inline std::string param_progress_json(uint16_t received, uint16_t total) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("received", static_cast<int>(received));
    jb.field("total",    static_cast<int>(total));
    jb.end_object();
    return jb.str();
}

} // namespace mg::bridge

#endif /* MGCORE_PARAM_JSON_SERIALIZERS_H */
