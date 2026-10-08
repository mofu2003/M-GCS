import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 姿态仪（人工地平仪）。
///
/// 和罗盘配成一对放在右下角：罗盘看"机头朝哪"，姿态仪看"机体怎么倾斜"。
/// 数据来自 `ATTITUDE`（`rollDegrees` / `pitchDegrees`，后端已换算成度）。
///
/// 画法沿用航电惯例：地平线随横滚反向旋转、随俯仰上下平移，机翼符号固定在
/// 表盘中央当基准；天地用 [MgColors.attitudeSky] / [MgColors.attitudeGround]
/// 这对仪表专用色，不借用界面强调色。
class MgAttitudeIndicator extends StatelessWidget {
  const MgAttitudeIndicator({
    super.key,
    required this.rollDegrees,
    required this.pitchDegrees,
  });

  final double? rollDegrees;
  final double? pitchDegrees;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return SizedBox(
      width: MgMetrics.attitudeSize,
      height: MgMetrics.attitudeSize,
      child: CustomPaint(
        painter: _AttitudePainter(
          rollDegrees: rollDegrees,
          pitchDegrees: pitchDegrees,
          skyColor: colors.attitudeSky,
          groundColor: colors.attitudeGround,
          horizonColor: colors.surface,
          ladderColor: colors.surface.withValues(alpha: 0.75),
          borderColor: colors.divider,
          hairlineColor: colors.hairline,
          dialColor: colors.floatingSurface,
          aircraftColor: colors.textPrimary,
          labelStyle: MgTypography.sans(
            MgTypography.caption.copyWith(height: 1, color: colors.surface),
          ),
        ),
      ),
    );
  }
}

class _AttitudePainter extends CustomPainter {
  _AttitudePainter({
    required this.rollDegrees,
    required this.pitchDegrees,
    required this.skyColor,
    required this.groundColor,
    required this.horizonColor,
    required this.ladderColor,
    required this.borderColor,
    required this.hairlineColor,
    required this.dialColor,
    required this.aircraftColor,
    required this.labelStyle,
  });

  final double? rollDegrees;
  final double? pitchDegrees;
  final Color skyColor;
  final Color groundColor;
  final Color horizonColor;
  final Color ladderColor;
  final Color borderColor;
  final Color hairlineColor;
  final Color dialColor;
  final Color aircraftColor;
  final TextStyle labelStyle;

  /// 每 1° 俯仰对应多少像素。值越大姿态变化看起来越明显。
  static const double _pixelsPerDegree = 2.6;

  /// 俯仰刻度间隔与范围。
  static const double _ladderStepDegrees = 10;
  static const double _ladderMaxDegrees = 30;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2 - 5;

    canvas.save();
    canvas.clipPath(
      ui.Path()..addOval(Rect.fromCircle(center: center, radius: radius)),
    );
    _paintHorizon(canvas, center, radius);
    canvas.restore();

    // 表盘外圈 + 内圈，和罗盘保持同一套"表壳"。
    canvas.drawCircle(
      center,
      radius,
      Paint()
        ..color = borderColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1,
    );
    canvas.drawCircle(
      center,
      radius - 4,
      Paint()
        ..color = hairlineColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1,
    );

    _paintAircraftSymbol(canvas, center, radius);
    canvas.drawCircle(center, 2, Paint()..color = aircraftColor);
  }

  /// 天地、天地线、俯仰刻度。
  void _paintHorizon(Canvas canvas, Offset center, double radius) {
    final roll = rollDegrees ?? 0;
    final pitch = pitchDegrees ?? 0;
    final extent = radius * 2.2;

    canvas.save();
    // 机头抬起 → 地平线相对表盘下移。
    canvas.translate(center.dx, center.dy + pitch * _pixelsPerDegree);
    canvas.rotate(-roll * math.pi / 180);

    canvas.drawRect(
      Rect.fromLTRB(-extent, -extent, extent, 0),
      Paint()..color = skyColor,
    );
    canvas.drawRect(
      Rect.fromLTRB(-extent, 0, extent, extent),
      Paint()..color = groundColor,
    );
    canvas.drawLine(
      Offset(-extent, 0),
      Offset(extent, 0),
      Paint()
        ..color = horizonColor
        ..strokeWidth = 1.6,
    );

    for (
      var deg = -_ladderMaxDegrees;
      deg <= _ladderMaxDegrees;
      deg += _ladderStepDegrees
    ) {
      if (deg == 0) continue;
      final y = -deg * _pixelsPerDegree;
      final halfWidth = deg.abs() % 20 == 0 ? radius * 0.42 : radius * 0.26;
      canvas.drawLine(
        Offset(-halfWidth, y),
        Offset(halfWidth, y),
        Paint()
          ..color = ladderColor
          ..strokeWidth = 1.2,
      );
      final text = TextPainter(
        text: TextSpan(text: deg.abs().toInt().toString(), style: labelStyle),
        textDirection: TextDirection.ltr,
      )..layout();
      text.paint(canvas, Offset(halfWidth + 5, y - text.height / 2));
    }
    canvas.restore();
  }

  /// 固定在中央的机翼符号：左右各一段横线，中间留口。
  void _paintAircraftSymbol(Canvas canvas, Offset center, double radius) {
    final paint = Paint()
      ..color = aircraftColor
      ..strokeWidth = 2.6
      ..strokeCap = StrokeCap.round;
    final gap = radius * 0.16;
    final length = radius * 0.40;
    canvas.drawLine(
      Offset(center.dx - gap - length, center.dy),
      Offset(center.dx - gap, center.dy),
      paint,
    );
    canvas.drawLine(
      Offset(center.dx + gap, center.dy),
      Offset(center.dx + gap + length, center.dy),
      paint,
    );
  }

  @override
  bool shouldRepaint(_AttitudePainter old) =>
      old.rollDegrees != rollDegrees ||
      old.pitchDegrees != pitchDegrees ||
      old.skyColor != skyColor ||
      old.groundColor != groundColor;
}
