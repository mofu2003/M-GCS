import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';

/// 带中位基准的范围条。
///
/// 舵机输出不是一个 0–100 的百分比，而是"最小值 / 中位值 / 最大值"三段范围，
/// 当前值落在其中哪一段才是有意义的读数，所以这里自绘而不是用 [ProgressBar]：
/// 只需要一条轨道、一个中位刻度和一个当前位置标记。
class MgRangeBar extends StatelessWidget {
  const MgRangeBar({
    super.key,
    required this.value,
    required this.minimum,
    required this.midpoint,
    required this.maximum,
    this.height = 7,
    this.semanticLabel,
  });

  final double? value;
  final double minimum;
  final double midpoint;
  final double maximum;
  final double height;
  final String? semanticLabel;

  double get _span {
    final span = maximum - minimum;
    return span.abs() < 1e-6 ? 1 : span;
  }

  double _ratio(double raw) => ((raw - minimum) / _span).clamp(0.0, 1.0);

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Semantics(
      label: semanticLabel,
      child: SizedBox(
        height: height,
        width: double.infinity,
        child: CustomPaint(
          painter: _MgRangeBarPainter(
            ratio: value == null ? null : _ratio(value!),
            midRatio: _ratio(midpoint),
            track: colors.hairline,
            fill: colors.accent,
            marker: colors.surface,
            markerBorder: colors.accent,
          ),
        ),
      ),
    );
  }
}

class _MgRangeBarPainter extends CustomPainter {
  const _MgRangeBarPainter({
    required this.ratio,
    required this.midRatio,
    required this.track,
    required this.fill,
    required this.marker,
    required this.markerBorder,
  });

  final double? ratio;
  final double midRatio;
  final Color track;
  final Color fill;
  final Color marker;
  final Color markerBorder;

  @override
  void paint(Canvas canvas, Size size) {
    final radius = Radius.circular(size.height / 2);
    final trackRect = Offset.zero & size;
    final trackPaint = Paint()..color = track;
    canvas.drawRRect(RRect.fromRectAndRadius(trackRect, radius), trackPaint);

    final current = ratio;
    if (current != null && current > 0) {
      final fillPaint = Paint()..color = fill;
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(0, 0, size.width * current, size.height),
          radius,
        ),
        fillPaint,
      );
    }

    // 中位刻度：一条竖向细线，提示"中位值"位置。
    final midX = (size.width * midRatio).clamp(0.0, size.width);
    final tickPaint = Paint()
      ..color = track
      ..strokeWidth = 1.5;
    canvas.drawLine(Offset(midX, -1), Offset(midX, size.height + 1), tickPaint);

    if (current == null) return;

    // 当前位置标记：白心圆点 + 强调色描边，空白值时不画。
    final markerRadius = size.height * 0.72;
    final center = Offset(
      (size.width * current).clamp(markerRadius, size.width - markerRadius),
      size.height / 2,
    );
    canvas.drawCircle(center, markerRadius, Paint()..color = marker);
    canvas.drawCircle(
      center,
      markerRadius,
      Paint()
        ..color = markerBorder
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.8,
    );
  }

  @override
  bool shouldRepaint(_MgRangeBarPainter oldDelegate) =>
      oldDelegate.ratio != ratio ||
      oldDelegate.midRatio != midRatio ||
      oldDelegate.track != track ||
      oldDelegate.fill != fill;
}
