import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

Map<String, dynamic> _loadCatalog(String locale) =>
    jsonDecode(File('lib/l10n/app_$locale.arb').readAsStringSync())
        as Map<String, dynamic>;

Map<String, dynamic> _metadata(Map<String, dynamic> catalog, String key) =>
    (catalog['@$key'] as Map<String, dynamic>?) ?? const <String, dynamic>{};

Set<String> _placeholders(Map<String, dynamic> catalog, String key) {
  final placeholders = _metadata(catalog, key)['placeholders'];
  if (placeholders is! Map<String, dynamic>) return const <String>{};
  return placeholders.keys.toSet();
}

void main() {
  test('Chinese and English catalogs have identical keys and placeholders', () {
    final zh = _loadCatalog('zh');
    final en = _loadCatalog('en');
    final zhKeys = zh.keys.where((key) => !key.startsWith('@')).toSet();
    final enKeys = en.keys.where((key) => !key.startsWith('@')).toSet();

    expect(zhKeys, enKeys);
    for (final key in zhKeys) {
      expect(zh[key], isA<String>(), reason: 'Chinese value missing for $key');
      expect(en[key], isA<String>(), reason: 'English value missing for $key');
      expect((zh[key] as String).trim(), isNotEmpty, reason: 'Empty zh.$key');
      expect((en[key] as String).trim(), isNotEmpty, reason: 'Empty en.$key');
      expect(
        _placeholders(zh, key),
        _placeholders(en, key),
        reason: 'Placeholder mismatch for $key',
      );
    }
  });

  test('protocol and product terminology stays aligned', () {
    final zh = _loadCatalog('zh');
    final en = _loadCatalog('en');
    const expected = <String, List<String>>{
      'serial': ['Serial', 'Serial'],
      'udp': ['UDP', 'UDP'],
      'tcp': ['TCP', 'TCP'],
      'baudRate': ['波特率', 'Baud rate'],
      'portConfiguration': ['端口配置', 'Port configuration'],
      'flightLogs': ['飞行日志', 'Flight logs'],
      'motorOutputs': ['电机输出', 'Motor outputs'],
      'remoteControl': ['遥控配置', 'Remote control setup'],
      'pidTuning': ['PID 调参', 'PID tuning'],
      'mission': ['航点任务', 'Waypoint mission'],
      'addWaypoint': ['添加航点', 'Add waypoint'],
      'armed': ['已解锁', 'Armed'],
      'disarmed': ['未解锁', 'Disarmed'],
    };

    for (final entry in expected.entries) {
      expect(zh[entry.key], entry.value[0], reason: 'zh.${entry.key}');
      expect(en[entry.key], entry.value[1], reason: 'en.${entry.key}');
    }
  });
}
