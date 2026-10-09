import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_command_confirmation.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

void main() {
  Future<void> show(
    WidgetTester tester, {
    required String label,
    required VoidCallback onConfirmed,
  }) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Center(
              child: MgHoldToConfirmButton(
                label: label,
                onConfirmed: onConfirmed,
              ),
            ),
          ),
        ),
      ),
    );
  }

  testWidgets('releasing before the hold completes cancels confirmation', (
    tester,
  ) async {
    var confirmed = 0;
    await show(tester, label: '返航', onConfirmed: () => confirmed++);

    final gesture = await tester.startGesture(
      tester.getCenter(find.byType(MgHoldToConfirmButton)),
    );
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));
    await gesture.up();
    await tester.pumpAndSettle();

    expect(confirmed, 0);
  });

  testWidgets('holding to full confirms the command once', (tester) async {
    var confirmed = 0;
    await show(tester, label: '开始任务', onConfirmed: () => confirmed++);

    final gesture = await tester.startGesture(
      tester.getCenter(find.byType(MgHoldToConfirmButton)),
    );
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 400));
    expect(confirmed, 0);
    expect(find.byIcon(CupertinoIcons.check_mark_circled_solid), findsNothing);

    // 填充完成，对勾开始动画，但指令要等对勾动画结束后才执行。
    await tester.pump(const Duration(milliseconds: 200));
    expect(
      find.byIcon(CupertinoIcons.check_mark_circled_solid),
      findsOneWidget,
    );
    expect(confirmed, 0);

    await tester.pump(const Duration(milliseconds: 200));
    expect(confirmed, 1);

    await gesture.up();
    await tester.pumpAndSettle();
    expect(confirmed, 1);
  });

  testWidgets('floating confirmation is centred below the map top edge', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(800, 600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    var cancelled = 0;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Stack(
              children: [
                Positioned.fill(child: ColoredBox(color: Colors.white)),
                MgCommandConfirmationOverlay(
                  command: MgCommandConfirmation(
                    label: '返航',
                    detail: '2 架飞机 · 系统 1、2',
                    execute: () async {},
                  ),
                  onConfirmed: () {},
                  onCancelled: () => cancelled++,
                ),
              ],
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('返航'), findsOneWidget);
    expect(find.text('2 架飞机 · 系统 1、2'), findsOneWidget);
    expect(find.text('长按确认'), findsOneWidget);

    final panel = find.byKey(
      const ValueKey('command-confirmation-overlay-row'),
    );
    expect(
      tester.getTopLeft(panel).dy,
      moreOrLessEquals(MgMetrics.commandConfirmTopInset),
    );
    expect(tester.getCenter(panel).dx, moreOrLessEquals(400));

    await tester.tap(find.byKey(const ValueKey('cancel-command-confirmation')));
    await tester.pumpAndSettle();
    expect(cancelled, 1);
    expect(tester.takeException(), isNull);
  });

  testWidgets('the confirmation label follows the pending command', (
    tester,
  ) async {
    await show(tester, label: '移动到此处', onConfirmed: () {});

    expect(find.text('移动到此处'), findsOneWidget);
    expect(find.text('长按确认'), findsOneWidget);

    await show(tester, label: '返航', onConfirmed: () {});
    expect(find.text('移动到此处'), findsNothing);
    expect(find.text('返航'), findsOneWidget);
    expect(find.text('长按确认'), findsOneWidget);
  });
}
