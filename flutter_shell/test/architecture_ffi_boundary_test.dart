import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

/// A5：`dart:ffi` 只允许出现在 backend 层。
///
/// 仓库里没有字面的 `backend/` 目录，约定是「功能目录下的 `*_ffi_backend.dart`」
/// 加上共享原语 `connection/native_core.dart`。页面、控制器、组件都不得直接
/// 导入 `dart:ffi` 或持有 `Pointer`/`DynamicLibrary`。
///
/// 这条规则以前只写在 `architecture.md` 里靠人工检查，所以在重构期间被悄悄破坏过
/// （`connection_controller.dart` 曾同时导入 `dart:ffi`、`package:ffi` 与 `dart:io`）。
/// 改成测试后，越界会直接让门禁失败。
/// 允许导入 `dart:ffi` 的文件（相对 `lib/` 的路径）。
const _ffiAllowlist = <String>{
  'connection/native_core.dart',
  'connection/calibration_ffi_backend.dart',
  'connection/flight_command_ffi_backend.dart',
  'connection/parameter_ffi_backend.dart',
  'connection/telemetry_ffi_backend.dart',
  'fleet/fleet_ffi_backend.dart',
  'mission/mission_document_ffi_backend.dart',
};

/// 原生句柄的痕迹。出现在非白名单文件里即为 A5 违规。
final _handlePatterns = <RegExp>[
  RegExp(r"""import\s+'dart:ffi'"""),
  RegExp(r"""import\s+'package:ffi"""),
  RegExp(r'\bDynamicLibrary\b'),
  RegExp(r'\bPointer<'),
  RegExp(r'\btoNativeUtf8\b'),
];

void main() {
  final libDir = Directory('lib');

  test('A5：只有 backend 文件可以导入 dart:ffi', () {
    expect(libDir.existsSync(), isTrue, reason: '测试需在 flutter_shell 目录下运行');

    final violations = <String>[];
    final actual = <String>{};

    for (final entity in libDir.listSync(recursive: true)) {
      if (entity is! File || !entity.path.endsWith('.dart')) continue;
      final relative = entity.path.replaceFirst(RegExp(r'^lib/'), '');
      final source = entity.readAsStringSync();
      final usesFfi = source.contains("import 'dart:ffi'") ||
          source.contains("import 'package:ffi");
      if (!usesFfi) continue;
      actual.add(relative);
      if (!_ffiAllowlist.contains(relative)) {
        violations.add('$relative 导入了 dart:ffi，但不在 backend 白名单里');
      }
    }

    expect(violations, isEmpty, reason: violations.join('\n'));

    // 白名单只允许收紧或平移：被删掉的文件必须同步从名单移除，避免名单腐化。
    final stale = actual.difference(_ffiAllowlist);
    expect(stale, isEmpty, reason: '白名单里有不再导入 dart:ffi 的文件：$stale');
  });

  test('A5：控制器与页面不持有原生句柄', () {
    final violations = <String>[];

    for (final entity in libDir.listSync(recursive: true)) {
      if (entity is! File || !entity.path.endsWith('.dart')) continue;
      final relative = entity.path.replaceFirst(RegExp(r'^lib/'), '');
      if (_ffiAllowlist.contains(relative)) continue;
      // 仅注释里提到 dart:ffi 不算违规，这里找的是真实的原生句柄用法。
      final source = entity.readAsStringSync();
      for (final pattern in _handlePatterns) {
        if (pattern == _handlePatterns.first) continue; // 导入单独由上面那条守住
        if (pattern.hasMatch(source)) {
          violations.add('$relative 出现 ${pattern.pattern}');
        }
      }
    }

    expect(violations, isEmpty, reason: violations.join('\n'));
  });
}
