/**
 * test_waveform_buffer.cc - WaveformBuffer 单元测试（无硬件）
 *
 * 覆盖：采样限流、环形回绕、min/max 抽稀、时间戳基准、空通道。
 */
#include "core/telemetry/waveform_buffer.h"

#include <cmath>
#include <iostream>
#include <vector>

using mg::core::telemetry::kWaveFloatsPerPoint;
using mg::core::telemetry::WaveformBuffer;

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; std::cout << "  PASS: " << name << std::endl; } \
    else { failed++; std::cout << "  FAIL: " << name << std::endl; } \
} while (0)

namespace {

struct Points {
    std::vector<float> data;
    size_t count = 0;

    float at(size_t point, size_t channel) const {
        return data[point * kWaveFloatsPerPoint + 1 + channel];
    }
    float time(size_t point) const { return data[point * kWaveFloatsPerPoint]; }
};

Points read(const WaveformBuffer& buffer, size_t max_points, uint64_t window_ms = 0) {
    Points out;
    out.data.assign(max_points * kWaveFloatsPerPoint, 0.0f);
    out.count = buffer.copy_points(out.data.data(), max_points, window_ms);
    return out;
}

/// 写入一组可区分的九轴数据，方便断言各通道落在正确的位置。
void push_imu(WaveformBuffer& buffer, float seed, uint64_t timestamp_ms) {
    buffer.set_accel(seed, seed + 1.0f, seed + 2.0f);
    buffer.set_gyro(seed + 3.0f, seed + 4.0f, seed + 5.0f);
    buffer.set_mag(seed + 6.0f, seed + 7.0f, seed + 8.0f);
    buffer.commit(timestamp_ms);
}

} // namespace

