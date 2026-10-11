/**
 * calibration_types.h - 传感器校准核心类型
 *
 * 定义校准系统使用的枚举、结构体和常量。
 * 目标是让每次校准都有明确状态、质量评估和可追溯结果。
 * 各传感器差异由独立策略封装。
 */

#ifndef MGCORE_CALIBRATION_TYPES_H
#define MGCORE_CALIBRATION_TYPES_H

#include "core/core_types.h"
#include <string>
#include <vector>
#include <array>

namespace mg::core::calibration {

// ================================================================
// 枚举
// ================================================================

/// 传感器校准类型。
enum class CalibrationType : uint8_t {
    Gyroscope     = 0,
    Accelerometer = 1,
    Magnetometer  = 2,
    Barometer     = 3,
    Radio         = 4,  // 遥控器行程校准（MAV_CMD_PREFLIGHT_CALIBRATION param4）。
};

/// 校准阶段，由状态机维护。
enum class CalibrationStage : uint8_t {
    Idle              = 0,  // 未校准。
    WaitingForStart   = 1,  // 等待用户确认开始。
    WaitingOrientation = 2, // 等待用户调整机体姿态。
    Calibrating       = 3,  // 正在采样。
    Verifying         = 4,  // 校准完成，正在验证参数。
    Success           = 5,  // 校准成功。
    Failed            = 6,  // 校准失败。
    Cancelled         = 7,  // 飞控已确认停止（或取消超时收尾）。
    // 已向飞控发出取消命令，等待确认。放在末尾而不是插在 Cancelled 前面，
    // 是为了不改变已落盘校准历史里 Cancelled=7 的数值（见
    // calibration_service.cc 的 MGCAL1 读写）。
    Cancelling        = 8,
};

/// 校准质量评估。
enum class CalibrationQuality : uint8_t {
    Excellent = 0,  // 偏移量处于理想范围。
    Good      = 1,  // 偏移量处于可接受范围。
    Marginal  = 2,  // 勉强可接受，建议关注。
    Poor      = 3,  // 异常，建议重新校准。
};

/// 加速度计校准模式，主要对应 ArduPilot。
enum class AccelCalMode : uint8_t {
    Full      = 1,  // 六面校准，精度最高。
    Simple    = 4,  // 单姿态简易校准。
    Trim      = 2,  // 板级 trim 校准。
};

// ================================================================
// 质量阈值
//
// 参考 ArduPilot 校准标准和 IMU 厂商规格，不同机架可按实例调整。
// ================================================================

struct QualityThresholds {
    // 陀螺仪：偏移量应非常小，单位 rad/s。
    float gyro_excellent = 0.01f;
    float gyro_good      = 0.05f;
    float gyro_marginal  = 0.1f;

    // 加速度计：偏移量单位 m/s²，这里比的是三轴偏移量的模长。
    //
    // ArduPilot 的 INS_ACCOFFS_* 量程是 ±3.5 m/s²；实测一块校准正常的板子，
    // 三轴偏置合起来常在 0.2–1.0 之间。原来 0.1/0.3/0.5 的阈值会把正常校准
    // 判成 Poor，进而在 handle_success 里把飞控已经成功的校准报成失败。
    float accel_excellent = 0.5f;
    float accel_good      = 1.0f;
    float accel_marginal  = 2.0f;

    // 磁力计：偏移量单位 mGauss，地磁场约 250-650 mGauss。
    float mag_excellent = 50.0f;
    float mag_good      = 200.0f;
    float mag_marginal  = 500.0f;

    // 磁力计 fitness：ArduPilot MAG_CAL 分数，越低越好。
    float mag_fitness_excellent = 10.0f;
    float mag_fitness_good      = 30.0f;
    float mag_fitness_marginal  = 50.0f;

    // 气压计：相对基准气压的偏差，单位 hPa。
    float baro_excellent = 0.5f;
    float baro_good      = 2.0f;
    float baro_marginal  = 5.0f;
};

// ================================================================
// 结果结构
// ================================================================

/// 单次校准结果，记录一次完整尝试。
struct CalibrationResult {
    CalibrationType    type       = CalibrationType::Gyroscope;
    CalibrationStage   stage      = CalibrationStage::Idle;
    CalibrationQuality quality    = CalibrationQuality::Poor;
    uint64_t           timestamp_ms = 0;
    AccelCalMode       accel_mode = AccelCalMode::Full;
    std::string        failure_reason;

    // 校准后从飞控读取的偏移量。
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;

    // 缩放系数，仅加速度计使用。
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float scale_z = 1.0f;

    // 磁力计专用：fitness、对角线和非对角线参数。
    float fitness     = 0.0f;    // 0-100，越低越好。
    bool  mag_report_received = false;  // 由 MAVLink MAG_CAL_REPORT 提供的权威结果。
    float diagonal_x  = 0.0f;
    float diagonal_y  = 0.0f;
    float diagonal_z  = 0.0f;
    float offdiag_x   = 0.0f;    // 非对角线元素。
    float offdiag_y   = 0.0f;
    float offdiag_z   = 0.0f;

    // 校准前的旧值，用于对比。
    float prev_offset_x = 0.0f;
    float prev_offset_y = 0.0f;
    float prev_offset_z = 0.0f;

    // 进度跟踪。
    uint8_t progress      = 0;   // 0-100。
    uint8_t current_step  = 0;   // 当前步骤。
    uint8_t total_steps   = 0;   // 总步骤，例如加速度计六面校准为 6。

