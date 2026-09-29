import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_motor_output_bar.dart';

void main() {
  testWidgets('motor output bar shows PWM value and range position', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 240,
              child: MgMotorOutputBar(
                value: 1500,
                minimum: 1000,
                maximum: 2000,
                semanticLabel: 'PWM 当前输出',
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('1500'), findsOneWidget);
    final progress = tester.widget<ProgressBar>(find.byType(ProgressBar));
    expect(progress.value, closeTo(50, 0.001));
    expect(tester.getSize(find.byType(ProgressBar)).height, 24);
    expect(tester.takeException(), isNull);
  });

  testWidgets('motor output bar keeps an empty track without a PWM value', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: const MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 240,
              child: MgMotorOutputBar(
                value: null,
                minimum: 1000,
                maximum: 2000,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('—'), findsOneWidget);
    final progress = tester.widget<ProgressBar>(find.byType(ProgressBar));
    expect(progress.value, 0);
    expect(tester.getSize(find.byType(ProgressBar)).height, 24);
    expect(tester.takeException(), isNull);
  });
}
