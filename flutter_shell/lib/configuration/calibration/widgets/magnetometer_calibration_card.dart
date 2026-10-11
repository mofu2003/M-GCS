import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/adaptive_popup_button.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';

import '../../widgets/parameter_helpers.dart';
import 'calibration_progress.dart';
import 'magnetic_field_strength.dart';

/// 磁力计校准控制卡片，显示飞控失败原因与各罗盘进度。
class MagnetometerCalibrationCard extends StatelessWidget {
  const MagnetometerCalibrationCard({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final status = controller.calibrationStatus;
    final calibrating =
        status.active && status.type == CalibrationType.magnetometer;
    final instances = controller.waveformMagInstances;
    final tolerance = controller.parameter('COMPASS_CAL_FIT');
    // 失败原因优先用控制器里的一次性错误，其次用飞控回传的原因。
    final failure = controller.calibrationError ?? status.failureReason;
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Text('容差等级', style: MacosTheme.of(context).typography.callout),
              const SizedBox(width: 10),
              SizedBox(
                width: 150,
                // 容差等级就是 COMPASS_CAL_FIT：4 = 最严、8 = 严格、16 = 默认、32 = 宽松。
                // 选项同样来自飞控描述库，与 MP 的 mavlinkComboBoxfitness 一致。
                child: AdaptivePopupButton<int>(
                  value: tolerance?.value.round() ?? 16,
                  options: enumOptionsFor(tolerance, const [4, 8, 16, 32]),
                  labelOf: (value) => enumLabelFor(tolerance, value),
                  onChanged: controller.isConnected
                      ? (value) => controller.setParameter(
                          'COMPASS_CAL_FIT',
                          value.toDouble(),
                        )
                      : null,
                ),
              ),
              const SizedBox(width: 18),
              Expanded(
                child: Wrap(
                  spacing: 18,
                  runSpacing: 4,
                  children: [
                    for (final instance in instances)
                      MagneticFieldStrength(
                        controller: controller,
                        instance: instance,
                      ),
                  ],
                ),
              ),
              // 两个按钮作为一组，整体和行内其它控件居中对齐。
              Row(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.center,
                children: [
                  PushButton(
                    controlSize: ControlSize.large,
                    onPressed: controller.isConnected && !status.active
                        ? () => controller.startCalibration(
                            CalibrationType.magnetometer,
                          )
                        : null,
                    child: const Text('开始校准'),
                  ),
                  const SizedBox(width: 8),
                  PushButton(
                    controlSize: ControlSize.large,
                    secondary: true,
                    onPressed: calibrating && !status.cancelling
                        ? () => controller.cancelCalibration()
                        : null,
                    child: const Text('取消'),
                  ),
                ],
              ),
            ],
          ),
          // 失败原因必须显式显示：以前这里什么都不提示，用户只看到"按钮没反应"。
          if (!calibrating && failure.isNotEmpty) ...[
            const SizedBox(height: 8),
            Text(
              failure,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: context.mgColors.danger),
              ),
            ),
          ],
          // 校准进行中才出现：进度来自飞控的 MAG_CAL_PROGRESS。
          if (calibrating) ...[
            const SizedBox(height: 10),
            CalibrationProgress(
              instances: instances,
              progressByCompass: status.compassProgress,
              fallbackProgress: status.progress,
            ),
          ],
        ],
      ),
    );
  }
}
