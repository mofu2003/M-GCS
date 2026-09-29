import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 电机 PWM 输出进度条。
///
/// 读数直接叠加在进度条右侧，填充宽度由 SERVO 输出范围驱动。
class MgMotorOutputBar extends StatelessWidget {
  const MgMotorOutputBar({
    super.key,
    required this.value,
    required this.minimum,
    required this.maximum,
    this.semanticLabel,
  });

  final int? value;
  final double minimum;
  final double maximum;
  final String? semanticLabel;

  double get _ratio {
    final current = value;
    if (current == null) return 0;
    final span = maximum - minimum;
    if (span.abs() < 1e-6) return 0;
    return ((current - minimum) / span).clamp(0.0, 1.0);
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final ratio = _ratio;
    final label = value?.toString() ?? '—';
    final textColor = ratio >= 0.35 ? colors.onMotorBar : colors.textPrimary;

    return Semantics(
      label: semanticLabel,
      value: label,
      child: SizedBox(
        height: MgMetrics.motorBarHeight,
        width: double.infinity,
        child: Stack(
          children: [
            SizedBox(
              width: double.infinity,
              height: MgMetrics.motorBarHeight,
              child: TweenAnimationBuilder<double>(
                tween: Tween<double>(end: ratio * 100),
                duration: const Duration(milliseconds: 240),
                curve: Curves.easeOutCubic,
                builder: (context, animated, _) => ProgressBar(
                  value: animated,
                  height: MgMetrics.motorBarHeight,
                  trackColor: colors.success,
                  backgroundColor: colors.surfaceMuted,
                  semanticLabel: semanticLabel,
                ),
              ),
            ),
            Align(
              alignment: Alignment.center,
              child: Text(
                label,
                // 条内读数比表格里的数值小一档：进度条只有 18 高，用 13 会塞满整条，
                // 看起来比最小值/最大值那些 30 高的格子里的同样字号更大。
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(
                    fontFeatures: const [FontFeature.tabularFigures()],
                  ),
                ).copyWith(color: textColor),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
