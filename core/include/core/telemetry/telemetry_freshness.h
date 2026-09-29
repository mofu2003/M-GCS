#ifndef MGCORE_TELEMETRY_FRESHNESS_H
#define MGCORE_TELEMETRY_FRESHNESS_H

#include "core/telemetry/json_builder.h"

#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <string>

namespace mg::core::telemetry {

enum class TelemetryType : uint8_t {
    kHeartbeat,
    kSysStatus,
    kGps,
    kAttitude,
    kVfrHud,
    kImu,
    kPressure,
    kFirmwareVersion,
    kRcChannels,
    kServoOutput,
    kStatustext,
    kGlobalPosition,
    kHomePosition,
    kBattery,
    kHighresImu,
    kExtendedSysState,
    kPrearmCheck,
    kVibration,
    kEscStatus,
    kWindEstimate,
    kCount,
};

constexpr size_t kTelemetryTypeCount = static_cast<size_t>(TelemetryType::kCount);

class TelemetryFreshness {
public:
    void mark_updated(TelemetryType type) {
        last_update_ms_[index(type)] = now_ms();
    }

    bool is_fresh(TelemetryType type, uint32_t stale_ms = 3000) const {
        const auto updated_at = last_update_ms_[index(type)];
        if (updated_at == 0) return false;
        return (now_ms() - updated_at) <= stale_ms;
    }

    uint32_t age_ms(TelemetryType type) const {
        const auto updated_at = last_update_ms_[index(type)];
        if (updated_at == 0) return UINT32_MAX;
        return static_cast<uint32_t>(now_ms() - updated_at);
    }

    std::string to_json() const {
        JsonBuilder jb;
        jb.begin_object();
        append(jb, "heartbeat", TelemetryType::kHeartbeat);
        append(jb, "gps", TelemetryType::kGps);
        append(jb, "attitude", TelemetryType::kAttitude);
        append(jb, "sys_status", TelemetryType::kSysStatus);
        append(jb, "battery", TelemetryType::kBattery);
        append(jb, "vfr_hud", TelemetryType::kVfrHud);
        append(jb, "rc_channels", TelemetryType::kRcChannels);
        append(jb, "servo_output", TelemetryType::kServoOutput);
        append(jb, "global_position", TelemetryType::kGlobalPosition);
        append(jb, "vibration", TelemetryType::kVibration);
        append(jb, "esc_status", TelemetryType::kEscStatus);
        append(jb, "wind_estimate", TelemetryType::kWindEstimate);
        append(jb, "prearm_check", TelemetryType::kPrearmCheck);
        jb.end_object();
        return jb.str();
    }

private:
    static constexpr size_t index(TelemetryType type) {
        return static_cast<size_t>(type);
    }

    static uint64_t now_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void append(JsonBuilder& jb, const char* name, TelemetryType type) const {
        const auto age = age_ms(type);
        jb.field(name, static_cast<int64_t>(age));
        jb.field((std::string(name) + "_fresh").c_str(), age < 3000);
    }

    std::array<uint64_t, kTelemetryTypeCount> last_update_ms_{};
};

} // namespace mg::core::telemetry

#endif /* MGCORE_TELEMETRY_FRESHNESS_H */
