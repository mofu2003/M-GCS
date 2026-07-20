/**
 * telemetry_ring_buffer.h - Circular buffer for telemetry history
 *
 * Stores time-series telemetry data (altitude, speed, battery, etc.)
 * for real-time charting and post-flight analysis.
 *
 * Design: fixed-size ring buffer, oldest data overwritten when full.
 * Thread-safe for concurrent write (from telemetry callback) and
 * read (from UI polling).
 */
#pragma once

#include "core/platform/platform_mutex.h"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace mg::core::telemetry {

/// Single telemetry data point
struct TelemetryPoint {
    uint64_t timestamp_ms = 0;   // Milliseconds since session start
    float value = 0.0f;
};

/// Telemetry data series (e.g., "altitude" over time)
struct TelemetrySeries {
    std::string name;            // Series name (e.g., "altitude", "speed")
    std::string unit;            // Unit string (e.g., "m", "m/s", "V")
    std::vector<TelemetryPoint> points;
    size_t max_size = 1000;     // Max points before oldest is overwritten
    size_t head = 0;            // Write position
    size_t count = 0;           // Number of valid points
};

/// Ring buffer manager for multiple telemetry series
class TelemetryRingBuffer {
public:
    /// max_points: maximum data points per series (default 1000 = ~10s at 100Hz)
    explicit TelemetryRingBuffer(size_t max_points = 1000);
    ~TelemetryRingBuffer();

    // Non-copyable
    TelemetryRingBuffer(const TelemetryRingBuffer&) = delete;
    TelemetryRingBuffer& operator=(const TelemetryRingBuffer&) = delete;

    /// Register a telemetry series
    void register_series(const std::string& name, const std::string& unit);

    /// Append a data point to a series
    void append(const std::string& name, uint64_t timestamp_ms, float value);

    /// Get all data points for a series (thread-safe snapshot)
    std::vector<TelemetryPoint> get_series(const std::string& name) const;

    /// Get the last N data points for a series
    std::vector<TelemetryPoint> get_series(const std::string& name, size_t last_n) const;

    /// Get the latest value for a series
    float get_latest(const std::string& name) const;

    /// Get the min/max values for a series
    void get_range(const std::string& name, float& min_val, float& max_val) const;

    /// Get all registered series names
    std::vector<std::string> get_series_names() const;

    /// Get data as JSON for a series (for FFI)
    std::string to_json(const std::string& name) const;

    /// Get summary statistics for a series
    std::string stats_json(const std::string& name) const;

    /// Clear all data
    void clear();

    /// Clear a specific series
    void clear_series(const std::string& name);

    /// Get total data points across all series
    size_t total_points() const;

private:
    mutable mg::core::platform::Mutex mtx_;
    size_t max_points_;
    std::vector<TelemetrySeries> series_;

    TelemetrySeries* find_series(const std::string& name);
    const TelemetrySeries* find_series(const std::string& name) const;
};

} // namespace mg::core::telemetry
