/**
 * statustext_match.h - 校准 STATUSTEXT 的完成/失败判定（内部共用）
 *
 * 五种校准策略原先各写一份判定逻辑，四份口径互不相同：gyro/baro/accel 按整句
 * 匹配、compass 先判成功再判失败、radio 用 "success"/"complete" 裸子串。裸子串
 * 会把飞控报的失败读成成功——"Gyro calibration unsuccessful" 含 "success"，
 * "Barometer calibration incomplete" 含 "complete"。这里收敛成一份，四种策略
 * 共用，判定顺序固定为「先失败后完成」。
 *
 * 话题词（Gyro / Baro / Compass / RC …）是各策略自己的，不在本文件。
 */

#ifndef MGCORE_CALIBRATION_STATUSTEXT_MATCH_H
#define MGCORE_CALIBRATION_STATUSTEXT_MATCH_H

#include <string>

namespace mg::core::calibration::text {

/// 完成短语必须整句匹配："...calibration incomplete" 含 "complete"，
/// "Calibration unsuccessful" 含 "successful"，用裸子串都会把失败读成成功。
inline bool mentions_completion(const std::string& text) {
    return text.find("calibration complete") != std::string::npos ||
           text.find("Calibration complete") != std::string::npos ||
           text.find("calibration successful") != std::string::npos ||
           text.find("Calibration successful") != std::string::npos;
}

/// 失败词。调用方必须**先**问这个再问 mentions_completion：
/// "unsuccessful" 含 "successful"、"incomplete" 含 "complete"。
inline bool mentions_failure(const std::string& text) {
    return text.find("failed") != std::string::npos ||
           text.find("Failed") != std::string::npos ||
           text.find("FAILED") != std::string::npos ||
           text.find("unsuccessful") != std::string::npos ||
           text.find("Unsuccessful") != std::string::npos ||
           text.find("incomplete") != std::string::npos ||
           text.find("Incomplete") != std::string::npos;
}

} // namespace mg::core::calibration::text

#endif // MGCORE_CALIBRATION_STATUSTEXT_MATCH_H
