#define MGCORE_EXPORTS
/**
 * ffi_health.cc - Section 6: Connection Health (Heartbeat + State)
 *
 * 连接健康状态：心跳检测、连接存活判断。
 *
 * mg_connection_get_heartbeat_age, mg_connection_check_heartbeat
 */
#include "ffi_internal.h"

MGCORE_API char* mg_connection_get_heartbeat_age(mg_core_t* core) {
    CHECK_CORE(core);
    uint64_t age = get_active_conn(core).heartbeat_age_ms();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("age_ms", (int64_t)age);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_connection_check_heartbeat(mg_core_t* core, int timeout_ms) {
    CHECK_CORE(core);
    uint64_t age = get_active_conn(core).heartbeat_age_ms();
    bool alive = get_active_conn(core).heartbeat_alive(static_cast<uint64_t>(timeout_ms));
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("alive",  alive);
    jb.field("age_ms", (int64_t)age);
    jb.end_object();
    return to_cstr(jb.str());
}
