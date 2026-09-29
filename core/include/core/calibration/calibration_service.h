/**
 * calibration_service.h - 统一传感器校准服务
 *
 * 这是所有校准操作的统一入口。服务负责维护校准状态机，
 * 将传感器差异委托给具体策略，处理 STATUSTEXT/ACK，
 * 并保存最近的校准历史。
 *
 * 用法示例：
 *   CalibrationService service;
 *   service.start(CalibrationType::Magnetometer);
 *   // 在 MAVLink 消息循环中：
 *   service.process_statustext(text, severity);
 *   service.process_command_ack(result, progress);
 *   // UI 或测试侧轮询状态：
 *   auto status = service.get_status();
 *
 * 设计：
 *   - 传感器差异使用 Strategy 模式封装，参考 QGC FirmwarePlugin 思路。
 *   - 校准生命周期由状态机驱动。
 *   - 失败重试次数可配置。
 *   - 每种校准类型保留最近 N 条内存历史。
 *   - 通过 platform::Mutex 保证线程安全。
 */

#ifndef MGCORE_CALIBRATION_SERVICE_H
#define MGCORE_CALIBRATION_SERVICE_H

#include "calibration_types.h"
#include "calibration_validator.h"
#include "calibration_strategy.h"
#include "core/platform/platform_mutex.h"
#include "core/transport/connection_manager.h"
#include <memory>
#include <array>
#include <vector>
#include <map>
#include <functional>

namespace mg::core::mavlink {
    class MavlinkAdapter;
    struct MagCalibrationProgressInfo;
    struct MagCalibrationReportInfo;
}

namespace mg::core::calibration {

/// 当前校准状态，供 UI 或测试轮询。
struct CalibrationStatus {
    bool        active = false;
    CalibrationType   current_type = CalibrationType::Gyroscope;
    CalibrationStage  stage = CalibrationStage::Idle;
    uint8_t     progress = 0;
    uint8_t     current_step = 0;
    uint8_t     total_steps = 0;
    std::string user_message;
    std::string failure_reason;
    int         retry_count = 0;
    int         max_retries = 0;
    uint64_t    start_time_ms = 0;
    AccelCalMode accel_mode = AccelCalMode::Full;

    /// 磁力计校准的分罗盘进度：compass_id（0 起）→ completion_pct。
    ///
    /// `progress` 仍然是所有罗盘的最大值，供单条进度条的界面使用；需要按罗盘
    /// 分别显示时读这张表（Mission Planner 就是按 compass_id 各画一条）。
    std::map<uint8_t, uint8_t> compass_progress;
};

/// 校准状态变化回调。
using CalibrationEventCallback = std::function<void(const CalibrationStatus&)>;

class CalibrationService {
public:
    CalibrationService();
    ~CalibrationService();

    /// 设置连接管理器，必须在 start 前完成。
    void set_connection(mg::core::transport::ConnectionManager* conn) { conn_ = conn; }

    /// 设置校准命令目标 system/component。
    void set_target(uint8_t sys, uint8_t comp) { target_sys_ = sys; target_comp_ = comp; }

    /// 注册校准事件回调，每次状态变化时触发。
    void set_event_callback(CalibrationEventCallback cb);

    /// 启动一次校准；若已有校准进行中则返回 false。
    bool start(CalibrationType type,
               AccelCalMode accel_mode = AccelCalMode::Full);

    /// 取消当前校准。
    bool cancel();

    /// 处理飞控发来的 STATUSTEXT。
    void process_statustext(const std::string& text, uint8_t severity);

    /// 处理飞控发来的 COMMAND_ACK。
    void process_command_ack(uint16_t command, uint8_t result, uint8_t progress);

    /// 处理 ArduPilot MAV_CAL 专用的罗盘进度和结果消息。
    void process_mag_cal_progress(const mg::core::mavlink::MagCalibrationProgressInfo& progress);
    void process_mag_cal_report(const mg::core::mavlink::MagCalibrationReportInfo& report);

    /// Handles the ArduPilot MAV_CMD_ACCELCAL_VEHICLE_POS notification sent
    /// by the flight controller for the next face or the terminal result.
    void process_accelerometer_position_request(uint32_t position);

    /// 获取当前校准状态线程安全快照。
    CalibrationStatus get_status() const;

    /// 获取指定类型的校准历史。
    std::vector<CalibrationResult> get_history(CalibrationType type) const;

    /// 判断当前是否有校准正在进行。
    bool is_active() const;

    /// 获取验证器，供 FFI 等外层使用。
    CalibrationValidator& validator() { return validator_; }
    const CalibrationValidator& validator() const { return validator_; }

private:
    class EventDispatchScope {
    public:
        explicit EventDispatchScope(CalibrationService& owner) : owner_(owner) {}
        ~EventDispatchScope() { owner_.fire_event_safe(); }

        EventDispatchScope(const EventDispatchScope&) = delete;
        EventDispatchScope& operator=(const EventDispatchScope&) = delete;

    private:
        CalibrationService& owner_;
    };

    // 每种校准类型对应一个策略。
    std::array<std::unique_ptr<ICalibrationStrategy>, 5> strategies_;

    // 当前校准状态。
    CalibrationStatus status_;
    ICalibrationStrategy* active_strategy_ = nullptr;
    mg::core::transport::ConnectionManager* conn_ = nullptr;
    uint8_t target_sys_ = 1;   // 由 set_target 设置。
    uint8_t target_comp_ = 1;  // 由 set_target 设置。

    CalibrationResult current_result_;
    RetryInfo retry_info_;
    AccelCalMode accel_mode_ = AccelCalMode::Full;

    // 按校准类型保存历史。
    std::array<std::vector<CalibrationResult>, 5> history_;
    // 历史有变动但尚未落盘。写盘放在锁外（fire_event_safe），
    // 避免在 MAVLink 回调的持锁路径里做文件 I/O。
    bool persist_dirty_ = false;

    // 校准结果验证器。
    CalibrationValidator validator_;

    // 状态变化回调。
    CalibrationEventCallback event_cb_;
    std::vector<CalibrationStatus> pending_events_;

    // 线程安全保护。
    mutable mg::core::platform::Mutex mtx_;

    // 内部辅助函数。
    void transition_to(CalibrationStage new_stage);
    void fire_event();
    void fire_event_safe();
    void handle_success();
    void handle_failure(const std::string& reason);
    void attempt_retry();
    void save_to_history();
    void update_status_from_result();
    ICalibrationStrategy* get_strategy(CalibrationType type);
    uint64_t now_ms() const;
    uint64_t wall_clock_now_ms() const;
    void load_history_from_disk();
    void write_history_file(
        const std::array<std::vector<CalibrationResult>, 5>& history) const;
    std::string history_storage_path() const;
};

} // namespace mg::core::calibration

#endif // MGCORE_CALIBRATION_SERVICE_H
