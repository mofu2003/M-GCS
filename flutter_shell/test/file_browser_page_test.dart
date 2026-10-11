import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/logging/file_browser_page.dart';
import 'package:mgcs_frontend/logging/file_controller.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import 'support/file_backend_fake.dart';

Future<void> _pumpPage(WidgetTester tester, FileController controller) async {
  tester.view.physicalSize = const Size(1400, 1200);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
  await tester.pumpWidget(
    MacosTheme(
      data: MacosThemeData.light().copyWith(
        typography: MgTypography.macos(Brightness.light),
      ),
      child: MaterialApp(
        home: Scaffold(body: FileBrowserPage(controller: controller)),
      ),
    ),
  );
}

/// 反复 pump 小步直到条件成立；不用 pumpAndSettle，避免卡在活动指示器动画上。
Future<void> _pumpUntil(
  WidgetTester tester,
  bool Function() done, {
  int maxPumps = 300,
}) async {
  for (var i = 0; i < maxPumps && !done(); i++) {
    await tester.pump(const Duration(milliseconds: 8));
  }
  if (!done()) fail('界面未在预期内更新');
}

void main() {
  late FakeFileBackend backend;

  setUp(() {
    backend = FakeFileBackend();
  });

  FileController makeController() => FileController(
    backend: backend,
    indexStore: MemoryFileIndexStore(),
    statusPollInterval: const Duration(milliseconds: 4),
    listTimeout: const Duration(milliseconds: 400),
  )..bindSession(1);

  testWidgets('渲染目录条目：目录给「打开」、文件给「下载」', (tester) async {
    backend.statusQueue.add(
      fileStatus(
        'completed',
        items: [
          fileItem('LOGS', directory: true),
          fileItem('a.bin', size: 2048),
        ],
      ),
    );
    final controller = makeController();
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);
    await _pumpUntil(tester, () => find.text('a.bin').evaluate().isNotEmpty);

    expect(find.text('LOGS'), findsOneWidget);
    expect(find.text('a.bin'), findsOneWidget);
    expect(find.text('打开'), findsOneWidget);
    expect(find.text('下载'), findsOneWidget);
    expect(find.text('共 2 项'), findsOneWidget);
    // 实时列表不显示缓存提示。
    expect(find.textContaining('本地缓存的目录内容'), findsNothing);
    expect(tester.takeException(), isNull);
  });

  testWidgets('断链时展示本地缓存内容并给出过期提示', (tester) async {
    backend.statusQueue.add(
      fileStatus('completed', items: [fileItem('cached.bin', size: 9)]),
    );
    final controller = makeController();
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);
    await _pumpUntil(
      tester,
      () => find.text('cached.bin').evaluate().isNotEmpty,
    );

    backend.connected = false;
    await controller.refreshList();
    await _pumpUntil(
      tester,
      () => find.textContaining('本地缓存的目录内容').evaluate().isNotEmpty,
    );

    expect(find.text('cached.bin'), findsOneWidget);
    expect(find.textContaining('未连接飞控'), findsWidgets);
    expect(tester.takeException(), isNull);
  });

  testWidgets('传输进行中显示进度卡片与取消按钮', (tester) async {
    backend.persistentStatus = fileStatus('running');
    final controller = makeController();
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);
    await _pumpUntil(tester, () => find.text('读取目录中').evaluate().isNotEmpty);

    expect(find.byType(ProgressBar), findsOneWidget);
    expect(find.text('取消'), findsOneWidget);

    // 让目录请求结束，避免测试结束时留下待触发的定时器。
    backend.persistentStatus = fileStatus('completed');
    await _pumpUntil(tester, () => !controller.listing);
    expect(tester.takeException(), isNull);
  });
}
