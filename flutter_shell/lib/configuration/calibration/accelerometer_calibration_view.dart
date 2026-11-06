import 'package:flutter/material.dart';

import 'package:mgcs_frontend/components/realtime_waveform.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

import 'widgets/accelerometer_calibration_card.dart';

/// 加速度计波形与六面、水平校准视图。
class AccelerometerCalibrationView extends StatelessWidget {
  const AccelerometerCalibrationView({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final sideBySide = constraints.maxWidth >= 960;
      final compactCard = constraints.maxWidth < 460;
      Widget card(AccelerometerCalibrationMode mode, double height) => SizedBox(
        height: height,
        child: AccelerometerCalibrationCard(controller: controller, mode: mode),
      );
      return SingleChildScrollView(
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.symmetric(
                horizontal: kWaveformContentInset,
                vertical: 12,
              ),
              child: AnimatedBuilder(
                animation: controller.calibrationChanges,
                builder: (context, _) => sideBySide
                    ? Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Expanded(
                            child: card(
                              AccelerometerCalibrationMode.sixFace,
                              320,
                            ),
                          ),
                          const SizedBox(width: kWaveformCardGap),
                          Expanded(
                            child: card(
                              AccelerometerCalibrationMode.level,
                              320,
                            ),
                          ),
                        ],
                      )
                    : Column(
                        children: [
                          card(
                            AccelerometerCalibrationMode.sixFace,
                            compactCard ? 460 : 300,
                          ),
                          const SizedBox(height: kWaveformCardGap),
                          card(
                            AccelerometerCalibrationMode.level,
                            compactCard ? 360 : 224,
                          ),
                        ],
                      ),
              ),
            ),
            SizedBox(
              height: 300,
              child: SensorWaveformPanel(
                controller: controller,
                groups: const [
                  SensorWaveformGroup.accel,
                  SensorWaveformGroup.gyro,
                ],
                instanceKind: WaveformInstanceKind.imu,
              ),
            ),
          ],
        ),
      );
    },
  );
}
