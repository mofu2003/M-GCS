import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_mac_glass_attitude_panel.dart';

void main() {
  testWidgets('composite instrument renders heading and attitude ball', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: const MaterialApp(
          home: Scaffold(
            body: Center(
              child: MgMacGlassAttitudeCompass(
                headingDegrees: 41,
                rollDegrees: 12,
                pitchDegrees: -6,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('北041°'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('composite instrument tolerates invalid telemetry', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: const MaterialApp(
          home: Scaffold(
            body: Center(
              child: MgMacGlassAttitudeCompass(
                headingDegrees: double.nan,
                rollDegrees: double.infinity,
                pitchDegrees: double.negativeInfinity,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('北000°'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
