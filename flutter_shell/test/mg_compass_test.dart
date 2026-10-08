import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_compass.dart';

void main() {
  Future<void> show(
    WidgetTester tester,
    double? heading, {
    double? roll,
    double? pitch,
  }) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Center(
              child: MgCompass(
                headingDegrees: heading,
                rollDegrees: roll,
                pitchDegrees: pitch,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
  }

  testWidgets('compass shows the Chinese direction and rounded heading', (
    tester,
  ) async {
    await show(tester, 41);
    expect(find.text('东北41°'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('compass renders the integrated attitude ball', (tester) async {
    await show(tester, 41, roll: 12, pitch: -6);
    expect(find.text('东北41°'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('compass handles cardinal headings and invalid data', (
    tester,
  ) async {
    await show(tester, 0);
    expect(find.text('北0°'), findsOneWidget);

    await show(tester, 90);
    expect(find.text('东90°'), findsOneWidget);

    await show(tester, 300);
    expect(find.text('西北300°'), findsOneWidget);

    await show(tester, null);
    expect(find.text('航向 --'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
