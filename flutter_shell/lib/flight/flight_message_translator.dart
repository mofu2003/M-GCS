import 'package:flutter/foundation.dart';

/// 一条飞控消息的展示结果。
@immutable
final class FlightMessageTranslation {
  const FlightMessageTranslation({
    required this.displayText,
    required this.originalText,
    required this.translated,
  });

  final String displayText;
  final String originalText;
  final bool translated;
}

/// 将常见 ArduPilot STATUSTEXT 规则化翻译成中文。
///
/// 这里刻意不做机器翻译：只翻译已知短语和模板，未知消息原样返回。
/// 这样可以避免错误翻译掩盖真实故障，同时保留查看原文的能力。
FlightMessageTranslation translateFlightMessage(String text) {
  final original = text.trim();
  if (original.isEmpty) {
    return FlightMessageTranslation(
      displayText: text,
      originalText: text,
      translated: false,
    );
  }

  final exact = _exactTranslations[original.toLowerCase()];
  if (exact != null) {
    return FlightMessageTranslation(
      displayText: exact,
      originalText: original,
      translated: true,
    );
  }

  for (final rule in _rules) {
    final match = rule.pattern.firstMatch(original);
    if (match == null) continue;
    return FlightMessageTranslation(
      displayText: rule.translate(match),
      originalText: original,
      translated: true,
    );
  }

  return FlightMessageTranslation(
    displayText: text,
    originalText: original,
    translated: false,
  );
}

final class _TranslationRule {
  const _TranslationRule(this.pattern, this.translate);

  final RegExp pattern;
  final String Function(RegExpMatch match) translate;
}

const Map<String, String> _exactTranslations = {
  'arming motors': '正在解锁电机',
  'disarming motors': '正在上锁电机',
  'arming checks passed': '解锁前检查通过',
  'arming checks failed': '解锁前检查失败',
  'motors armed': '电机已解锁',
  'motors disarmed': '电机已上锁',
  'mission complete': '任务完成',
  'returning to launch': '正在返航',
  'landing': '正在降落',
  'takeoff complete': '起飞完成',
  'throttle failsafe on': '油门失控保护已触发',
  'radio failsafe on': '遥控器失控保护已触发',
  'gps failsafe on': 'GPS 失控保护已触发',
  'battery failsafe on': '电池失控保护已触发',
  'ekf variance': 'EKF 方差过大',
  'gyro calibration successful': '陀螺仪校准成功',
  'gyro calibration failed': '陀螺仪校准失败',
  'compass calibration successful': '罗盘校准成功',
  'compass calibration failed': '罗盘校准失败',
  'barometer calibration successful': '气压计校准成功',
  'barometer calibration failed': '气压计校准失败',
  'accelerometer calibration successful': '加速度计校准成功',
  'accelerometer calibration failed': '加速度计校准失败',
};

final List<_TranslationRule> _rules = [
  _TranslationRule(
    RegExp(r'^PreArm:\s*(.+)$', caseSensitive: false),
    (match) => '预解锁：${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^EKF(\d+)\s+IMU(\d+)\s+is using GPS$', caseSensitive: false),
    (match) => 'EKF${match.group(1)} IMU${match.group(2)} 正在使用 GPS',
  ),
  _TranslationRule(
    RegExp(r'^EKF(\d+)\s+IMU(\d+)\s+is using (.+)$', caseSensitive: false),
    (match) =>
        'EKF${match.group(1)} IMU${match.group(2)} 正在使用 ${_translateFragment(match.group(3)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Battery\s+(\d+)\s+is low$', caseSensitive: false),
    (match) => '电池 ${match.group(1)} 电量低',
  ),
  _TranslationRule(
    RegExp(r'^GPS:\s*(.+)$', caseSensitive: false),
    (match) => 'GPS：${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Mode changed to (.+)$', caseSensitive: false),
    (match) => '飞行模式已切换为 ${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Flight mode = (.+)$', caseSensitive: false),
    (match) => '飞行模式 = ${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Reached waypoint #?(\d+)$', caseSensitive: false),
    (match) => '已到达航点 #${match.group(1)}',
  ),
  _TranslationRule(
    RegExp(r'^Mission:\s*(.+)$', caseSensitive: false),
    (match) => '任务：${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Takeoff to (.+)$', caseSensitive: false),
    (match) => '起飞至 ${_translateFragment(match.group(1)!)}',
  ),
  _TranslationRule(
    RegExp(r'^Landing at (.+)$', caseSensitive: false),
    (match) => '正在降落至 ${_translateFragment(match.group(1)!)}',
  ),
];

String _translateFragment(String fragment) {
  var result = fragment;
  const fragments = {
    'pre-arm': '预解锁',
    'pre arm': '预解锁',
    'arming': '解锁',
    'disarming': '上锁',
    'accelerometer': '加速度计',
    'barometer': '气压计',
    'compass': '罗盘',
    'gyroscope': '陀螺仪',
    'gyro': '陀螺仪',
    'calibration': '校准',
    'successful': '成功',
    'failed': '失败',
    'battery': '电池',
    'voltage': '电压',
    'throttle': '油门',
    'failsafe': '失控保护',
    'position': '定位',
    'GPS': 'GPS',
  };
  for (final entry in fragments.entries) {
    result = result.replaceAll(
      RegExp(RegExp.escape(entry.key), caseSensitive: false),
      entry.value,
    );
  }
  return result;
}
