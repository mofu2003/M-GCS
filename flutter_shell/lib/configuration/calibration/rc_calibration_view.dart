import 'dart:async';

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/components/realtime_waveform.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';
import 'package:mgcs_frontend/widgets/mg_confirm.dart';
import 'package:mgcs_frontend/widgets/mg_motor_output_bar.dart';

/// 按角色 `rc.map.*` 找到通道当前承担的功能，用于给主通道加中文标注。
///
/// 实际固件参数名（`RCMAP_ROLL` 等）由后端按机型下发，前端只引用语义角色
/// （safety.md S6）。角色表异步到位，角色缺失时返回 null，等重建后再标注。
String? _rcChannelFunction(ConnectionController controller, int channel) {
  const functions = [
    ('rc.map.roll', '横滚'),
    ('rc.map.pitch', '俯仰'),
    ('rc.map.throttle', '油门'),
    ('rc.map.yaw', '偏航'),
  ];
  for (final (role, label) in functions) {
    final name = controller.roleParam(role);
    if (name == null) continue;
    final value = controller.parameters[name]?.value;
    if (value != null && value.round() == channel) return label;
  }
  return null;
}

/// 遥控器校准：实时行程 + 反向开关，校准指令由飞控采样行程。
class RcCalibrationView extends StatelessWidget {
  const RcCalibrationView({super.key, required this.controller});

  final ConnectionController controller;

  static const _primaryChannels = [1, 2, 3, 4];
  static const _auxiliaryChannels = [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      controller.parameterChanges,
      controller.telemetryChanges,
      controller.calibrationChanges,
    ]),
    builder: (context, _) {
      final theme = MacosTheme.of(context);
      final colors = context.mgColors;
      final status = controller.calibrationStatus;
      final radioActive = status.active && status.type == CalibrationType.radio;
      final error = controller.calibrationError;
      final message = error ?? (radioActive ? status.message : '');
      final hint = message.isNotEmpty
          ? message
          : radioActive
          ? '正在校准，请按提示打满各通道行程'
          : '点击开始校准后按提示打满各通道行程';

      return SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(
          kWaveformContentInset,
          8,
          kWaveformContentInset,
          20,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            MacPanel(
              padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      hint,
                      style: theme.typography.body.copyWith(
                        color: error != null
                            ? colors.danger
                            : colors.textSecondary,
                      ),
                    ),
                  ),
                  const SizedBox(width: 12),
                  if (radioActive)
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: status.cancelling
                          ? null
                          : () => unawaited(controller.cancelCalibration()),
                      child: const Text('取消'),
                    )
                  else
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: controller.isConnected && !status.active
                          ? () => unawaited(
                              _confirmStartRadioCalibration(
                                context,
                                controller,
                              ),
                            )
                          : null,
                      child: const Text('开始校准'),
                    ),
                ],
              ),
            ),
            const SizedBox(height: MgMetrics.configSectionGap),
            Text('主通道', style: theme.typography.title3),
            const SizedBox(height: MgMetrics.configHeaderGap),
            _RcChannelGrid(channels: _primaryChannels, controller: controller),
            const SizedBox(height: MgMetrics.configSectionGap),
            Text('辅助通道', style: theme.typography.title3),
            const SizedBox(height: MgMetrics.configHeaderGap),
            _RcChannelGrid(
              channels: _auxiliaryChannels,
              controller: controller,
            ),
          ],
        ),
      );
    },
  );
}

/// RC 校准启动确认（KD-10）：startCalibration 会让飞控进入校准流程。
Future<void> _confirmStartRadioCalibration(
  BuildContext context,
  ConnectionController controller,
) async {
  final confirmed = await showMgConfirmDialog(
    context,
    title: '开始 RC 校准？',
    message:
        '飞控会进入遥控校准流程，需要按提示拨动并回中所有摇杆与开关。'
        '中途可取消，取消会等待飞控确认后才停止。',
    confirmLabel: '开始校准',
  );
  if (!confirmed || !context.mounted) return;
  await controller.startCalibration(CalibrationType.radio);
}

class _RcChannelGrid extends StatelessWidget {
  const _RcChannelGrid({required this.channels, required this.controller});

  final List<int> channels;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 900 ? 2 : 1;
      final width =
          (constraints.maxWidth - (columns - 1) * MgMetrics.configCardGap) /
          columns;
      return Wrap(
        spacing: MgMetrics.configCardGap,
        runSpacing: 4,
        children: [
          for (final channel in channels)
            SizedBox(
              width: width,
              child: _RcChannelTile(channel: channel, controller: controller),
            ),
        ],
      );
    },
  );
}

class _RcChannelTile extends StatelessWidget {
  const _RcChannelTile({required this.channel, required this.controller});

  final int channel;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final function = _rcChannelFunction(controller, channel);
    final value =
        controller.rcChannelsValid && channel <= controller.rcChannels.length
        ? controller.rcChannels[channel - 1]
        : null;
    final reversed =
        (controller.parameters['RC${channel}_REVERSED']?.value ?? 0) != 0;
    final canReverse =
        controller.isConnected &&
        controller.parameters.containsKey('RC${channel}_REVERSED');

    return SizedBox(
      height: MgMetrics.rowHeight,
      child: Row(
        children: [
          SizedBox(
            width: 108,
            child: Text(
              function == null ? '通道$channel' : '通道$channel（$function）',
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: theme.typography.caption1,
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: MgMotorOutputBar(
              value: value,
              minimum: 800,
              maximum: 2200,
              semanticLabel: '通道$channel PWM',
            ),
          ),
          const SizedBox(width: 10),
          MacosSwitch(
            value: reversed,
            size: ControlSize.small,
            onChanged: canReverse
                ? (next) => unawaited(
                    controller.setParameter(
                      'RC${channel}_REVERSED',
                      next ? 1 : 0,
                    ),
                  )
                : null,
            semanticLabel: '通道$channel 反向',
          ),
        ],
      ),
    );
  }
}
