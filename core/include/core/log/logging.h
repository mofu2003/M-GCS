/**
 * logging.h - 轻量级结构化日志系统
 *
 * Usage:
 *   #include "core/log/logging.h"
 *   MGC_LOG_INFO("[SerialLink] Opened %s @ %u baud", port, baud);
 *   MGC_LOG_ERROR("[Mavlink] Parse error on msg %u", msg_id);
 *   MGC_LOG_WARN("[Discovery] No FC ports found");
 *   MGC_LOG_DEBUG("[Adapter] Heartbeat from sysid %u", sysid);
 *
 * Design:
 *   - No external dependencies (no spdlog, no glog).
 *   - All output goes to stderr (can be redirected by the platform shell).
 *   - MGC_LOG_LEVEL controls compilation: 0=off, 1=error, 2=warn, 3=info, 4=debug.
 *   - Default level is 3 (info) for release, 4 (debug) for debug builds.
 */

#ifndef MGCORE_LOGGING_H
#define MGCORE_LOGGING_H

#include <cstdio>
#include <cstdarg>

#ifndef MGC_LOG_LEVEL
  #ifdef NDEBUG
    #define MGC_LOG_LEVEL 3   // info in release
  #else
    #define MGC_LOG_LEVEL 4   // debug in debug builds
  #endif
#endif

namespace mg::core::log {

inline void write_log(const char* level, const char* fmt, ...) {
    std::fprintf(stderr, "[%s] ", level);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

} // namespace mg::core::log

#define MGC_LOG_ERROR(...) \
    do { if (MGC_LOG_LEVEL >= 1) { mg::core::log::write_log("ERROR", __VA_ARGS__); } } while(0)

#define MGC_LOG_WARN(...) \
    do { if (MGC_LOG_LEVEL >= 2) { mg::core::log::write_log("WARN ", __VA_ARGS__); } } while(0)

#define MGC_LOG_INFO(...) \
    do { if (MGC_LOG_LEVEL >= 3) { mg::core::log::write_log("INFO ", __VA_ARGS__); } } while(0)

#define MGC_LOG_DEBUG(...) \
    do { if (MGC_LOG_LEVEL >= 4) { mg::core::log::write_log("DEBUG", __VA_ARGS__); } } while(0)

#endif /* MGCORE_LOGGING_H */