    // 原始状态文本，仅用于调试，不直接发送给前端。
    std::string raw_status_text;
};

/// 重试信息。
struct RetryInfo {
    int         retry_count = 0;
    int         max_retries = 3;
    int         backoff_ms  = 1000;
    std::string last_failure;
};

// ================================================================
// ArduPilot 参数名
//
// 这些参数保存校准结果。校准完成后读取它们，用于验证数值和评估质量。
// ================================================================

namespace ardupilot_params {

    // 陀螺仪偏移量，按 IMU 实例区分。
    inline constexpr const char* kGyroOffsX[]  = {"INS_GYROFFS_X", "INS_GYR2OFFS_X", "INS_GYR3OFFS_X"};
    inline constexpr const char* kGyroOffsY[]  = {"INS_GYROFFS_Y", "INS_GYR2OFFS_Y", "INS_GYR3OFFS_Y"};
    inline constexpr const char* kGyroOffsZ[]  = {"INS_GYROFFS_Z", "INS_GYR2OFFS_Z", "INS_GYR3OFFS_Z"};

    // 加速度计偏移量，按 IMU 实例区分。
    inline constexpr const char* kAccelOffsX[] = {"INS_ACCOFFS_X", "INS_ACC2OFFS_X", "INS_ACC3OFFS_X"};
    inline constexpr const char* kAccelOffsY[] = {"INS_ACCOFFS_Y", "INS_ACC2OFFS_Y", "INS_ACC3OFFS_Y"};
    inline constexpr const char* kAccelOffsZ[] = {"INS_ACCOFFS_Z", "INS_ACC2OFFS_Z", "INS_ACC3OFFS_Z"};

    // 加速度计缩放系数。
    inline constexpr const char* kAccelScaleX[] = {"INS_ACCSCAL_X", "INS_ACC2SCAL_X", "INS_ACC3SCAL_X"};
    inline constexpr const char* kAccelScaleY[] = {"INS_ACCSCAL_Y", "INS_ACC2SCAL_Y", "INS_ACC3SCAL_Y"};
    inline constexpr const char* kAccelScaleZ[] = {"INS_ACCSCAL_Z", "INS_ACC2SCAL_Z", "INS_ACC3SCAL_Z"};

    // 磁力计偏移量，按 compass 实例区分。
    inline constexpr const char* kMagOffsX[] = {"COMPASS_OFS_X", "COMPASS_OFS2_X", "COMPASS_OFS3_X"};
    inline constexpr const char* kMagOffsY[] = {"COMPASS_OFS_Y", "COMPASS_OFS2_Y", "COMPASS_OFS3_Y"};
    inline constexpr const char* kMagOffsZ[] = {"COMPASS_OFS_Z", "COMPASS_OFS2_Z", "COMPASS_OFS3_Z"};

    // 磁力计对角线参数。
    inline constexpr const char* kMagDiaX[] = {"COMPASS_DIA_X", "COMPASS_DIA2_X", "COMPASS_DIA3_X"};
    inline constexpr const char* kMagDiaY[] = {"COMPASS_DIA_Y", "COMPASS_DIA2_Y", "COMPASS_DIA3_Y"};
    inline constexpr const char* kMagDiaZ[] = {"COMPASS_DIA_Z", "COMPASS_DIA2_Z", "COMPASS_DIA3_Z"};

    // 磁力计非对角线参数。
    inline constexpr const char* kMagOdiX[] = {"COMPASS_ODI_X", "COMPASS_ODI2_X", "COMPASS_ODI3_X"};
    inline constexpr const char* kMagOdiY[] = {"COMPASS_ODI_Y", "COMPASS_ODI2_Y", "COMPASS_ODI3_Y"};
    inline constexpr const char* kMagOdiZ[] = {"COMPASS_ODI_Z", "COMPASS_ODI2_Z", "COMPASS_ODI3_Z"};

    // 磁力计 fitness，ArduPilot 在 MAG_CAL_REPORT 中上报。
    inline constexpr const char* kMagFitness[] = {"COMPASS_MOTCT"};  // 电机补偿类型，间接参考。

    // 气压计地面气压。
    inline constexpr const char* kBaroGroundPress = "GND_ABS_PRESS";
    inline constexpr const char* kBaroGroundTemp   = "GND_TEMP";

    // 遥控器行程参数（按通道 1..4 检查主通道即可判定校准结果）。
    inline constexpr const char* kRcMin[]  = {"RC1_MIN", "RC2_MIN", "RC3_MIN", "RC4_MIN"};
    inline constexpr const char* kRcTrim[] = {"RC1_TRIM", "RC2_TRIM", "RC3_TRIM", "RC4_TRIM"};
    inline constexpr const char* kRcMax[]  = {"RC1_MAX", "RC2_MAX", "RC3_MAX", "RC4_MAX"};

} // namespace ardupilot_params

// ================================================================
// 常量
// ================================================================

inline constexpr int kMaxCalibrationHistory = 10;  // 每种类型最多保留 N 条结果。
inline constexpr int kDefaultMaxRetries    = 3;
inline constexpr int kDefaultRetryBackoffMs = 1000;

// ArduPilot MAV_CMD_PREFLIGHT_CALIBRATION 参数值。
inline constexpr float kCalGyroStart     = 1.0f;
inline constexpr float kCalMagStart      = 1.0f;
inline constexpr float kCalBaroStart     = 1.0f;
inline constexpr float kCalAccelFull     = 1.0f;
inline constexpr float kCalAccelTrim     = 2.0f;
inline constexpr float kCalAccelSimple   = 4.0f;
inline constexpr float kCalMagForceSave  = 76.0f;
inline constexpr float kCalAccelForceSave = 76.0f;
inline constexpr float kCalRadioStart    = 1.0f;
inline constexpr float kCalRadioTrim     = 2.0f;

} // namespace mg::core::calibration

#endif // MGCORE_CALIBRATION_TYPES_H
