import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';

import 'calibration_aircraft.dart';

/// 六面或水平加速度计校准卡片，显示飞控状态与操作门控。
class AccelerometerCalibrationCard extends StatelessWidget {
  const AccelerometerCalibrationCard({
    super.key,
    required this.controller,
    required this.mode,
  });

  final ConnectionController controller;
  final AccelerometerCalibrationMode mode;

  bool get isSixFace => mode == AccelerometerCalibrationMode.sixFace;

  bool get isThisCalibrationActive {
    final status = controller.calibrationStatus;
    return status.active &&
        status.isAccelerometer &&
        (isSixFace
            ? status.accelerometerMode == 'six_face'
            : status.accelerometerMode == 'level');
  }

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    final status = controller.calibrationStatus;
    final activeOtherCalibration = status.active && !isThisCalibrationActive;
    final canStart =
        controller.isConnected &&
        !status.active &&
        controller.calibrationError == null;
    final step = isSixFace ? status.currentStep.clamp(1, 6) : 1;
    final orientation = isSixFace ? AccelOrientation.fromStep(step) : null;
    final busy = isThisCalibrationActive;
    final waiting = busy && status.isWaitingOrientation;
    final title = isSixFace ? '六面校准' : '水平校准';
    final subtitle = isSixFace ? '按飞控提示依次摆放，摆好后确认当前姿态' : '将飞机放在平整水平面上，保持静止后开始';
    final message = controller.calibrationError ?? (busy ? status.message : '');

    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          SizedBox(
            width: 150,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                CalibrationAircraft(orientation: orientation),
                const SizedBox(height: 6),
                Text(
                  orientation?.label ?? '水平放置',
                  style: theme.typography.caption1.copyWith(
                    color: colors.accent,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Row(
                  children: [
                    Text(title, style: theme.typography.title3),
                    const Spacer(),
                    if (busy && isSixFace)
                      Text(
                        '第 $step/6 面',
                        style: theme.typography.caption1.copyWith(
                          color: colors.accent,
                        ),
                      ),
                  ],
                ),
                const SizedBox(height: 4),
                Text(subtitle, style: theme.typography.caption1),
                const SizedBox(height: 12),
                if (busy) ...[
                  LinearProgressIndicator(
                    value: status.progress.clamp(0, 100) / 100,
                    minHeight: 5,
                    borderRadius: BorderRadius.circular(3),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    message.isEmpty
                        ? (isSixFace ? '请按图示摆放并保持静止' : '飞控正在执行水平校准')
                        : message,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: theme.typography.caption1,
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      if (isSixFace)
                        Expanded(
                          child: PushButton(
                            controlSize: ControlSize.large,
                            secondary: !waiting,
                            onPressed: waiting
                                ? () => controller.confirmAccelerometerPosition(
                                    step,
                                  )
                                : null,
                            child: Text(waiting ? '确认当前姿态' : '等待飞控提示'),
                          ),
                        )
                      else
                        Expanded(
                          child: Text(
                            '保持静止，等待飞控完成…',
                            style: theme.typography.caption1,
                          ),
                        ),
                      const SizedBox(width: 8),
                      PushButton(
                        controlSize: ControlSize.large,
                        secondary: true,
                        onPressed: () => controller.cancelCalibration(),
                        child: const Text('取消'),
                      ),
                    ],
                  ),
                ] else ...[
                  Text(
                    activeOtherCalibration
                        ? '另一项加速度计校准正在进行'
                        : controller.isConnected
                        ? '开始后请严格按照图示摆放飞机'
                        : '连接飞控后才能开始校准',
                    style: theme.typography.caption1,
                  ),
                  const SizedBox(height: 10),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: PushButton(
                      controlSize: ControlSize.large,
                      onPressed: canStart
                          ? () => controller.startAccelerometerCalibration(mode)
                          : null,
                      child: Text(isSixFace ? '开始六面校准' : '开始水平校准'),
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}
