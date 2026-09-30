import 'package:flutter/material.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_animated_number.dart';

/// 地图左上角的飞行遥测悬浮窗。
///
/// 只展示 QGC / MP 飞行页最常用的即时读数，保持轻量；数据全部来自现有
/// `ConnectionController` 的 10Hz 遥测轮询，不引入新的取数链路。
class FlightTelemetryOverlay extends StatelessWidget {
  const FlightTelemetryOverlay({
    super.key,
    required this.controller,
    this.maxWidth = 680,
  });

  final ConnectionController controller;
  final double maxWidth;

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: controller.telemetryChanges,
      builder: (context, _) {
        final colors = MgColors.of(context);
        return ConstrainedBox(
          constraints: BoxConstraints(maxWidth: maxWidth),
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: colors.mapControlSurface,
              borderRadius: BorderRadius.circular(9),
              border: Border.all(color: colors.divider),
              boxShadow: [
                BoxShadow(
                  color: colors.shadow,
                  blurRadius: 18,
                  offset: const Offset(0, 5),
                ),
              ],
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
              child: Wrap(
                spacing: 16,
                runSpacing: 6,
                children: [
                  _TelemetryMetric(
                    label: 'GPS',
                    value: _clean(controller.satellitesVisible?.toDouble()),
                    fractionDigits: 0,
                  ),
                  _TelemetryMetric(
                    label: '航向',
                    value: _clean(controller.headingDegrees),
                    fractionDigits: 0,
                    unit: '°',
                  ),
                  _TelemetryMetric(
                    label: '空速',
                    value: _clean(controller.airspeedMetersPerSecond),
                    fractionDigits: 1,
                    unit: 'm/s',
                  ),
                  _TelemetryMetric(
                    label: '地速',
                    value: _clean(controller.groundSpeedMetersPerSecond),
                    fractionDigits: 1,
                    unit: 'm/s',
                  ),
                  _TelemetryMetric(
                    label: '相对高度',
                    value: _clean(controller.relativeAltitudeMeters),
                    fractionDigits: 1,
                    unit: 'm',
                  ),
                  _TelemetryMetric(
                    label: '绝对高度',
                    value: _clean(controller.absoluteAltitudeMeters),
                    fractionDigits: 1,
                    unit: 'm',
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }

  static double? _clean(double? value) {
    if (value == null || !value.isFinite) return value;
    return value.abs() < 0.05 ? 0.0 : value;
  }
}

class _TelemetryMetric extends StatelessWidget {
  const _TelemetryMetric({
    required this.label,
    required this.value,
    required this.fractionDigits,
    this.unit,
  });

  final String label;
  final double? value;
  final int fractionDigits;
  final String? unit;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return Row(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.baseline,
      textBaseline: TextBaseline.alphabetic,
      children: [
        Text(
          label,
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textSecondary),
          ),
        ),
        const SizedBox(width: 6),
        MgAnimatedNumber(
          value: value ?? double.nan,
          fractionDigits: fractionDigits,
          style: MgTypography.dataLarge,
          color: colors.textPrimary,
          placeholder: '—',
        ),
        if (unit != null) ...[
          const SizedBox(width: 3),
          Text(
            unit!,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ],
    );
  }
}
