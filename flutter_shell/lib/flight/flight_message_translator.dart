import 'package:flutter/foundation.dart';

import 'ardupilot_message_catalog.dart';
import 'flight_mode_labels.dart';

/// 翻译只影响消息展示，英文原文仍保留供查看和诊断。
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

FlightMessageTranslation translateFlightMessage(String text) {
  final display = _translateKnownMessage(text.trim());
  return FlightMessageTranslation(
    displayText: display ?? text,
    originalText: text,
    translated: display != null,
  );
}

final _commandNames = {
  for (final entry in ardupilotMissionCommandNames.entries)
    entry.key.toLowerCase(): entry.value,
};
final _numberedCommand = RegExp(r'^(\d+)\s+(.+)$');
final _jumpCommand = RegExp(
  r'^(Jump|JumpToTag|Tag)\s+(-?\d+)$',
  caseSensitive: false,
);
final _messagePrefix = RegExp(
  r'^(PreArm|Arm|GPS|Mission|AHRS):\s*(.+)$',
  caseSensitive: false,
);
const _prefixLabels = {
  'prearm': '解锁前检查',
  'arm': '解锁检查',
  'gps': 'GPS',
  'mission': '任务',
  'ahrs': 'AHRS',
};
final _modeMessage = RegExp(
  r'^(Mode changed to |Flight mode = )(.+)$',
  caseSensitive: false,
);
final _ekfUsing = RegExp(
  r'^EKF(\d+) IMU(\d+) is using (GPS|external nav data|optical flow)$',
  caseSensitive: false,
);
final _rules = [
  for (final (source, translation) in ardupilotMessageTemplates)
    _PrintfMessageRule(source, translation),
];

String? _translateKnownMessage(String text) {
  final exact =
      ardupilotMessageBodies[text.toLowerCase()] ??
      _commandNames[text.toLowerCase()];
  if (exact != null) return exact;
  final numbered = _numberedCommand.firstMatch(text);
  if (numbered != null) {
    final command = numbered.group(2)!;
    final name = _commandNames[command.toLowerCase()];
    if (name != null) return '${numbered.group(1)} $name';
    // AP_Mission appends p1 to Jump/JumpToTag/Tag, not to every command.
    final jump = _jumpCommand.firstMatch(command);
    if (jump != null) {
      return '${numbered.group(1)} ${_commandNames[jump.group(1)!.toLowerCase()]} ${jump.group(2)}';
    }
  }
  final mode = _modeMessage.firstMatch(text);
  if (mode != null) {
    final label = _modeLabel(mode.group(2)!);
    return mode.group(1)!.toLowerCase().startsWith('mode changed')
        ? '飞行模式已切换为 $label'
        : '飞行模式 = $label';
  }
  final ekf = _ekfUsing.firstMatch(text);
  if (ekf != null) {
    final input = switch (ekf.group(3)!.toLowerCase()) {
      'gps' => 'GPS',
      'external nav data' => '外部导航数据',
      _ => '光流',
    };
    return 'EKF${ekf.group(1)} IMU${ekf.group(2)} 正在使用 $input';
  }
  // Parse known wrappers before a leading %s can absorb "Arm:" as part of a mode.
  final prefix = _messagePrefix.firstMatch(text);
  if (prefix != null) {
    final translatedBody = _translateKnownMessage(prefix.group(2)!);
    if (translatedBody != null) {
      return '${_prefixLabels[prefix.group(1)!.toLowerCase()]}：$translatedBody';
    }
  }
  for (final rule in _rules) {
    final translated = rule.translate(text);
    if (translated != null) return translated;
  }
  // AP_Arming applies phase prefixes to the same fault body. Unknown bodies
  // stay intact: never replace substrings inside diagnostics or parameter names.
  if (prefix != null) {
    final body = prefix.group(2)!;
    return '${_prefixLabels[prefix.group(1)!.toLowerCase()]}：$body';
  }
  return null;
}

