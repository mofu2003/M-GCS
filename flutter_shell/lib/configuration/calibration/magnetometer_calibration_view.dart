import 'package:flutter/material.dart';

import 'package:mgcs_frontend/components/realtime_waveform.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

import 'widgets/magnetometer_calibration_card.dart';
import 'widgets/magnetometer_options_card.dart';

/// 磁力计波形、安装选项与校准进度视图。
class MagnetometerCalibrationView extends StatelessWidget {
  const MagnetometerCalibrationView({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      controller.waveformChanges,
      controller.parameterChanges,
      controller.calibrationChanges,
    ]),
    builder: (context, _) => Column(
      children: [
        Flexible(
          child: SensorWaveformPanel(
            controller: controller,
            groups: const [SensorWaveformGroup.mag],
            instanceKind: WaveformInstanceKind.mag,
          ),
        ),
        const SizedBox(height: 8),
        Padding(
          padding: const EdgeInsets.symmetric(
            horizontal: kWaveformContentInset,
          ),
          child: Column(
            children: [
              MagnetometerOptionsCard(controller: controller),
              const SizedBox(height: 8),
              MagnetometerCalibrationCard(controller: controller),
            ],
          ),
        ),
      ],
    ),
  );
}
