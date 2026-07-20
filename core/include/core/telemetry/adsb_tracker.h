/**
 * adsb_tracker.h - ADS-B vehicle tracker for mgroundcontrol
 *
 * Tracks nearby aircraft received via ADS-B (Automatic Dependent
 * Surveillance-Broadcast). Vehicles that have not been heard from within
 * kExpireTimeoutMs are considered stale and pruned on the next query or
 * explicit expire_stale() call.
 *
 * Thread-safe: all public methods acquire an internal mutex.
 */

#ifndef MGCORE_ADSB_TRACKER_H
#define MGCORE_ADSB_TRACKER_H

#include "core/platform/platform_mutex.h"
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>

namespace mg::core::telemetry {

/// ADS-B transponder report for a single vehicle.
struct ADSBVehicle {
    uint32_t    icao_address  = 0;
    std::string callsign;
    double      lat           = 0;    // degrees
    double      lon           = 0;    // degrees
    float       alt_m         = 0;    // meters (altitude)
    float       heading       = 0;    // degrees
    float       hor_velocity  = 0;    // m/s
    float       ver_velocity  = 0;    // m/s
    uint16_t    squawk        = 0;
    uint8_t     emitter_type  = 0;
    uint8_t     tslc          = 0;    // time since last communication (seconds)
    uint16_t    flags         = 0;
    int64_t     last_update_ms = 0;   // timestamp of last update (steady clock)
};

/// Tracks a set of nearby ADS-B vehicles with automatic expiry.
class ADSBTracker {
public:
    static constexpr int kMaxVehicles     = 50;
    static constexpr int kExpireTimeoutMs = 15000;   // 15 seconds

    /// Update or add a tracked vehicle. If the ICAO address already exists
    /// the entry is overwritten; otherwise a new entry is appended (up to
    /// kMaxVehicles).
    void update(const ADSBVehicle& vehicle);

    /// Return a snapshot of all currently tracked (non-expired) vehicles.
    std::vector<ADSBVehicle> get_all() const;

    /// Return the number of tracked (non-expired) vehicles.
    size_t count() const;

    /// Remove vehicles whose last update is older than kExpireTimeoutMs.
    void expire_stale();

    /// Remove all tracked vehicles.
    void clear();

private:
    mutable mg::core::platform::Mutex mtx_;
    std::vector<ADSBVehicle>          vehicles_;

    /// Current time in milliseconds from steady_clock.
    int64_t now_ms() const;
};

} // namespace mg::core::telemetry

#endif // MGCORE_ADSB_TRACKER_H