String _modeLabel(String raw) {
  String normalise(String name) =>
      name.replaceAll(RegExp(r'[\s_]+'), '').toLowerCase();
  // Reuse the mode vocabulary without changing the English top status bar.
  for (final name in const [
    'Manual',
    'Circle',
    'Stabilize',
    'Training',
    'Acro',
    'FBWA',
    'FBWB',
    'Cruise',
    'Autotune',
    'Auto',
    'AutoRTL',
    'RTL',
    'SmartRTL',
    'Loiter',
    'Takeoff',
    'Guided',
    'GuidedNoGPS',
    'Thermal',
    'Autoland',
    'QStabilize',
    'QHover',
    'QLoiter',
    'QLand',
    'QRTL',
    'QAutotune',
    'QAcro',
    'AltHold',
    'Land',
    'Drift',
    'Sport',
    'Flip',
    'PosHold',
    'Brake',
    'Throw',
    'FlowHold',
    'Follow',
    'Zigzag',
    'SystemID',
    'Autorotate',
    'Hold',
    'Simple',
    'Smart RTL',
    'Loiter2QLand',
    'AvoidADSB',
    'Steering',
    'Steer',
    'SuperSimple',
    'Turtle',
    'Surface',
    'Dock',
    'Termination',
  ]) {
    if (normalise(name) == normalise(raw)) {
      return flightModeLabel(name == 'Smart RTL' ? 'SmartRTL' : name);
    }
  }
  return raw;
}

/// Only reviewed firmware printf templates are matched, as complete messages.
/// Captures retain number spelling, signs, precision, units and device IDs.
final class _PrintfMessageRule {
  _PrintfMessageRule(this.source, String translation) {
    final pattern = StringBuffer('^');
    var offset = 0;
    for (final token in _placeholder.allMatches(source)) {
      pattern.write(RegExp.escape(source.substring(offset, token.start)));
      final type = token.group(0)!.substring(token.group(0)!.length - 1);
      pattern.write(switch (type) {
        'u' => r'\s*(\d+)',
        'd' || 'i' => r'\s*([+-]?\d+)',
        'f' =>
          r'\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|[+-]?(?:nan|inf))',
        _ => r'(.*?)',
      });
      offset = token.end;
    }
    pattern.write(RegExp.escape(source.substring(offset)));
    pattern.write(r'$');
    _pattern = RegExp(pattern.toString(), caseSensitive: false);
    _parts = translation.split(_placeholder);
  }

  final String source;
  late final RegExp _pattern;
  late final List<String> _parts;
  static final _placeholder = RegExp(r'%(?:\d+)?(?:\.\d+)?[diufs]');

  String? translate(String text) {
    final match = _pattern.firstMatch(text);
    if (match == null) return null;
    // Custom scripted frame names can contain '/'; do not interpret their parts.
    if (source == 'Frame: %s/%s' &&
        !ardupilotFrameClasses.containsKey(match.group(1)!.toUpperCase())) {
      return null;
    }
    final output = StringBuffer(_parts.first);
    for (var i = 1; i < _parts.length; i++) {
      var value = match.group(i)!;
      if (source == 'Frame: %s' || source == 'Frame: %s/%s') {
        final labels = i == 1 ? ardupilotFrameClasses : ardupilotFrameLayouts;
        value = labels[value.toUpperCase()] ?? value;
      }
      if (source == 'EKF Failsafe: changed to %s Mode' ||
          source == 'RC Short Failsafe: switched to %s' ||
          source == 'GCS Failsafe On: switched to %s' ||
          source == 'RC Long Failsafe On: switched to %s' ||
          source == 'Flight mode %s restored' ||
          source == '%s mode not armable' ||
          source == '%s not Q mode' ||
          source == '%s not in VTOL takeoff' ||
          source == '%s missing takeoff waypoint' ||
          (source == 'Mode change to %s failed: %s' && i == 1)) {
        value = _modeLabel(value);
      }
      if (source == 'Mode change to %s failed: %s' && i == 2) {
        value = _translateKnownMessage(value) ?? value;
      }
      if (source == 'Missing mission item: %s') {
        value =
            const {
              'land': '降落',
              'vtol land': '垂直降落',
              'do land start': '降落序列起点',
              'takeoff': '起飞',
              'vtol takeoff': '垂直起飞',
              'rtl': '返航',
            }[value.toLowerCase()] ??
            value;
      }
      output
        ..write(value)
        ..write(_parts[i]);
    }
    return output.toString();
  }
}
