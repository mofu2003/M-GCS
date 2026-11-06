import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/configuration/calibration/widgets/accelerometer_calibration_card.dart';
import 'package:mgcs_frontend/configuration/calibration/widgets/calibration_aircraft.dart';

void main() {
  for (final width in [360.0, 500.0]) {
    for (final step in [0, 1, 2, 3, 4, 5, 6]) {
      testWidgets('six-face guide fits width $width at step $step', (
        tester,
      ) async {
        final controller = ConnectionController()
          ..calibrationStatus = CalibrationStatus(
            active: true,
            type: CalibrationType.accelerometer,
            accelerometerMode: 'six_face',
            stage: 'waiting_orientation',
            currentStep: step,
            progress: 20,
          );
        await tester.pumpWidget(
          MacosApp(
            home: Scaffold(
              body: Center(
                child: SizedBox(
                  width: width,
                  height: width < 460 ? 460 : 320,
                  child: AccelerometerCalibrationCard(
                    controller: controller,
                    mode: AccelerometerCalibrationMode.sixFace,
                  ),
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.byType(CalibrationAircraft), findsOneWidget);
        for (final pose in AccelOrientation.values) {
          expect(find.text('${pose.index + 1} ${pose.label}'), findsOneWidget);
        }
        expect(find.text(step == 0 ? '等待飞控提示' : '确认当前姿态'), findsOneWidget);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
        controller.dispose();
      });
    }
  }
}
