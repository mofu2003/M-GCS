import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/mission/mission_export.dart';

import 'support/mission_backend_fake.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUpAll(() {
    // macos_ui 的 AccentColorListener 首次访问时会调用 appkit 平台通道；widget 测试
    // 里没有真实实现，注册一个内存回调，避免真实事件循环跑动时抛
    // MissingPluginException（文件导出依赖 runAsync 驱动真实 I/O）。
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(
          const MethodChannel('appkit_ui_element_colors'),
          (call) async => <String, dynamic>{'hueComponent': 0.6085324903200698},
        );
  });

  test('MissionInterchangeExport reads native json fields', () {
    final export = MissionInterchangeExport.fromJson({
      'format': 'plan',
      'item_count': 3,
      'text': '{"fileType":"Plan"}',
      'warnings': ['dropped param4'],
    });
    expect(export.format, 'plan');
    expect(export.itemCount, 3);
    expect(export.text, '{"fileType":"Plan"}');
    expect(export.warnings, ['dropped param4']);

    final empty = MissionInterchangeExport.fromJson(const {});
    expect(empty.format, '');
    expect(empty.itemCount, 0);
    expect(empty.text, '');
    expect(empty.warnings, isEmpty);
  });

  test('defaultMissionExportFileName stamps the timestamp', () {
    final name = defaultMissionExportFileName(
      '.plan',
      now: DateTime(2026, 10, 11, 9, 5, 3),
    );
    expect(name, 'mission-20261011-090503.plan');
  });

  testWidgets('mission export sheet writes waypoints text to disk', (
    tester,
  ) async {
    final directory = _tempDirectory();
    final backend = FakeMissionBackend();
    backend.interchangeText = 'QGC WPL 110\n0\t1\t0\t16\t...\n';
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    await controller.initialize();
    await controller.addEntity(
      const MissionEntity(
        type: 'waypoint',
        latitude: 31.2,
        longitude: 121.4,
        altitudeMeters: 50,
      ),
    );

    MissionExportResult? result;
    await _pumpHost(
      tester,
      controller: controller,
      directory: directory,
      onResult: (value) => result = value,
    );
    await tester.tap(find.text('打开导出'));
    await _pumpFrames(tester);

    await _tapExport(tester, done: () => result != null);
    await _pumpFrames(tester);

    expect(result, isNotNull);
    expect(result!.path, endsWith('.waypoints'));
    final file = File(result!.path);
    expect(file.existsSync(), isTrue);
    expect(file.readAsStringSync(), backend.interchangeText);
    expect(result!.itemCount, 1);
    expect(backend.lastInterchangeFormat, 'waypoints');
  });

  testWidgets('switching to plan changes extension and export format', (
    tester,
  ) async {
    final directory = _tempDirectory();
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    await controller.initialize();

    MissionExportResult? result;
    await _pumpHost(
      tester,
      controller: controller,
      directory: directory,
      onResult: (value) => result = value,
    );
    await tester.tap(find.text('打开导出'));
    await _pumpFrames(tester);

    // 默认 waypoints：切到 QGC .plan 后，未改动的路径扩展名跟着变。
    final field = find.byType(MacosTextField);
    expect(
      tester.widget<MacosTextField>(field).controller!.text,
      endsWith('.waypoints'),
    );

    await tester.tap(find.text('QGC .plan'));
    await _pumpFrames(tester);
    expect(
      tester.widget<MacosTextField>(field).controller!.text,
      endsWith('.plan'),
    );

    await _tapExport(tester, done: () => result != null);
    await _pumpFrames(tester);

    expect(result, isNotNull);
    expect(result!.path, endsWith('.plan'));
    expect(backend.lastInterchangeFormat, 'plan');
  });

  testWidgets('export sheet surfaces native warnings', (tester) async {
    final directory = _tempDirectory();
    final backend = FakeMissionBackend();
    backend.interchangeWarnings = const ['复杂项已降级为航点'];
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    await controller.initialize();

    MissionExportResult? result;
    await _pumpHost(
      tester,
      controller: controller,
      directory: directory,
      onResult: (value) => result = value,
    );
    await tester.tap(find.text('打开导出'));
    await _pumpFrames(tester);
    await _tapExport(tester, done: () => result != null);
    await _pumpFrames(tester);

    expect(result!.warnings, ['复杂项已降级为航点']);
  });

  testWidgets('empty path shows a validation error', (tester) async {
    final directory = _tempDirectory();
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    await controller.initialize();

    MissionExportResult? result;
    await _pumpHost(
      tester,
      controller: controller,
      directory: directory,
      onResult: (value) => result = value,
    );
    await tester.tap(find.text('打开导出'));
    await _pumpFrames(tester);

    await tester.enterText(find.byType(MacosTextField), '');
    await tester.tap(find.text('导出'));
    await tester.pump();

    expect(find.text('请填写导出路径'), findsOneWidget);
    expect(result, isNull);
    expect(backend.lastInterchangeFormat, isNull);
  });
}

Directory _tempDirectory() {
  final directory = Directory.systemTemp.createTempSync('mg-export-');
  addTearDown(() {
    if (directory.existsSync()) directory.deleteSync(recursive: true);
  });
  return directory;
}

/// 点击「导出」并在真实事件循环里等待文件写入完成。
/// 点「导出」并等到落盘完成（[done] 由调用方的 `onResult` 置位）。
///
/// 导出走的是真实文件 I/O，只在 `runAsync` 的真实时间里推进。原先写死等 200 ms，
/// 在负载下（例如 preflight 一边编译一边跑测试）不够用，`result` 仍为 null 就断言
/// 失败。改为轮询到条件成立，最长 5 s。
Future<void> _tapExport(
  WidgetTester tester, {
  required bool Function() done,
}) => tester.runAsync(() async {
  await tester.tap(find.text('导出'));
  final deadline = DateTime.now().add(const Duration(seconds: 5));
  while (!done() && DateTime.now().isBefore(deadline)) {
    await Future<void>.delayed(const Duration(milliseconds: 10));
  }
});

/// 逐帧推进动画，避开文本光标与分段控件指示器的持续 ticker（不能用 pumpAndSettle）。
Future<void> _pumpFrames(WidgetTester tester) async {
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 80));
  await tester.pump(const Duration(milliseconds: 400));
  await tester.pump(const Duration(milliseconds: 400));
}

Future<void> _pumpHost(
  WidgetTester tester, {
  required MissionDocumentController controller,
  required Directory directory,
  required ValueChanged<MissionExportResult?> onResult,
}) {
  return tester.pumpWidget(
    MacosTheme(
      data: MacosThemeData.light(),
      child: MaterialApp(
        // 与 MacosApp 一致：给整棵树（含 dialog overlay）一个默认文本样式。
        builder: (context, child) => DefaultTextStyle(
          style: MacosTheme.of(context).typography.body,
          child: child!,
        ),
        home: Builder(
          builder: (context) => Scaffold(
            body: Center(
              child: TextButton(
                onPressed: () async {
                  final result = await showMissionExportSheet(
                    context: context,
                    controller: controller,
                    directoryProvider: () async => directory,
                  );
                  onResult(result);
                },
                child: const Text('打开导出'),
              ),
            ),
          ),
        ),
      ),
    ),
  );
}
