#pragma once

/**
 * waveform_buffer.h - 实时波形采样缓冲
 *
 * 传感器波形页面把加速度计、陀螺仪和磁力计画在同一条时间轴上，所以这里按
 * 「帧」存储：一帧同时携带九个轴的值和同一个时间戳。同一帧内各轴来自同一次
 * IMU 采样，前端不需要再做时间对齐，抽稀也只需要对整个缓冲跑一遍。
 *
 * 采样按最小间隔限流，链路突发时不会把缓冲写满；读取时按目标点数做
 * min/max 分桶抽稀，长窗口下尖峰不会被丢掉。
 *
 * 线程安全：接收线程写入，UI 线程读取。
 */

#include "core/platform/platform_mutex.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mg::core::telemetry {

/// 波形通道。顺序是前后端契约的一部分，不能随意调整。
enum WaveformChannel : size_t {
    kWaveAccelX = 0,
    kWaveAccelY = 1,
    kWaveAccelZ = 2,
    kWaveGyroX = 3,
    kWaveGyroY = 4,
    kWaveGyroZ = 5,
    kWaveMagX = 6,
    kWaveMagY = 7,
    kWaveMagZ = 8,
    kWaveChannelCount = 9,
};

/// 每个输出点占用的 float 数量：相对时间戳 + 每个通道一个值。
constexpr size_t kWaveFloatsPerPoint = kWaveChannelCount + 1;

struct WaveformFrame {
    uint64_t timestamp_ms = 0;                 // 单调时钟毫秒
    float values[kWaveChannelCount] = {};      // 按 WaveformChannel 排列
};

class WaveformBuffer {
public:
    /// capacity: 最多保留的帧数；min_interval_ms: 两次采样之间的最小间隔
    explicit WaveformBuffer(size_t capacity = 3000, uint64_t min_interval_ms = 20);
    ~WaveformBuffer();

    WaveformBuffer(const WaveformBuffer&) = delete;
    WaveformBuffer& operator=(const WaveformBuffer&) = delete;

    /// 写入加速度计三轴（单位：m/s²）。
    void set_accel(float x_ms2, float y_ms2, float z_ms2);

    /// 写入陀螺仪三轴（单位：度/秒）。
    void set_gyro(float x_dps, float y_dps, float z_dps);

    /// 写入磁力计三轴（单位：高斯）。
    void set_mag(float x_gauss, float y_gauss, float z_gauss);

    /// 提交一帧。距上一帧不足 min_interval_ms 时这一帧被丢弃，
    /// 所以同一次 IMU 采样的九个轴会落在同一帧里。
    void commit(uint64_t timestamp_ms);

    void clear();

    size_t size() const;
    bool empty() const;
    uint64_t latest_timestamp_ms() const;

    /// 把缓冲内容写入 out_points（每个点 kWaveFloatsPerPoint 个 float）。
    ///
    /// - window_ms > 0 时只取最新的这么多毫秒，0 表示整个缓冲；
    /// - 时间戳以窗口内最早的一个点为零点，单位毫秒；
    /// - 帧数超过 max_points 时按 min/max 分桶抽稀，最多写 max_points 个点，
    ///   分桶边界对齐到绝对时间栅格，这样相邻两帧输出的横向位置不会跳动；
    /// - 返回实际写入的点数，0 表示没有数据。
    size_t copy_points(float* out_points, size_t max_points, uint64_t window_ms = 0) const;

private:
    void emit_locked(uint64_t timestamp_ms);

    mutable mg::core::platform::Mutex mtx_;
    const size_t capacity_;
    const uint64_t min_interval_ms_;

    std::vector<WaveformFrame> frames_;
    size_t head_ = 0;         // 下一个写入位置
    size_t count_ = 0;        // 有效帧数
    uint64_t last_emit_ms_ = 0;
    bool has_emit_ = false;

    float pending_[kWaveChannelCount];  // 各通道最近一次采样值
};

} // namespace mg::core::telemetry