int main() {
    std::cout << "=== WaveformBuffer Unit Tests ===" << std::endl;

    {
        std::cout << "\n[Group] 采样限流" << std::endl;
        WaveformBuffer buffer(100, 20);

        push_imu(buffer, 1.0f, 1000);
        push_imu(buffer, 11.0f, 1005);   // 间隔不足，这一帧被丢弃
        push_imu(buffer, 21.0f, 1020);   // 达到最小间隔，保留

        TEST("低于最小间隔的帧被丢弃", buffer.size() == 2);
        auto points = read(buffer, 10);
        TEST("保留的是最后一次采样值", std::abs(points.at(1, 0) - 21.0f) < 1e-6f);
    }

    {
        std::cout << "\n[Group] 九轴落在同一帧" << std::endl;
        WaveformBuffer buffer(10, 0);
        push_imu(buffer, 100.0f, 1000);

        auto points = read(buffer, 5);
        TEST("一次提交只产生一帧", points.count == 1);
        TEST("加速度计 X", std::abs(points.at(0, 0) - 100.0f) < 1e-6f);
        TEST("加速度计 Y", std::abs(points.at(0, 1) - 101.0f) < 1e-6f);
        TEST("加速度计 Z", std::abs(points.at(0, 2) - 102.0f) < 1e-6f);
        TEST("陀螺仪 X", std::abs(points.at(0, 3) - 103.0f) < 1e-6f);
        TEST("陀螺仪 Y", std::abs(points.at(0, 4) - 104.0f) < 1e-6f);
        TEST("陀螺仪 Z", std::abs(points.at(0, 5) - 105.0f) < 1e-6f);
        TEST("磁力计 X", std::abs(points.at(0, 6) - 106.0f) < 1e-6f);
        TEST("磁力计 Y", std::abs(points.at(0, 7) - 107.0f) < 1e-6f);
        TEST("磁力计 Z", std::abs(points.at(0, 8) - 108.0f) < 1e-6f);
    }

    {
        std::cout << "\n[Group] 环形缓冲回绕" << std::endl;
        WaveformBuffer buffer(3, 0);
        for (int i = 0; i < 5; i++) {
            push_imu(buffer, static_cast<float>(i), static_cast<uint64_t>(1000 + i * 100));
        }

        TEST("容量被遵守", buffer.size() == 3);
        auto points = read(buffer, 3);
        TEST("读出 3 个点", points.count == 3);
        TEST("最旧的数据被覆盖", std::abs(points.at(0, 0) - 2.0f) < 1e-6f);
        TEST("顺序为从旧到新", std::abs(points.at(2, 0) - 4.0f) < 1e-6f);
        TEST("时间戳以最旧的点为零点", std::abs(points.time(0)) < 1e-6f);
        TEST("时间戳递增", points.time(2) > points.time(1));
        TEST("最新时间戳可查询", buffer.latest_timestamp_ms() == 1400);
    }

    {
        std::cout << "\n[Group] min/max 抽稀" << std::endl;
        WaveformBuffer buffer(1000, 0);
        for (int i = 0; i < 400; i++) {
            const float value = (i == 200) ? 100.0f : 1.0f;
            buffer.set_accel(value, -1.0f, 0.0f);
            buffer.set_gyro(value, 0.0f, 0.0f);
            buffer.set_mag(0.0f, 0.0f, 0.0f);
            buffer.commit(static_cast<uint64_t>(1000 + i * 10));
        }

        auto points = read(buffer, 20);
        TEST("点数不超过上限", points.count <= 20);
        TEST("抽稀后仍有数据", points.count > 0);

        bool spike_kept = false;
        bool negative_kept = false;
        for (size_t i = 0; i < points.count; i++) {
            if (std::abs(points.at(i, 0) - 100.0f) < 1e-6f) spike_kept = true;
            if (std::abs(points.at(i, 1) + 1.0f) < 1e-6f) negative_kept = true;
        }
        TEST("尖峰不会被抽稀丢掉", spike_kept);
        TEST("负向极值同样保留", negative_kept);

        bool monotonic = true;
        for (size_t i = 1; i < points.count; i++) {
            if (points.time(i) < points.time(i - 1)) monotonic = false;
        }
        TEST("抽稀后时间戳仍然单调", monotonic);
    }

    {
        std::cout << "\n[Group] 时间窗口与分桶稳定性" << std::endl;
        WaveformBuffer buffer(1000, 0);
        for (int i = 0; i < 400; i++) {
            push_imu(buffer, static_cast<float>(i % 7), static_cast<uint64_t>(100000 + i * 50));
        }

        // 只取最近 5 秒：400 帧共 20 秒，窗口内应明显少于全部帧。
        auto windowed = read(buffer, 20, 5000);
        TEST("时间窗口裁掉了旧数据", windowed.count > 0 && windowed.count < 20 * 2);

        // 分桶对齐绝对时间：输出点必须落在等宽的时间栅格上，
        // 这样新数据到来时已有数据点不会左右跳动。
        bool paired = windowed.count > 2 && windowed.count % 2 == 0;
        for (size_t i = 0; paired && i + 1 < windowed.count; i += 2) {
            if (std::abs(windowed.time(i) - windowed.time(i + 1)) > 1e-3f) paired = false;
        }
        TEST("同一分桶的 min/max 落在同一横向位置", paired);

        const float step = windowed.time(2) - windowed.time(0);
        bool uniform = windowed.count > 4 && step > 0;
        for (size_t i = 0; uniform && i + 2 < windowed.count; i += 2) {
            if (std::abs((windowed.time(i + 2) - windowed.time(i)) - step) > 1e-3f) {
                uniform = false;
            }
        }
        TEST("抽稀输出落在等宽时间栅格上", uniform);
    }

    {
        std::cout << "\n[Group] 通道与生命周期" << std::endl;
        WaveformBuffer buffer(10, 0);

        TEST("空缓冲读出 0 个点", read(buffer, 5).count == 0);

        buffer.set_gyro(1.0f, 2.0f, 3.0f);
        buffer.commit(500);
        auto points = read(buffer, 5);
        TEST("只收到陀螺仪时也有数据", points.count == 1);
        TEST("陀螺仪通道正常", std::abs(points.at(0, 3) - 1.0f) < 1e-6f);
        TEST("未收到的加速度通道为空", std::isnan(points.at(0, 0)));
        TEST("未收到的磁力计通道为空", std::isnan(points.at(0, 7)));

        buffer.clear();
        TEST("clear 之后为空", buffer.size() == 0 && buffer.latest_timestamp_ms() == 0);
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}
