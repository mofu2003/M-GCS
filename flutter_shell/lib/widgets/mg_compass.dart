import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 航向罗盘。
///
/// 表盘像真实航向卡一样相对机头反向旋转，顶部固定箭头始终表示飞机当前航向。
/// 中央内嵌姿态球，机头基准、横滚和俯仰都在同一仪表内表达。
///
/// 数据来自 `GLOBAL_POSITION_INT.hdg` / `VFR_HUD.heading`，不另算一套航向。
/// 视觉上参考航空罗盘的“大盘面 + 密集刻度 + 红色北标”，外圈材质使用
/// [MgColors.floatingSurface]，跟随 macOS 明暗模式与地图浮层体系。
class MgCompass extends StatelessWidget {
  const MgCompass({
    super.key,
    required this.headingDegrees,
    this.rollDegrees,
    this.pitchDegrees,
  });

  /// 航向角（度，0 = 正北，顺时针为正）；无效时只画表盘。
  final double? headingDegrees;

  /// 横滚角与俯仰角（度）；有效时在罗盘中央绘制姿态球。
  final double? rollDegrees;
  final double? pitchDegrees;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final heading = _normalizeHeading(headingDegrees);
    final roundedHeading = heading == null ? null : heading.round() % 360;
    final headingText = roundedHeading == null
        ? '航向 --'
        : '${_directionLabel(heading!)}$roundedHeading°';

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          headingText,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MgTypography.sans(
            MgTypography.compassHeading.copyWith(color: colors.textPrimary),
          ),
        ),
        const SizedBox(height: MgMetrics.compassHeadingGap),
        SizedBox(
          width: MgMetrics.compassSize,
          height: MgMetrics.compassSize + MgMetrics.compassArrowArea,
          child: CustomPaint(
            painter: _CompassPainter(
              headingDegrees: heading,
              rollDegrees: rollDegrees,
              pitchDegrees: pitchDegrees,
              dialColor: colors.instrumentGlass,
              shadowColor: colors.shadow,
              borderColor: colors.instrumentGlassBorder,
              hairlineColor: colors.attitudeBallBorder,
              tickColor: colors.instrumentTick,
              labelColor: colors.instrumentMajorTick,
              northColor: colors.compassNorth,
              attitudeSky: colors.attitudeSky,
              attitudeGround: colors.attitudeGround,
              horizonColor: colors.attitudeHorizon,
              ladderColor: colors.attitudeLadder,
              aircraftColor: colors.attitudeAircraft,
              directionColor: colors.instrumentInk,
              numberStyle: MgTypography.sans(
                MgTypography.caption.copyWith(
                  height: 1,
                  color: colors.instrumentLabel,
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  static double? _normalizeHeading(double? heading) {
    if (heading == null || !heading.isFinite) return null;
    return ((heading % 360) + 360) % 360;
  }

  static String _directionLabel(double heading) {
    const labels = ['北', '东北', '东', '东南', '南', '西南', '西', '西北'];
    final index = ((heading + 22.5) ~/ 45) % labels.length;
    return labels[index];
  }
}

class _CompassPainter extends CustomPainter {
  _CompassPainter({
    required this.headingDegrees,
    required this.rollDegrees,
    required this.pitchDegrees,
    required this.dialColor,
    required this.shadowColor,
    required this.borderColor,
    required this.hairlineColor,
    required this.tickColor,
    required this.labelColor,
    required this.northColor,
    required this.attitudeSky,
    required this.attitudeGround,
    required this.horizonColor,
    required this.ladderColor,
    required this.aircraftColor,
    required this.directionColor,
    required this.numberStyle,
  });

  final double? headingDegrees;
  final double? rollDegrees;
  final double? pitchDegrees;
  final Color dialColor;
  final Color shadowColor;
  final Color borderColor;
  final Color hairlineColor;
  final Color tickColor;
  final Color labelColor;
  final Color northColor;
  final Color attitudeSky;
  final Color attitudeGround;
  final Color horizonColor;
  final Color ladderColor;
  final Color aircraftColor;
  final Color directionColor;
  final TextStyle numberStyle;

  /// 罗盘刻度：5° 一个小刻度，30° 中刻度，90° 长刻度。
  static const double _minorStepDegrees = 5;
  static const double _dialMargin = 4;

  @override
  void paint(Canvas canvas, Size size) {
    final radius = (size.width - _dialMargin * 2) / 2;
    final center = Offset(size.width / 2, size.height - radius - _dialMargin);
    final heading = headingDegrees ?? 0;

    final dialPath = ui.Path()
      ..addOval(Rect.fromCircle(center: center, radius: radius));
    canvas.drawShadow(dialPath, shadowColor, 8, false);
    canvas.drawPath(dialPath, Paint()..color = dialColor);
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

    _paintTicks(canvas, center, radius, heading);
    _paintNumbers(canvas, center, radius, heading);
    _paintNorthMarker(canvas, center, radius, heading);
    if (rollDegrees != null && pitchDegrees != null) {
      _paintAttitude(canvas, center, radius);
    } else {
      _paintCenterCross(canvas, center, radius);
    }
    _paintTopArrow(canvas, center, radius);
  }

  /// 把方位角换算成屏幕角度。航向本身永远落在顶部。
  double _screenAngle(double bearing, double heading) =>
      (bearing - heading - 90) * math.pi / 180;

  void _paintTicks(
    Canvas canvas,
    Offset center,
    double radius,
    double heading,
  ) {
    final minor = Paint()
      ..color = tickColor.withValues(alpha: 0.42)
      ..strokeWidth = 0.8
      ..strokeCap = StrokeCap.round;
    final ten = Paint()
      ..color = tickColor.withValues(alpha: 0.62)
      ..strokeWidth = 1
      ..strokeCap = StrokeCap.round;
    final medium = Paint()
      ..color = tickColor.withValues(alpha: 0.82)
      ..strokeWidth = 1.25
      ..strokeCap = StrokeCap.round;
    final major = Paint()
      ..color = tickColor
      ..strokeWidth = 1.8
      ..strokeCap = StrokeCap.round;

    final outer = radius - 6;
    for (var deg = 0.0; deg < 360; deg += _minorStepDegrees) {
      final isMajor = deg % 90 == 0;
      final isMedium = deg % 30 == 0;
      final isTen = deg % 10 == 0;
      final inner = outer - (isMajor ? 12 : (isMedium ? 8 : (isTen ? 5 : 3)));
      final angle = _screenAngle(deg, heading);
      canvas.drawLine(
        center + Offset(math.cos(angle) * inner, math.sin(angle) * inner),
        center + Offset(math.cos(angle) * outer, math.sin(angle) * outer),
        isMajor ? major : (isMedium ? medium : (isTen ? ten : minor)),
      );
    }
  }

  void _paintNumbers(
    Canvas canvas,
    Offset center,
    double radius,
    double heading,
  ) {
    for (var deg = 0; deg < 360; deg += 30) {
      final angleFromTop = (deg - heading) * math.pi / 180;
      final angle = angleFromTop - math.pi / 2;
      final position =
          center + Offset(math.cos(angle), math.sin(angle)) * (radius - 30);
      final text = TextPainter(
        text: TextSpan(text: '$deg', style: numberStyle),
        textDirection: TextDirection.ltr,
      )..layout();
      canvas.save();
      canvas.translate(position.dx, position.dy);
      canvas.rotate(angleFromTop);
      text.paint(canvas, Offset(-text.width / 2, -text.height / 2));
      canvas.restore();
    }
  }

  void _paintNorthMarker(
    Canvas canvas,
    Offset center,
    double radius,
    double heading,
  ) {
    final angle = _screenAngle(0, heading);
    final direction = Offset(math.cos(angle), math.sin(angle));
    final tangent = Offset(-direction.dy, direction.dx);
    final tip = center + direction * (radius - 23);
    final base = center + direction * (radius - 12);
    final marker = ui.Path()
      ..moveTo(tip.dx, tip.dy)
      ..lineTo(base.dx + tangent.dx * 5, base.dy + tangent.dy * 5)
      ..lineTo(base.dx - tangent.dx * 5, base.dy - tangent.dy * 5)
      ..close();
    canvas.drawPath(marker, Paint()..color = northColor);
  }

  /// 中央姿态球：天地随横滚反向旋转、随俯仰上下平移。
  ///
  /// 外圈航向卡负责“机头朝哪”，内部姿态球负责“机体怎么倾斜”，两者共用同一
  /// 圆心和固定机头基准，避免再放两台互相抢空间的独立仪表。
  void _paintAttitude(Canvas canvas, Offset center, double radius) {
    final roll = rollDegrees!;
    final pitch = pitchDegrees!;
    final ballRadius = radius * 0.62;
    final ballPath = ui.Path()
      ..addOval(Rect.fromCircle(center: center, radius: ballRadius));
    final extent = ballRadius * 2.5;

    canvas.save();
    canvas.clipPath(ballPath);
    canvas.translate(center.dx, center.dy + pitch * (ballRadius * 0.035));
    canvas.rotate(-roll * math.pi / 180);
    canvas.drawRect(
      Rect.fromLTRB(-extent, -extent, extent, 0),
      Paint()..color = attitudeSky,
    );
    canvas.drawRect(
      Rect.fromLTRB(-extent, 0, extent, extent),
      Paint()..color = attitudeGround,
    );
    canvas.drawLine(
      Offset(-extent, 0),
      Offset(extent, 0),
      Paint()
        ..color = horizonColor
        ..strokeWidth = 1.6,
    );

    final ladderPaint = Paint()
      ..color = ladderColor
      ..strokeWidth = 1;
    for (var deg = -20; deg <= 20; deg += 10) {
      if (deg == 0) continue;
      final y = -deg * (ballRadius * 0.035);
      final halfWidth = deg.abs() % 20 == 0
          ? ballRadius * 0.42
          : ballRadius * 0.27;
      canvas.drawLine(Offset(-halfWidth, y), Offset(halfWidth, y), ladderPaint);
    }
    canvas.restore();

    canvas.drawCircle(
      center,
      ballRadius,
      Paint()
        ..color = borderColor.withValues(alpha: 0.78)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.2,
    );

    final aircraftPaint = Paint()
      ..color = aircraftColor
      ..strokeWidth = 2.4
      ..strokeCap = StrokeCap.round;
    final gap = ballRadius * 0.18;
    final length = ballRadius * 0.42;
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
    canvas.drawCircle(center, 2.2, Paint()..color = aircraftColor);
  }

  void _paintCenterCross(Canvas canvas, Offset center, double radius) {
    final paint = Paint()
      ..color = borderColor.withValues(alpha: 0.72)
      ..strokeWidth = 1.3
      ..strokeCap = StrokeCap.round;
    final extent = radius * 0.25;
    final diagonalX = extent * math.cos(math.pi / 4);
    final diagonalY = extent * math.sin(math.pi / 4);
    canvas.drawLine(
      center + Offset(-diagonalX, diagonalY),
      center + Offset(diagonalX, -diagonalY),
      paint,
    );
    canvas.drawLine(
      center + Offset(-diagonalX, -diagonalY),
      center + Offset(diagonalX, diagonalY),
      paint,
    );
    canvas.drawCircle(
      center,
      1.8,
      Paint()..color = borderColor.withValues(alpha: 0.82),
    );
  }

  /// 顶部固定箭头：它是机头基准，不随航向卡旋转。
  void _paintTopArrow(Canvas canvas, Offset center, double radius) {
    final top = center.dy - radius - 13;
    final shaftBottom = center.dy - radius - 2;
    final shaftPaint = Paint()
      ..color = directionColor
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(
      Offset(center.dx, top + 7),
      Offset(center.dx, shaftBottom),
      shaftPaint,
    );

    final head = ui.Path()
      ..moveTo(center.dx, top)
      ..lineTo(center.dx - 6, top + 8)
      ..lineTo(center.dx + 6, top + 8)
      ..close();
    canvas.drawPath(head, Paint()..color = directionColor);
  }

  @override
  bool shouldRepaint(_CompassPainter old) =>
      old.headingDegrees != headingDegrees ||
      old.dialColor != dialColor ||
      old.shadowColor != shadowColor ||
      old.borderColor != borderColor ||
      old.hairlineColor != hairlineColor ||
      old.tickColor != tickColor ||
      old.labelColor != labelColor ||
      old.rollDegrees != rollDegrees ||
      old.pitchDegrees != pitchDegrees ||
      old.northColor != northColor ||
      old.attitudeSky != attitudeSky ||
      old.attitudeGround != attitudeGround ||
      old.horizonColor != horizonColor ||
      old.ladderColor != ladderColor ||
      old.aircraftColor != aircraftColor ||
      old.directionColor != directionColor ||
      old.numberStyle != numberStyle;
}
