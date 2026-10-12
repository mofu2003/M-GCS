import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/native_core.dart';

/// 覆盖 7 个 backend 曾经各自复制的那份 `_read(Pointer<Utf8>)`。
///
/// 合并前这 7 份实现有三处必须完全一致的语义：空指针处理、原生字符串的释放
/// 时机、非 Map 响应怎么报错。下面先钉住这三条，再钉住「不许再复制一份」。
void main() {
  group('readJsonMap：后端 JSON 响应解析（合并后唯一实现）', () {
    test('空指针抛 StateError，且不调用释放钩子', () {
      var freed = 0;
      expect(
        () => readJsonMap(
          nullptr,
          freeString: (_) => freed++,
          invalidResponseMessage: '格式无效',
        ),
        throwsA(isA<StateError>()),
      );
      expect(freed, 0, reason: '没有原生字符串可释放，释放钩子不得被调用');
    });

    test('对象响应解码成 Map<String, Object?> 并释放一次', () {
      final pointer = jsonEncode({'success': true, 'count': 2}).toNativeUtf8();
      var freed = 0;
      final result = readJsonMap(
        pointer,
        freeString: (p) {
          freed++;
          malloc.free(p);
        },
        invalidResponseMessage: '格式无效',
      );

      expect(result, {'success': true, 'count': 2});
      expect(result, isA<Map<String, Object?>>());
      expect(freed, 1);
    });

    test('非对象响应抛 FormatException（用调用方给的消息），仍然释放', () {
      for (final payload in <String>['[]', '"text"', '3', 'null']) {
        final pointer = payload.toNativeUtf8();
        var freed = 0;
        expect(
          () => readJsonMap(
            pointer,
            freeString: (p) {
              freed++;
              malloc.free(p);
            },
            invalidResponseMessage: '校准响应格式无效',
          ),
          throwsA(
            isA<FormatException>().having(
              (e) => e.message,
              'message',
              '校准响应格式无效',
            ),
          ),
          reason: 'payload=$payload',
        );
        expect(freed, 1, reason: 'payload=$payload');
      }
    });

    test('非法 JSON 抛 FormatException，且释放发生在抛错之前', () {
      final pointer = '{not json'.toNativeUtf8();
      var freed = 0;
      Object? captured;
      try {
        readJsonMap(
          pointer,
          freeString: (p) {
            freed++;
            malloc.free(p);
          },
          invalidResponseMessage: '日志响应格式无效',
        );
      } catch (error) {
        captured = error;
      }

      expect(captured, isA<FormatException>());
      expect(freed, 1, reason: '解码失败也必须释放原生字符串（ffi-contract F3）');
    });

    test('空对象是合法响应', () {
      final pointer = '{}'.toNativeUtf8();
      var freed = 0;
      final result = readJsonMap(
        pointer,
        freeString: (p) {
          freed++;
          malloc.free(p);
        },
        invalidResponseMessage: '格式无效',
      );
      expect(result, isEmpty);
      expect(freed, 1);
    });
  });

  group('合并后的调用点：不再各自复制 _read', () {
    // 合并的 7 份：调用共享 helper，文件里不再有本地 jsonDecode。
    const merged = <String>[
      'lib/connection/calibration_ffi_backend.dart',
      'lib/connection/telemetry_ffi_backend.dart',
      'lib/payload/payload_ffi_backend.dart',
      'lib/terrain/terrain_ffi_backend.dart',
      'lib/logging/tlog_ffi_backend.dart',
      'lib/logging/file_ffi_backend.dart',
      'lib/logging/log_ffi_backend.dart',
    ];

    // 刻意保留的 3 份：契约不同，不能被"顺手统一"。
    const kept = <String>[
      'lib/connection/parameter_ffi_backend.dart', // _readRaw 还服务 all() 的非 Map 原始 JSON
      'lib/connection/flight_command_ffi_backend.dart', // 构造函数可注入释放钩子
      'lib/fleet/fleet_ffi_backend.dart', // 返回 Object? 契约不同
    ];

    test('7 个 backend 调用共享 helper，且没有本地 jsonDecode', () {
      final libDir = Directory('lib');
      expect(libDir.existsSync(), isTrue, reason: '测试需在 flutter_shell 目录下运行');

      for (final path in merged) {
        final source = File(path).readAsStringSync();
        expect(
          source.contains('readJsonMap('),
          isTrue,
          reason: '$path 应调用 connection/native_core.dart 的 readJsonMap',
        );
        expect(
          RegExp(r'jsonDecode\(').hasMatch(source),
          isFalse,
          reason: '$path 仍有本地 JSON 解析实现（重复实现不允许回流）',
        );
      }
    });

    test('保留的 3 份仍有各自实现（有真实差异，不许被合并）', () {
      final parameter = File(kept[0]).readAsStringSync();
      expect(parameter.contains('Object? _readRaw('), isTrue);
      expect(parameter.contains('jsonDecode('), isTrue);

      final flightCommand = File(kept[1]).readAsStringSync();
      expect(flightCommand.contains('_freeStringOverride'), isTrue);
      expect(flightCommand.contains('jsonDecode('), isTrue);

      final fleet = File(kept[2]).readAsStringSync();
      expect(fleet.contains('Object? _read('), isTrue);
      expect(fleet.contains('jsonDecode('), isTrue);
    });
  });
}
