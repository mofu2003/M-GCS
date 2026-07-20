/**
 * terrain_query.cc - Terrain elevation cache and query scaffold
 * 地形查询实现 — 当前提供内存缓存和查询形状，HTTP 拉取尚未接入
 *
 * The production fetch path is reserved for an HTTP terrain provider such as
 * Open Elevation. Until that provider is wired, uncached points are reported
 * as invalid rather than returning fabricated terrain.
 *
 * QGC precedent: TerrainTileManager + TerrainAtCoordinateBatchManager
 *   - QGC uses tile-level caching with disk persistence
 *   - We use point-level caching in memory (simpler, sufficient for v1)
 *   - QGC batches requests with 500ms window; we do synchronous batch
 */

#include "core/terrain/terrain_query.h"
#include "core/log/logging.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mg::core::terrain {

// ================================================================
// TerrainCache — in-memory LRU tile cache
// ================================================================

TerrainCache::TerrainCache(size_t max_entries) : max_entries_(max_entries) {}
TerrainCache::~TerrainCache() = default;

uint64_t TerrainCache::to_key(double lat_deg, double lon_deg) {
    // Round to 0.01 degree grid (~1.1km at equator)
    int32_t lat_i = static_cast<int32_t>(std::round(lat_deg * 100.0));
    int32_t lon_i = static_cast<int32_t>(std::round(lon_deg * 100.0));
    return (static_cast<uint64_t>(static_cast<uint32_t>(lat_i)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(lon_i));
}

bool TerrainCache::contains(double lat_deg, double lon_deg) const {
    mg::core::platform::LockGuard lk(mtx_);
    return cache_.count(to_key(lat_deg, lon_deg)) > 0;
}

std::optional<float> TerrainCache::get(double lat_deg, double lon_deg) const {
    mg::core::platform::LockGuard lk(mtx_);
    auto it = cache_.find(to_key(lat_deg, lon_deg));
    if (it != cache_.end()) {
        hits_++;
        return it->second;
    }
    misses_++;
    return std::nullopt;
}

void TerrainCache::put(double lat_deg, double lon_deg, float elevation_m) {
    mg::core::platform::LockGuard lk(mtx_);
    if (cache_.size() >= max_entries_) {
        // Simple eviction: clear half the cache (LRU would need a linked list)
        // This is good enough for v1 — QGC's TerrainTileManager also does bulk eviction
        auto it = cache_.begin();
        std::advance(it, cache_.size() / 2);
        cache_.erase(cache_.begin(), it);
    }
    cache_[to_key(lat_deg, lon_deg)] = elevation_m;
}

double TerrainCache::hit_rate() const {
    mg::core::platform::LockGuard lk(mtx_);
    uint64_t total = hits_ + misses_;
    return total > 0 ? static_cast<double>(hits_) / total : 0.0;
}

void TerrainCache::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    cache_.clear();
    hits_ = 0;
    misses_ = 0;
}

size_t TerrainCache::size() const {
    mg::core::platform::LockGuard lk(mtx_);
    return cache_.size();
}

// ================================================================
// TerrainQuery — with cache integration
// ================================================================

TerrainQuery::TerrainQuery() = default;
TerrainQuery::~TerrainQuery() = default;

TerrainPoint TerrainQuery::query_elevation(double lat_deg, double lon_deg) {
    TerrainPoint pt;
    pt.lat_deg = lat_deg;
    pt.lon_deg = lon_deg;

    // Check cache first
    auto cached = cache_.get(lat_deg, lon_deg);
    if (cached.has_value()) {
        pt.elevation_m = cached.value();
        pt.valid = true;
        return pt;
    }

    // Cache miss: fetch from API
    float elev = fetch_elevation(lat_deg, lon_deg);
    if (elev > -1000) {
        pt.elevation_m = elev;
        pt.valid = true;
        cache_.put(lat_deg, lon_deg, elev);
    } else {
        pt.elevation_m = 0;
        pt.valid = false;
        MGC_LOG_WARN("[Terrain] No elevation data for %.6f, %.6f", lat_deg, lon_deg);
    }
    return pt;
}

TerrainResult TerrainQuery::query_batch(const std::vector<std::pair<double, double>>& points) {
    TerrainResult result;

    // Deduplicate coordinates (QGC TerrainAtCoordinateBatchManager pattern)
    // Collect unique uncached points, fetch in one batch, then assemble results
    std::vector<std::pair<double, double>> uncached;
    std::vector<size_t> uncached_indices;

    result.points.resize(points.size());

    for (size_t i = 0; i < points.size(); i++) {
        auto& [lat, lon] = points[i];
        auto cached = cache_.get(lat, lon);
        if (cached.has_value()) {
            result.points[i] = {lat, lon, cached.value(), true};
        } else {
            uncached.push_back({lat, lon});
            uncached_indices.push_back(i);
            // Placeholder for now
            result.points[i] = {lat, lon, 0.0f, false};
        }
    }

    // Fetch uncached points individually (in production, batch via HTTP API)
    for (size_t j = 0; j < uncached.size(); j++) {
        auto& [lat, lon] = uncached[j];
        float elev = fetch_elevation(lat, lon);
        size_t idx = uncached_indices[j];
        if (elev > -1000) {
            result.points[idx].elevation_m = elev;
            result.points[idx].valid = true;
            cache_.put(lat, lon, elev);
        }
    }

    result.valid = true;
    return result;
}

TerrainResult TerrainQuery::query_line(double lat1, double lon1, double lat2, double lon2,
                                        int num_samples) {
    // Generate coordinates along the line, then batch query
    if (num_samples < 2) num_samples = 2;

    std::vector<std::pair<double, double>> coords;
    coords.reserve(num_samples);
    for (int i = 0; i < num_samples; i++) {
        double t = static_cast<double>(i) / (num_samples - 1);
        coords.push_back({lat1 + t * (lat2 - lat1), lon1 + t * (lon2 - lon1)});
    }
    return query_batch(coords);
}

float TerrainQuery::fetch_elevation(double, double) {
    // Future provider: https://api.open-elevation.com/api/v1/lookup?locations=lat,lon
    return -1001.0f;
}

} // namespace mg::core::terrain
