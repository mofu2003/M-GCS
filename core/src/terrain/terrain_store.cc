/**
 * terrain_store.cc - Terrain elevation cache and line sampling
 *
 * 无网络实现：高程由前端写入，本文件只做缓存与剖面采样。
 * QGC 的 TerrainTileManager 自带网络与磁盘缓存；M-GCS core 只保留缓存部分，
 * 网络在 Flutter 层（见 terrain_store.h 的说明）。
 */
#include "core/terrain/terrain_store.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace mg::core::terrain {

// ================================================================
// TerrainCache - in-memory tile cache
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
        // Bulk eviction: drop the first half. QGC's TerrainTileManager also
        // evicts in bulk rather than maintaining per-entry LRU order.
        auto it = cache_.begin();
        std::advance(it, static_cast<long>(cache_.size() / 2));
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
// TerrainStore
// ================================================================

TerrainStore::TerrainStore(size_t max_entries) : cache_(max_entries) {}
TerrainStore::~TerrainStore() = default;

void TerrainStore::store(double lat_deg, double lon_deg, float elevation_m) {
    cache_.put(lat_deg, lon_deg, elevation_m);
}

size_t TerrainStore::store_batch(const std::vector<TerrainPoint>& points) {
    size_t stored = 0;
    for (const auto& pt : points) {
        if (!pt.valid) continue;
        cache_.put(pt.lat_deg, pt.lon_deg, pt.elevation_m);
        ++stored;
    }
    return stored;
}

std::optional<float> TerrainStore::lookup(double lat_deg, double lon_deg) const {
    return cache_.get(lat_deg, lon_deg);
}

TerrainProfile TerrainStore::sample_line(double lat1, double lon1, double lat2, double lon2,
                                         int num_samples) const {
    TerrainProfile profile;
    if (num_samples < 2) num_samples = 2;
    profile.points.reserve(static_cast<size_t>(num_samples));
    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(num_samples - 1);
        TerrainPoint pt;
        pt.lat_deg = lat1 + t * (lat2 - lat1);
        pt.lon_deg = lon1 + t * (lon2 - lon1);
        auto cached = cache_.get(pt.lat_deg, pt.lon_deg);
        if (cached.has_value()) {
            pt.elevation_m = cached.value();
            pt.valid = true;
        } else {
            profile.missing.push_back(static_cast<size_t>(i));
        }
        profile.points.push_back(pt);
    }
    return profile;
}

void TerrainStore::clear() { cache_.clear(); }
size_t TerrainStore::size() const { return cache_.size(); }
double TerrainStore::hit_rate() const { return cache_.hit_rate(); }

} // namespace mg::core::terrain
