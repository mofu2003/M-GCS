/**
 * terrain_store.h - Terrain elevation store (cache + line sampling)
 *
 * 地形高程存储。core 不内置 HTTPS 客户端：目标平台 macOS/Linux/Windows/
 * Android 的 TLS 方案各不相同，把它们塞进 core 会新增平台依赖（见
 * docs/constraints/architecture.md A6）。因此高程数据由前端从 provider 拉取后，
 * 经 `mg_terrain_cache_update` 写入本存储；单点查询与航线剖面采样在这里完成，
 * 缓存只有这一个权威来源，provider 可替换。
 *
 * Design: inspired by QGC TerrainTileManager (tile cache), without the network.
 */
#pragma once

#include "core/platform/platform_mutex.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mg::core::terrain {

/// Terrain elevation sample.
struct TerrainPoint {
    double lat_deg = 0;
    double lon_deg = 0;
    float  elevation_m = 0;  // meters above sea level
    bool   valid = false;    // false = elevation unknown
};

/// A sampled line profile. `missing` lists the indices in `points` whose
/// elevation is not cached yet; the caller fetches those and refills.
struct TerrainProfile {
    std::vector<TerrainPoint> points;
    std::vector<size_t>       missing;

    bool complete() const { return missing.empty(); }
};

/// Elevation cache keyed by a 0.01 degree grid cell (~1.1 km at the equator).
/// Bulk-evicts when full; not persisted to disk.
class TerrainCache {
public:
    /// max_entries: maximum number of cached elevation cells (default 10000).
    explicit TerrainCache(size_t max_entries = 10000);
    ~TerrainCache();

    bool contains(double lat_deg, double lon_deg) const;

    /// Cached elevation. Returns nullopt if not cached (counts a miss).
    std::optional<float> get(double lat_deg, double lon_deg) const;

    void put(double lat_deg, double lon_deg, float elevation_m);

    /// Cache hit rate over get() calls (0.0 - 1.0).
    double hit_rate() const;

    void clear();
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

/// Terrain elevation store consumed by the FFI layer. One instance lives on
/// `mg_core_t`, so the cache is shared across every FFI call on that core.
class TerrainStore {
public:
    explicit TerrainStore(size_t max_entries = 10000);
    ~TerrainStore();

    /// Cache one elevation.
    void store(double lat_deg, double lon_deg, float elevation_m);

    /// Cache many points; points with `valid == false` are skipped.
    /// Returns the number of points actually stored.
    size_t store_batch(const std::vector<TerrainPoint>& points);

    /// Cached elevation for a coordinate, if any.
    std::optional<float> lookup(double lat_deg, double lon_deg) const;

    /// Sample `num_samples` points along the lat/lon straight line and report
    /// which samples have no cached elevation.
    TerrainProfile sample_line(double lat1, double lon1, double lat2, double lon2,
                               int num_samples) const;

    void clear();
    size_t size() const;
    double hit_rate() const;

private:
    TerrainCache cache_;
};

} // namespace mg::core::terrain
