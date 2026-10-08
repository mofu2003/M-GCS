import 'package:flutter/material.dart';

import 'package:mgcs_frontend/components/realtime_waveform.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

import 'widgets/accelerometer_calibration_card.dart';

/// 加速度计波形与六面、水平校准视图。
class AccelerometerCalibrationView extends StatelessWidget {
  const AccelerometerCalibrationView({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      // 用 loose 约束让波形区贴合自身内容高度：之前用 Expanded 会把它撑满，
      // 内容不足时图表下方会空出一大片，把校准卡片推得很远。
      Flexible(
        child: SensorWaveformPanel(
          controller: controller,
          groups: const [SensorWaveformGroup.accel, SensorWaveformGroup.gyro],
          instanceKind: WaveformInstanceKind.imu,
        ),
      ),
      const SizedBox(height: 8),
      // 左右内边距和卡片间距与上面的波形卡片保持一致，两行卡片左右边界对齐。
      Padding(
        padding: const EdgeInsets.symmetric(horizontal: kWaveformContentInset),
        child: SizedBox(
          height: 224,
          child: AnimatedBuilder(
            animation: controller.calibrationChanges,
            builder: (context, _) => Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(
                  child: AccelerometerCalibrationCard(
                    controller: controller,
                    mode: AccelerometerCalibrationMode.sixFace,
                  ),
                ),
                const SizedBox(width: kWaveformCardGap),
                Expanded(
                  child: AccelerometerCalibrationCard(
                    controller: controller,
                    mode: AccelerometerCalibrationMode.level,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    ],
  );
}
