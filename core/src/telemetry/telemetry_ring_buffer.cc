/**
 * telemetry_ring_buffer.cc - Telemetry ring buffer implementation
 * 遥测环形缓冲区 — 固定容量时序数据存储
 */
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/telemetry/json_builder.h"
#include <algorithm>
#include <cmath>

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

    JsonBuilder jb;
    jb.begin_array();
    for (const auto& point : points) {
        JsonBuilder item;
        item.begin_object();
        item.field("t", static_cast<long long>(point.timestamp_ms));
        item.field("v", point.value);
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return jb.str();
}

std::string TelemetryRingBuffer::stats_json(const std::string& name) const {
    mg::core::platform::LockGuard lk(mtx_);

    auto* series = find_series(name);
    if (!series || series->count == 0) {
        JsonBuilder jb;
        jb.begin_object();
        jb.field("name", "");
        jb.field("unit", "");
        jb.field("count", 0);
        jb.field("min", 0.0);
        jb.field("max", 0.0);
        jb.field("avg", 0.0);
        jb.field("latest", 0.0);
        jb.end_object();
        return jb.str();
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

    // name/unit 来自前端（mg_telemetry_register_series），必须经 JsonBuilder 的
    // esc() 转义后再进 JSON，否则引号/反斜杠/控制字符会破坏响应结构。
    JsonBuilder jb;
    jb.begin_object();
    jb.field("name", series->name);
    jb.field("unit", series->unit);
    jb.field("count", static_cast<long long>(series->count));
    jb.field("min", min_val);
    jb.field("max", max_val);
    jb.field("avg", sum / n);
    jb.field("latest", series->points[last_idx].value);
    jb.end_object();
    return jb.str();
}

void TelemetryRingBuffer::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    for (auto& s : series_) {
        s.head = 0;
        s.count = 0;
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
