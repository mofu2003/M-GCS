import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 右下角的复合圆形仪表：外圈航向卡 + 中央姿态球。
///
/// 外圈只保留密集刻度、30° 数字和固定航向箭头，方位字去掉，避免和姿态球
/// 争抢视觉层级。中央姿态球放大，显示天地、地平线、俯仰刻度和固定机翼
/// 基准。材质和颜色全部复用当前设计 token。
class MgMacGlassAttitudeCompass extends StatelessWidget {
  const MgMacGlassAttitudeCompass({
    super.key,
    required this.headingDegrees,
    required this.rollDegrees,
    required this.pitchDegrees,
  });

  final double headingDegrees;
  final double rollDegrees;
  final double pitchDegrees;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final heading = _normalizeHeading(headingDegrees);
    final roll = _finiteOrZero(rollDegrees);
    final pitch = _finiteOrZero(pitchDegrees);
    final headingText = heading.round().toString().padLeft(3, '0');
    final cardinal = _cardinalLabel(heading);

    return RepaintBoundary(
      child: SizedBox.square(
        dimension: MgMetrics.attitudeCompassSize,
        child: DecoratedBox(
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: colors.attitudeCompassGlass,
            border: Border.all(color: colors.attitudeCompassGlassBorder),
            boxShadow: [
              BoxShadow(
                color: colors.shadow.withValues(alpha: 0.12),
                blurRadius: 12,
                offset: const Offset(0, 4),
              ),
            ],
          ),
          child: ClipOval(
            child: BackdropFilter(
              filter: ui.ImageFilter.blur(
                sigmaX: MgMetrics.attitudeCompassBlurSigma,
                sigmaY: MgMetrics.attitudeCompassBlurSigma,
              ),
              child: Center(
                child: SizedBox.square(
                  dimension: MgMetrics.attitudeCompassContentSize,
                  child: Stack(
                    fit: StackFit.expand,
                    clipBehavior: Clip.none,
                    children: [
                      RepaintBoundary(
                        child: CustomPaint(
                          painter: _CompassDialPainter(
                            headingDegrees: heading,
                            tickColor: colors.instrumentTick,
                            majorTickColor: colors.instrumentMajorTick,
                            cardinalTickColor: colors.instrumentInk,
                            labelColor: colors.instrumentLabel,
                            numberStyle: MgTypography.sans(
                              MgTypography.caption.copyWith(
                                height: 1,
                                color: colors.instrumentLabel,
                              ),
                            ),
                          ),
                        ),
                      ),
                      RepaintBoundary(
                        child: CustomPaint(
                          painter: _AttitudeBallPainter(
                            rollDegrees: roll,
                            pitchDegrees: pitch,
                            skyColor: colors.attitudeSky,
                            groundColor: colors.attitudeGround,
                            horizonColor: colors.attitudeHorizon,
                            ladderColor: colors.attitudeLadder,
                            borderColor: colors.attitudeBallBorder,
                            aircraftColor: colors.attitudeAircraft,
                          ),
                        ),
                      ),
                      RepaintBoundary(
                        child: CustomPaint(
                          painter: _HeadingPointerPainter(
                            color: colors.instrumentInk,
                          ),
                        ),
                      ),
                      Align(
                        alignment: const Alignment(0, 0.52),
                        child: Container(
                          width: MgMetrics.attitudeHeadingReadoutWidth,
                          height: MgMetrics.attitudeHeadingReadoutHeight,
                          alignment: Alignment.center,
                          decoration: BoxDecoration(
                            color: colors.attitudeCompassGlass,
                            border: Border.all(
                              color: colors.attitudeCompassGlassBorder,
                            ),
                            borderRadius: BorderRadius.circular(
                              MgMetrics.attitudeHeadingReadoutRadius,
                            ),
                          ),
                          child: Text(
                            '$cardinal$headingText°',
                            maxLines: 1,
                            style: MgTypography.sans(
                              MgTypography.instrumentHeadingReadout.copyWith(
                                color: colors.instrumentInk,
                              ),
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _CompassDialPainter extends CustomPainter {
  const _CompassDialPainter({
    required this.headingDegrees,
    required this.tickColor,
    required this.majorTickColor,
    required this.cardinalTickColor,
    required this.labelColor,
    required this.numberStyle,
  });

  final double headingDegrees;
  final Color tickColor;
  final Color majorTickColor;
  final Color cardinalTickColor;
  final Color labelColor;
  final TextStyle numberStyle;

  static const double _minorStepDegrees = 5;
  static const double _majorStepDegrees = 30;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;
    final rotation = -headingDegrees * math.pi / 180;

    canvas.save();
    canvas.translate(center.dx, center.dy);
    canvas.rotate(rotation);
    canvas.translate(-center.dx, -center.dy);

    _paintTicks(canvas, center, radius);
    _paintNumbers(canvas, center, radius);

    canvas.restore();
  }

  void _paintTicks(Canvas canvas, Offset center, double radius) {
    final minorPaint = Paint()
      ..color = tickColor
      ..strokeWidth = 0.9
      ..strokeCap = StrokeCap.round;
    final majorPaint = Paint()
      ..color = majorTickColor
      ..strokeWidth = 1.6
      ..strokeCap = StrokeCap.round;
    final cardinalPaint = Paint()
      ..color = cardinalTickColor
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round;
    final outer = radius - 7;

    for (var deg = 0.0; deg < 360; deg += _minorStepDegrees) {
      final isMajor = deg % _majorStepDegrees == 0;
      final isCardinal = deg % 90 == 0;
      final inner = outer - (isMajor ? 9 : 5);
      final angle = (deg - 90) * math.pi / 180;
      canvas.drawLine(
        center + Offset(math.cos(angle) * inner, math.sin(angle) * inner),
        center + Offset(math.cos(angle) * outer, math.sin(angle) * outer),
        isCardinal ? cardinalPaint : (isMajor ? majorPaint : minorPaint),
      );
    }
  }

  void _paintNumbers(Canvas canvas, Offset center, double radius) {
    for (var bearing = 0; bearing < 360; bearing += _majorStepDegrees.toInt()) {
      final angle = (bearing - 90) * math.pi / 180;
      final position =
          center + Offset(math.cos(angle), math.sin(angle)) * (radius - 24);
      final painter = _CompassLabelTextPainterCache.get(
        bearing.toString(),
        numberStyle,
      );
      canvas.save();
      canvas.translate(position.dx, position.dy);
      canvas.rotate(bearing * math.pi / 180);
      painter.paint(canvas, Offset(-painter.width / 2, -painter.height / 2));
      canvas.restore();
    }
  }

  @override
  bool shouldRepaint(_CompassDialPainter old) =>
      old.headingDegrees != headingDegrees ||
      old.tickColor != tickColor ||
      old.majorTickColor != majorTickColor ||
      old.cardinalTickColor != cardinalTickColor ||
      old.labelColor != labelColor ||
      old.numberStyle != numberStyle;
}

class _AttitudeBallPainter extends CustomPainter {
  const _AttitudeBallPainter({
    required this.rollDegrees,
    required this.pitchDegrees,
    required this.skyColor,
    required this.groundColor,
    required this.horizonColor,
    required this.ladderColor,
    required this.borderColor,
    required this.aircraftColor,
  });

  final double rollDegrees;
  final double pitchDegrees;
  final Color skyColor;
  final Color groundColor;
  final Color horizonColor;
  final Color ladderColor;
  final Color borderColor;
  final Color aircraftColor;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final ballRadius =
        size.shortestSide / 2 - MgMetrics.attitudeCompassBallInset;
    final ballPath = ui.Path()
      ..addOval(Rect.fromCircle(center: center, radius: ballRadius));
    final pitchOffset =
        pitchDegrees.clamp(-20.0, 20.0) / 20 * ballRadius * 0.28;
    final extent = ballRadius * 2.4;

    canvas.save();
    canvas.clipPath(ballPath);
    canvas.translate(center.dx, center.dy + pitchOffset);
    canvas.rotate(rollDegrees * math.pi / 180);

    canvas.drawRect(
      Rect.fromLTRB(-extent, -extent, extent, 0),
      Paint()..color = skyColor,
    );
    canvas.drawRect(
      Rect.fromLTRB(-extent, 0, extent, extent),
      Paint()..color = groundColor,
    );
    canvas.drawLine(
      const Offset(-1000, 0),
      const Offset(1000, 0),
      Paint()
        ..color = horizonColor
        ..strokeWidth = 1.8,
    );

    final ladderPaint = Paint()
      ..color = ladderColor
      ..strokeWidth = 1;
    for (final degree in const [-20.0, -10.0, 10.0, 20.0]) {
      final y = degree * ballRadius * 0.014;
      final halfWidth = degree.abs() >= 20
          ? ballRadius * 0.38
          : ballRadius * 0.25;
      canvas.drawLine(Offset(-halfWidth, y), Offset(halfWidth, y), ladderPaint);
    }
    canvas.restore();

    canvas.drawCircle(
      center,
      ballRadius,
      Paint()
        ..color = borderColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.2,
    );

    final aircraftPaint = Paint()
      ..color = aircraftColor
      ..strokeWidth = 2.2
      ..strokeCap = StrokeCap.round;
    final gap = ballRadius * 0.16;
    final length = ballRadius * 0.38;
    canvas.drawLine(
      Offset(center.dx - gap - length, center.dy),
      Offset(center.dx - gap, center.dy),
      aircraftPaint,
    );
    canvas.drawLine(
      Offset(center.dx + gap, center.dy),
      Offset(center.dx + gap + length, center.dy),
      aircraftPaint,
    );
    canvas.drawCircle(center, 2.1, Paint()..color = aircraftColor);
  }

  @override
  bool shouldRepaint(_AttitudeBallPainter old) =>
      old.rollDegrees != rollDegrees ||
      old.pitchDegrees != pitchDegrees ||
      old.skyColor != skyColor ||
      old.groundColor != groundColor ||
      old.horizonColor != horizonColor ||
      old.ladderColor != ladderColor ||
      old.borderColor != borderColor ||
      old.aircraftColor != aircraftColor;
}

class _HeadingPointerPainter extends CustomPainter {
  const _HeadingPointerPainter({required this.color});

  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;
    final top = center.dy - radius + 8;
    final path = ui.Path()
      ..moveTo(center.dx, top + 10)
      ..lineTo(center.dx - 6, top)
      ..lineTo(center.dx + 6, top)
      ..close();
    canvas.drawPath(path, Paint()..color = color);
  }

  @override
  bool shouldRepaint(_HeadingPointerPainter old) => old.color != color;
}

final class _CompassLabelTextPainterCache {
  const _CompassLabelTextPainterCache._();

  static final Map<(String, TextStyle), TextPainter> _painters = {};

  static TextPainter get(String text, TextStyle style) {
    return _painters.putIfAbsent((text, style), () {
      return TextPainter(
        text: TextSpan(text: text, style: style),
        textAlign: TextAlign.center,
        textDirection: TextDirection.ltr,
        maxLines: 1,
      )..layout();
    });
  }
}

double _finiteOrZero(double value) => value.isFinite ? value : 0;

double _normalizeHeading(double value) {
  if (!value.isFinite) return 0;
  final normalized = value % 360;
  return normalized < 0 ? normalized + 360 : normalized;
}

String _cardinalLabel(double heading) {
  if (heading >= 315 || heading < 45) return '北';
  if (heading < 135) return '东';
  if (heading < 225) return '南';
  return '西';
}
