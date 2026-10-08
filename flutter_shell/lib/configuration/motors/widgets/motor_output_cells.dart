import 'dart:async';

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/mg_motor_output_bar.dart';

import '../../widgets/parameter_helpers.dart';
import '../../widgets/parameter_value_control.dart';
import '../../widgets/parameter_value_well.dart';

/// 电机参数表头单元格。
class HeaderCell extends StatelessWidget {
  const HeaderCell({super.key, required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: 42,
    child: Center(
      child: Text(
        label,
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(fontWeight: FontWeight.w600),
      ),
    ),
  );
}

/// 电机输出通道编号单元格。
class ChannelNumberCell extends StatelessWidget {
  const ChannelNumberCell({super.key, required this.number});

  final int number;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: Container(
        width: 24,
        height: 24,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: context.mgColors.accentSurface,
          shape: BoxShape.circle,
          border: Border.all(
            color: context.mgColors.accent.withValues(alpha: 0.35),
          ),
        ),
        child: Text(
          '$number',
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: context.mgColors.accent),
          ),
        ),
      ),
    ),
  );
}

/// 电机实时 PWM 输出单元格。
class LiveOutputCell extends StatelessWidget {
  const LiveOutputCell({
    super.key,
    required this.value,
    required this.minimum,
    required this.maximum,
  });

  final int? value;
  final double minimum;
  final double maximum;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 8),
      child: Center(
        child: MgMotorOutputBar(
          value: value,
          minimum: minimum,
          maximum: maximum,
          semanticLabel: 'PWM 当前输出',
        ),
      ),
    ),
  );
}

/// 电机输出反向参数单元格。
class ReverseCell extends StatelessWidget {
  const ReverseCell({
    super.key,
    required this.parameter,
    required this.controller,
  });

  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: parameter == null
          ? Text('—', style: MacosTheme.of(context).typography.body)
          : MacosSwitch(
              value: parameter!.value != 0,
              onChanged: parameter!.readOnly || !controller.isConnected
                  ? null
                  : (value) => unawaited(
                      controller.setParameter(parameter!.name, value ? 1 : 0),
                    ),
              size: ControlSize.small,
              semanticLabel: '反向输出',
            ),
    ),
  );
}

/// 电机输出功能枚举参数单元格。
class FunctionCell extends StatelessWidget {
  const FunctionCell({
    super.key,
    required this.parameter,
    required this.controller,
  });

  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 5),
        child: SizedBox(
          width: double.infinity,
          child: ParameterValueControl(
            parameter: parameter,
            controller: controller,
            center: false,
            popupWidth: 108,
          ),
        ),
      ),
    ),
  );
}

/// PWM 最小值、中位值与最大值的显示语义。
enum LimitTone { min, mid, max }

/// PWM 行程参数编辑单元格。
class LimitCell extends StatelessWidget {
  const LimitCell({
    super.key,
    required this.parameter,
    required this.tone,
    required this.controller,
  });

  final VehicleParameter? parameter;
  final LimitTone tone;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final toneColor = switch (tone) {
      LimitTone.min => colors.danger,
      LimitTone.mid => colors.accent,
      LimitTone.max => colors.success,
    };
    return SizedBox(
      height: MgMetrics.rowHeight,
      child: Center(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 5, vertical: 5),
          child: ParameterValueWell(
            text: parameter == null
                ? '—'
                : formatParameterValue(parameter!.value),
            enabled:
                parameter != null &&
                controller.isConnected &&
                !parameter!.readOnly,
            center: true,
            fillColor: toneColor.withValues(alpha: colors.isDark ? 0.28 : 0.12),
            onSubmitted: parameter == null
                ? null
                : (value) => controller.setParameter(parameter!.name, value),
          ),
        ),
      ),
    );
  }
}
