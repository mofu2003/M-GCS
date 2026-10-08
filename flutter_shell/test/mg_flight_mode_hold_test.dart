import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/flight/flight_mode_labels.dart';
import 'package:mgcs_frontend/widgets/mg_top_status_bar.dart';

void main() {
  group('flight mode Chinese labels', () {
    test('common ArduPilot modes are translated', () {
      expect(flightModeLabel('Guided'), '引导');
      expect(flightModeLabel('Loiter'), '盘旋');
      expect(flightModeLabel('QLoiter'), '多旋翼悬停');
      expect(flightModeLabel('RTL'), '返航');
      expect(flightModeLabel('Auto'), '自动航线');
    });

    test('unknown modes fall back to the firmware identifier', () {
      expect(flightModeLabel('SomeFutureMode'), 'SomeFutureMode');
    });
  });

  Future<void> show(WidgetTester tester, VoidCallback onTriggered) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Center(
              child: SizedBox(
                width: 250,
                child: MgFlightModeRow(
                  option: const FlightModeOption(
                    id: 12,
                    name: 'LOITER',
                    settable: true,
                  ),
                  selected: false,
                  onTriggered: onTriggered,
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  testWidgets('holding the row to full triggers the mode switch once', (
    tester,
  ) async {
    var triggered = 0;
    await show(tester, () => triggered++);

    final gesture = await tester.startGesture(
      tester.getCenter(find.byType(MgFlightModeRow)),
    );
    await tester.pump();
    // 进度拉满前不触发
    await tester.pump(const Duration(milliseconds: 400));
    expect(triggered, 0);

    // 超过 0.5 s 的按住时长后触发一次
    await tester.pump(const Duration(milliseconds: 300));
    await tester.pumpAndSettle();
    expect(triggered, 1);

    await gesture.up();
    await tester.pumpAndSettle();
    expect(triggered, 1);
  });

  testWidgets('releasing early cancels the switch', (tester) async {
    var triggered = 0;
    await show(tester, () => triggered++);

    final gesture = await tester.startGesture(
      tester.getCenter(find.byType(MgFlightModeRow)),
    );
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));
    await gesture.up();
    await tester.pumpAndSettle();

    expect(triggered, 0);
  });

  testWidgets('a plain tap does not switch the mode', (tester) async {
    var triggered = 0;
    await show(tester, () => triggered++);

    await tester.tap(find.byType(MgFlightModeRow));
    await tester.pumpAndSettle();

    expect(triggered, 0);
  });
}
