import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

/// 单个罗盘实例的实时磁场强度读数。
class MagneticFieldStrength extends StatelessWidget {
  const MagneticFieldStrength({
    super.key,
    required this.controller,
    required this.instance,
  });

  final ConnectionController controller;
  final WaveformInstance instance;

  @override
  Widget build(BuildContext context) {
    final values = controller.waveformMagLatest(instance.index);
    final finite = values.every((value) => value.isFinite);
    final magnitude = finite
        ? math.sqrt(
                values[0] * values[0] +
                    values[1] * values[1] +
                    values[2] * values[2],
              ) *
              1000
        : double.nan;
    return Text.rich(
      TextSpan(
        text: '磁力计${instance.index + 1} 磁场强度：',
        style: MacosTheme.of(context).typography.callout,
        children: [
          TextSpan(
            text: magnitude.isFinite ? magnitude.round().toString() : '—',
            style: MgTypography.sans(
              MgTypography.body.copyWith(color: context.mgColors.success),
            ),
          ),
        ],
      ),
    );
  }
}
