/**
 * telemetry_ring_buffer.cc - Telemetry ring buffer implementation
 * 遥测环形缓冲区 — 固定容量时序数据存储
 */
#include "core/telemetry/telemetry_ring_buffer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace mg::core::telemetry {

TelemetryRingBuffer::TelemetryRingBuffer(size_t max_points)
    : max_points_(max_points) {}

TelemetryRingBuffer::~TelemetryRingBuffer() = default;

void TelemetryRingBuffer::register_series(const std::string& name, const std::string& unit) {
    mg::core::platform::LockGuard lk(mtx_);

    // Check if already exists
    for (const auto& s : series_) {
        if (s.name == name) return;
    }

    TelemetrySeries series;
    series.name = name;
    series.unit = unit;
    series.max_size = max_points_;
    series.points.resize(max_points_);
    series_.push_back(series);
}

void TelemetryRingBuffer::append(const std::string& name, uint64_t timestamp_ms, float value) {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series) return;

    series->points[series->head] = {timestamp_ms, value};
    series->head = (series->head + 1) % series->max_size;
    if (series->count < series->max_size) series->count++;
}

std::vector<TelemetryPoint> TelemetryRingBuffer::get_series(const std::string& name) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series) return {};

    std::vector<TelemetryPoint> result;
    result.reserve(series->count);

    if (series->count < series->max_size) {
        // Buffer not full, read from 0 to count
        for (size_t i = 0; i < series->count; i++) {
            result.push_back(series->points[i]);
        }
    } else {
        // Buffer full, read from head (oldest) to head-1 (newest)
        for (size_t i = 0; i < series->max_size; i++) {
            size_t idx = (series->head + i) % series->max_size;
            result.push_back(series->points[idx]);
        }
    }

    return result;
}

std::vector<TelemetryPoint> TelemetryRingBuffer::get_series(const std::string& name, size_t last_n) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series) return {};

    size_t n = std::min(last_n, series->count);
    std::vector<TelemetryPoint> result;
    result.reserve(n);

    for (size_t i = 0; i < n; i++) {
        size_t idx;
        if (series->count < series->max_size) {
            idx = series->count - n + i;
        } else {
            idx = (series->head - n + i + series->max_size) % series->max_size;
        }
        result.push_back(series->points[idx]);
    }

    return result;
}

float TelemetryRingBuffer::get_latest(const std::string& name) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series || series->count == 0) return 0.0f;

    size_t idx;
    if (series->count < series->max_size) {
        idx = series->count - 1;
    } else {
        idx = (series->head - 1 + series->max_size) % series->max_size;
    }
    return series->points[idx].value;
}

void TelemetryRingBuffer::get_range(const std::string& name, float& min_val, float& max_val) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series || series->count == 0) {
        min_val = 0;
        max_val = 0;
        return;
    }

    min_val = std::numeric_limits<float>::max();
    max_val = std::numeric_limits<float>::lowest();

    size_t n = series->count;
    size_t start = (series->count < series->max_size) ? 0 :
                   series->head;

    for (size_t i = 0; i < n; i++) {
        size_t idx = (start + i) % series->max_size;
        float v = series->points[idx].value;
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }
}

std::vector<std::string> TelemetryRingBuffer::get_series_names() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::vector<std::string> names;
    for (const auto& s : series_) {
        names.push_back(s.name);
    }
    return names;
}

std::string TelemetryRingBuffer::to_json(const std::string& name) const {
    auto points = get_series(name);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < points.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{\"t\":" << points[i].timestamp_ms
            << ",\"v\":" << points[i].value << "}";
    }
    oss << "]";
    return oss.str();
}

std::string TelemetryRingBuffer::stats_json(const std::string& name) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series || series->count == 0) {
        return R"({"name":"","unit":"","count":0,"min":0,"max":0,"avg":0,"latest":0})";
    }

    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    double sum = 0;

    size_t n = series->count;
    size_t start = (series->count < series->max_size) ? 0 : series->head;

    for (size_t i = 0; i < n; i++) {
        size_t idx = (start + i) % series->max_size;
        float v = series->points[idx].value;
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
        sum += v;
    }

    size_t last_idx;
    if (series->count < series->max_size) {
        last_idx = series->count - 1;
    } else {
        last_idx = (series->head - 1 + series->max_size) % series->max_size;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << R"({"name":")" << series->name
        << R"(","unit":")" << series->unit
        << R"(","count":)" << series->count
        << R"(,"min":)" << min_val
        << R"(,"max":)" << max_val
        << R"(,"avg":)" << (sum / n)
        << R"(,"latest":)" << series->points[last_idx].value
        << "}";
    return oss.str();
}

void TelemetryRingBuffer::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    for (auto& s : series_) {
        s.head = 0;
        s.count = 0;
    }
}

void TelemetryRingBuffer::clear_series(const std::string& name) {
    mg::core::platform::LockGuard lk(mtx_);
    auto* series = find_series(name);
    if (series) {
        series->head = 0;
        series->count = 0;
    }
}

size_t TelemetryRingBuffer::total_points() const {
    mg::core::platform::LockGuard lk(mtx_);
    size_t total = 0;
    for (const auto& s : series_) {
        total += s.count;
    }
    return total;
}

TelemetrySeries* TelemetryRingBuffer::find_series(const std::string& name) {
    for (auto& s : series_) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

const TelemetrySeries* TelemetryRingBuffer::find_series(const std::string& name) const {
    for (const auto& s : series_) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

} // namespace mg::core::telemetry
