/**
 * waveform_buffer.cc - 实时波形采样缓冲实现
 */
#include "core/telemetry/waveform_buffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mg::core::telemetry {

namespace {

/// 累加一个通道的极值，NaN 表示该通道还没有有效采样。
inline void accumulate(float value, float& low, float& high) {
    if (!std::isfinite(value)) return;
    if (!std::isfinite(low) || value < low) low = value;
    if (!std::isfinite(high) || value > high) high = value;
}

/// 分桶内先输出靠近第一个采样值的极值，保持折线走向和真实时序一致。
inline bool min_comes_first(float first, float low, float high) {
    if (!std::isfinite(first) || !std::isfinite(low) || !std::isfinite(high)) return true;
    return std::fabs(first - low) <= std::fabs(first - high);
}

} // namespace

WaveformBuffer::WaveformBuffer(size_t capacity, uint64_t min_interval_ms)
    : capacity_(capacity == 0 ? 1 : capacity), min_interval_ms_(min_interval_ms) {
    frames_.resize(capacity_);
    for (size_t i = 0; i < kWaveChannelCount; ++i) {
        pending_[i] = std::numeric_limits<float>::quiet_NaN();
    }
}

WaveformBuffer::~WaveformBuffer() = default;

void WaveformBuffer::set_accel(float x_ms2, float y_ms2, float z_ms2) {
    mg::core::platform::LockGuard lk(mtx_);
    pending_[kWaveAccelX] = x_ms2;
    pending_[kWaveAccelY] = y_ms2;
    pending_[kWaveAccelZ] = z_ms2;
}

void WaveformBuffer::set_gyro(float x_dps, float y_dps, float z_dps) {
    mg::core::platform::LockGuard lk(mtx_);
    pending_[kWaveGyroX] = x_dps;
    pending_[kWaveGyroY] = y_dps;
    pending_[kWaveGyroZ] = z_dps;
}

void WaveformBuffer::set_mag(float x_gauss, float y_gauss, float z_gauss) {
    mg::core::platform::LockGuard lk(mtx_);
    pending_[kWaveMagX] = x_gauss;
    pending_[kWaveMagY] = y_gauss;
    pending_[kWaveMagZ] = z_gauss;
}

void WaveformBuffer::commit(uint64_t timestamp_ms) {
    mg::core::platform::LockGuard lk(mtx_);
    if (has_emit_ && timestamp_ms - last_emit_ms_ < min_interval_ms_) return;
    emit_locked(timestamp_ms);
}

void WaveformBuffer::emit_locked(uint64_t timestamp_ms) {
    WaveformFrame& frame = frames_[head_];
    frame.timestamp_ms = timestamp_ms;
    std::copy(std::begin(pending_), std::end(pending_), frame.values);

    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
    last_emit_ms_ = timestamp_ms;
    has_emit_ = true;
}

void WaveformBuffer::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    head_ = 0;
    count_ = 0;
    has_emit_ = false;
    last_emit_ms_ = 0;
    for (size_t i = 0; i < kWaveChannelCount; ++i) {
        pending_[i] = std::numeric_limits<float>::quiet_NaN();
    }
}

size_t WaveformBuffer::size() const {
    mg::core::platform::LockGuard lk(mtx_);
    return count_;
}

bool WaveformBuffer::empty() const {
    mg::core::platform::LockGuard lk(mtx_);
    return count_ == 0;
}

uint64_t WaveformBuffer::latest_timestamp_ms() const {
    mg::core::platform::LockGuard lk(mtx_);
    if (count_ == 0) return 0;
    const size_t newest = (count_ < capacity_)
                              ? count_ - 1
                              : (head_ + capacity_ - 1) % capacity_;
    return frames_[newest].timestamp_ms;
}

