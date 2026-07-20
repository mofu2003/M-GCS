/**
 * adsb_tracker.cc - ADS-B vehicle tracker implementation
 * ADS-B 追踪器 — 周边飞行器位置跟踪与过期清理
 */
#include "core/telemetry/adsb_tracker.h"
#include "core/log/logging.h"
#include <algorithm>

namespace mg::core::telemetry {

// ---------------------------------------------------------------------------
// update() - insert or replace a vehicle keyed by ICAO address
// ---------------------------------------------------------------------------
void ADSBTracker::update(const ADSBVehicle& vehicle) {
    mg::core::platform::LockGuard lk(mtx_);

    // Look for an existing entry with the same ICAO address.
    for (auto& v : vehicles_) {
        if (v.icao_address == vehicle.icao_address) {
            v = vehicle;
            v.last_update_ms = now_ms();
            MGC_LOG_DEBUG("[ADSBTracker] Updated ICAO 0x%06X", vehicle.icao_address);
            return;
        }
    }

    // Not found - insert if we have room.
    if (static_cast<int>(vehicles_.size()) >= kMaxVehicles) {
        MGC_LOG_WARN("[ADSBTracker] Max vehicles (%d) reached, dropping ICAO 0x%06X",
                     kMaxVehicles, vehicle.icao_address);
        return;
    }

    ADSBVehicle entry = vehicle;
    entry.last_update_ms = now_ms();
    vehicles_.push_back(std::move(entry));

    MGC_LOG_DEBUG("[ADSBTracker] Added ICAO 0x%06X (total %zu)",
                  vehicle.icao_address, vehicles_.size());
}

// ---------------------------------------------------------------------------
// get_all() - return snapshot of non-expired vehicles
// ---------------------------------------------------------------------------
std::vector<ADSBVehicle> ADSBTracker::get_all() const {
    mg::core::platform::LockGuard lk(mtx_);

    const int64_t now = now_ms();
    std::vector<ADSBVehicle> result;
    result.reserve(vehicles_.size());

    for (const auto& v : vehicles_) {
        if ((now - v.last_update_ms) <= kExpireTimeoutMs) {
            result.push_back(v);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// count() - number of non-expired vehicles
// ---------------------------------------------------------------------------
size_t ADSBTracker::count() const {
    mg::core::platform::LockGuard lk(mtx_);

    const int64_t now = now_ms();
    size_t n = 0;

    for (const auto& v : vehicles_) {
        if ((now - v.last_update_ms) <= kExpireTimeoutMs) {
            ++n;
        }
    }

    return n;
}

// ---------------------------------------------------------------------------
// expire_stale() - remove vehicles that have gone quiet
// ---------------------------------------------------------------------------
void ADSBTracker::expire_stale() {
    mg::core::platform::LockGuard lk(mtx_);

    const int64_t now     = now_ms();
    const size_t  before  = vehicles_.size();

    vehicles_.erase(
        std::remove_if(vehicles_.begin(), vehicles_.end(),
                        [&](const ADSBVehicle& v) {
                            return (now - v.last_update_ms) > kExpireTimeoutMs;
                        }),
        vehicles_.end());

    const size_t removed = before - vehicles_.size();
    if (removed > 0) {
        MGC_LOG_DEBUG("[ADSBTracker] Expired %zu stale vehicle(s), %zu remaining",
                      removed, vehicles_.size());
    }
}

// ---------------------------------------------------------------------------
// clear()
// ---------------------------------------------------------------------------
void ADSBTracker::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    vehicles_.clear();
    MGC_LOG_DEBUG("[ADSBTracker] Cleared all vehicles");
}

// ---------------------------------------------------------------------------
// now_ms() - steady_clock milliseconds
// ---------------------------------------------------------------------------
int64_t ADSBTracker::now_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace mg::core::telemetry
