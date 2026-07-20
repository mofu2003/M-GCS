/**
 * terrain_query.h - Terrain elevation data query
 * 地形查询 — SRTM 高程数据下载，支持地形跟随任务
 *
 * Downloads SRTM/ASTER terrain data for terrain-following missions.
 * Uses Open Elevation API (free, no key required).
 *
 * Design: inspired by QGC TerrainTileManager.
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <functional>
#include "core/platform/platform_mutex.h"

namespace mg::core::terrain {

/// Terrain elevation sample
struct TerrainPoint {
    double lat_deg = 0;
    double lon_deg = 0;
    float  elevation_m = 0;  // Elevation above sea level
    bool   valid = false;
};

/// Terrain query result
struct TerrainResult {
    std::vector<TerrainPoint> points;
    bool   valid = false;
    std::string error;
};

/// Callback for async terrain queries
using TerrainCallback = std::function<void(const TerrainResult&)>;


/// Terrain tile cache with LRU eviction.
/// Tiles are keyed by a grid cell (lat/lon rounded to 0.01 degree = ~1km).
/// QGC precedent: TerrainTileManager (tile hash cache + database persistence).
/// Our implementation is simpler: in-memory only, no disk persistence yet.
class TerrainCache {
public:
    /// max_entries: maximum number of cached elevation points (default 10000)
    explicit TerrainCache(size_t max_entries = 10000);
    ~TerrainCache();

    /// Check if a coordinate is in the cache.
    bool contains(double lat_deg, double lon_deg) const;

    /// Get cached elevation. Returns nullopt if not cached.
    std::optional<float> get(double lat_deg, double lon_deg) const;

    /// Store an elevation value.
    void put(double lat_deg, double lon_deg, float elevation_m);

    /// Get cache hit rate (0.0 - 1.0).
    double hit_rate() const;

    /// Clear the cache.
    void clear();

    /// Get number of entries.
    size_t size() const;

private:
    /// Convert lat/lon to a grid key (rounded to 0.01 degree).
    static uint64_t to_key(double lat_deg, double lon_deg);

    mutable mg::core::platform::Mutex mtx_;
    std::unordered_map<uint64_t, float> cache_;
    size_t max_entries_;
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
};

class TerrainQuery {
public:
    TerrainQuery();
    ~TerrainQuery();

    /// Query elevation for a single point (synchronous).
    TerrainPoint query_elevation(double lat_deg, double lon_deg);

    /// Query elevation for multiple points (synchronous).
    TerrainResult query_batch(const std::vector<std::pair<double, double>>& points);

    /// Query elevation along a line (interpolated).
    TerrainResult query_line(double lat1, double lon1, double lat2, double lon2,
                             int num_samples = 10);

private:
    /// Fetch elevation from Open Elevation API.
    float fetch_elevation(double lat_deg, double lon_deg);

    mutable mg::core::platform::Mutex mtx_;
    TerrainCache cache_;
};

} // namespace mg::core::terrain