size_t WaveformBuffer::copy_points(float* out_points, size_t max_points,
                                   uint64_t window_ms) const {
    if (!out_points || max_points == 0) return 0;

    mg::core::platform::LockGuard lk(mtx_);
    if (count_ == 0) return 0;

    // 缓冲未满时从 0 开始，已满时从 head_（最旧的一帧）开始。
    const size_t oldest = (count_ < capacity_) ? 0 : head_;
    const uint64_t newest_ms = frames_[(oldest + count_ - 1) % capacity_].timestamp_ms;

    // 时间窗口裁剪：至少保留一帧。
    size_t first = 0;
    if (window_ms > 0 && newest_ms > window_ms) {
        const uint64_t cutoff = newest_ms - window_ms;
        while (first + 1 < count_ &&
               frames_[(oldest + first) % capacity_].timestamp_ms < cutoff) {
            ++first;
        }
    }
    const size_t available = count_ - first;
    const uint64_t window_start_ms = frames_[(oldest + first) % capacity_].timestamp_ms;

    size_t written = 0;
    if (available <= max_points) {
        for (size_t i = first; i < count_; ++i) {
            const WaveformFrame& frame = frames_[(oldest + i) % capacity_];
            float* dst = out_points + written * kWaveFloatsPerPoint;
            dst[0] = static_cast<float>(frame.timestamp_ms - window_start_ms);
            std::copy(std::begin(frame.values), std::end(frame.values), dst + 1);
            ++written;
        }
        return written;
    }

    // 每个分桶输出 min/max 两个点，因此分桶数取 max_points 的一半。
    // 桶边界落在绝对时间的整数倍上，所以每帧输出的横向位置是固定的。
    const size_t buckets = std::max<size_t>(1, max_points / 2);
    const uint64_t span_ms = newest_ms - window_start_ms + 1;
    const uint64_t bucket_ms =
        std::max<uint64_t>(1, (span_ms + buckets - 1) / buckets);
    const uint64_t first_cell = window_start_ms / bucket_ms;

    struct Cell {
        float low[kWaveChannelCount];
        float high[kWaveChannelCount];
        float first[kWaveChannelCount];
        bool seen = false;
    };
    std::vector<Cell> cells(buckets + 2);
    for (auto& cell : cells) {
        for (size_t c = 0; c < kWaveChannelCount; ++c) {
            cell.low[c] = std::numeric_limits<float>::quiet_NaN();
            cell.high[c] = std::numeric_limits<float>::quiet_NaN();
            cell.first[c] = std::numeric_limits<float>::quiet_NaN();
        }
    }

    for (size_t i = first; i < count_; ++i) {
        const WaveformFrame& frame = frames_[(oldest + i) % capacity_];
        const uint64_t cell_index = frame.timestamp_ms / bucket_ms - first_cell;
        if (cell_index >= cells.size()) continue;
        Cell& cell = cells[cell_index];
        for (size_t c = 0; c < kWaveChannelCount; ++c) {
            if (!cell.seen) cell.first[c] = frame.values[c];
            accumulate(frame.values[c], cell.low[c], cell.high[c]);
        }
        cell.seen = true;
    }

    for (size_t index = 0; index < cells.size() && written + 2 <= max_points; ++index) {
        const Cell& cell = cells[index];
        if (!cell.seen) continue;
        // 横向位置取桶中心：同一帧数据每次落点一致，波形不会左右抖动。
        const float x = static_cast<float>(index * bucket_ms + bucket_ms / 2);
        float* dst_low = out_points + written * kWaveFloatsPerPoint;
        float* dst_high = dst_low + kWaveFloatsPerPoint;
        for (size_t c = 0; c < kWaveChannelCount; ++c) {
            if (min_comes_first(cell.first[c], cell.low[c], cell.high[c])) {
                dst_low[1 + c] = cell.low[c];
                dst_high[1 + c] = cell.high[c];
            } else {
                dst_low[1 + c] = cell.high[c];
                dst_high[1 + c] = cell.low[c];
            }
        }
        dst_low[0] = x;
        dst_high[0] = x;
        written += 2;
    }

    return written;
}

} // namespace mg::core::telemetry
