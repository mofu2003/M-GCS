import 'dart:math' as math;
import 'package:flutter/material.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// Apple 风格姿态仪表盘：人工地平线 + 罗盘环。
///
/// 内圈：双色地平线（天空蓝 / 大地棕），中央固定橙色飞机剪影，地平线按 pitch/roll 转动。
/// 外圈：罗盘刻度环，按 heading 转动，N 标红色。
class MgAttitudeCompass extends StatelessWidget {
  const MgAttitudeCompass({
    super.key,
    required this.pitch,
    required this.roll,
    required this.heading,
    this.size = 200,
  });

  /// 俯仰角（度），范围 -90 ~ +90，正值机头向上。
  final double pitch;

  /// 横滚角（度），范围 -180 ~ +180，正值右倾。
  final double roll;

  /// 航向角（度），范围 0 ~ 360，正北为 0。
  final double heading;

  /// 仪表盘尺寸（直径）。
  final double size;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return SizedBox(
      width: size,
      height: size,
      child: CustomPaint(
        painter: _AttitudeCompassPainter(
          pitch: pitch,
          roll: roll,
          heading: heading,
          colors: colors,
        ),
      ),
    );
  }
}

class _AttitudeCompassPainter extends CustomPainter {
  _AttitudeCompassPainter({
    required this.pitch,
    required this.roll,
    required this.heading,
    required this.colors,
  });

  final double pitch;
  final double roll;
  final double heading;
  final MgColors colors;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;

    // 裁剪圆形区域
    canvas.save();
    canvas.clipPath(Path()..addOval(Rect.fromCircle(center: center, radius: radius)));

    // 1. 绘制地平线内圈（天空 + 大地）
    _drawHorizon(canvas, center, radius);

    canvas.restore();

    // 2. 绘制罗盘外环
    _drawCompassRing(canvas, center, radius);

    // 3. 绘制中央飞机剪影（固定不动）
    _drawAircraftSymbol(canvas, center, radius);

    // 4. 绘制外圈边框
    _drawBorder(canvas, center, radius);
  }

  /// 绘制人工地平线（天空 + 大地双色填充）
  void _drawHorizon(Canvas canvas, Offset center, double radius) {
    canvas.save();
    canvas.translate(center.dx, center.dy);
    canvas.rotate(roll * math.pi / 180);

    // 俯仰偏移：pitch 为正时地平线上移（天空扩大），反之下移
    // 1 度约等于仪表盘半径的 1%
    final pitchOffset = -pitch * radius * 0.01;

    // 天空渐变（顶部深蓝 → 地平线浅蓝）
    final skyRect = Rect.fromLTRB(-radius * 2, -radius * 2, radius * 2, pitchOffset);
    final skyGradient = LinearGradient(
      begin: Alignment.topCenter,
      end: Alignment.bottomCenter,
      colors: [
        const Color(0xFF0A84FF), // 深蓝
        const Color(0xFF64B5F6), // 浅蓝
      ],
    );
    canvas.drawRect(
      skyRect,
      Paint()..shader = skyGradient.createShader(skyRect),
    );

    // 大地渐变（地平线深棕 → 底部浅棕）
    final groundRect = Rect.fromLTRB(-radius * 2, pitchOffset, radius * 2, radius * 2);
    final groundGradient = LinearGradient(
      begin: Alignment.topCenter,
      end: Alignment.bottomCenter,
      colors: [
        const Color(0xFF8B4513), // 深棕
        const Color(0xFFA0826D), // 浅棕
      ],
    );
    canvas.drawRect(
      groundRect,
      Paint()..shader = groundGradient.createShader(groundRect),
    );

    // 地平线白线
    canvas.drawLine(
      Offset(-radius * 2, pitchOffset),
      Offset(radius * 2, pitchOffset),
      Paint()
        ..color = Colors.white
        ..strokeWidth = 2
        ..style = PaintingStyle.stroke,
    );

    canvas.restore();
  }

  /// 绘制罗盘外环（刻度 + 方位字母）
  void _drawCompassRing(Canvas canvas, Offset center, double radius) {
    canvas.save();
    canvas.translate(center.dx, center.dy);
    canvas.rotate(-heading * math.pi / 180); // 罗盘逆时针转动

    final ringRadius = radius * 0.88;
    final textRadius = radius * 0.75;

    // 绘制 36 个刻度（每 10 度一个）
    for (int i = 0; i < 36; i++) {
      final angle = i * 10 * math.pi / 180;
      final isMajor = i % 3 == 0; // 每 30 度一个主刻度

      final tickStart = Offset(
        math.sin(angle) * (ringRadius - (isMajor ? 12 : 8)),
        -math.cos(angle) * (ringRadius - (isMajor ? 12 : 8)),
      );
      final tickEnd = Offset(
        math.sin(angle) * ringRadius,
        -math.cos(angle) * ringRadius,
      );

      canvas.drawLine(
        tickStart,
        tickEnd,
        Paint()
          ..color = Colors.white.withValues(alpha: 0.9)
          ..strokeWidth = isMajor ? 2 : 1
          ..style = PaintingStyle.stroke,
      );
    }

    // 绘制方位字母 N E S W
    final directions = ['N', 'E', 'S', 'W'];
    for (int i = 0; i < 4; i++) {
      final angle = i * 90 * math.pi / 180;
      final x = math.sin(angle) * textRadius;
      final y = -math.cos(angle) * textRadius;

      canvas.save();
      canvas.translate(x, y);
      canvas.rotate(heading * math.pi / 180); // 字母保持正向

      final textPainter = TextPainter(
        text: TextSpan(
          text: directions[i],
          style: MgTypography.sans(MgTypography.bodyStrong.copyWith(
            color: i == 0 ? colors.danger : Colors.white, // N 标红
            fontSize: 14,
            height: 1.0,
          )),
        ),
        textDirection: TextDirection.ltr,
      )..layout();

      textPainter.paint(
        canvas,
        Offset(-textPainter.width / 2, -textPainter.height / 2),
      );

      canvas.restore();
    }

    canvas.restore();
  }

  /// 绘制中央飞机剪影（橙色，固定不动）
  void _drawAircraftSymbol(Canvas canvas, Offset center, double radius) {
    final symbolSize = radius * 0.25;
    final paint = Paint()
      ..color = const Color(0xFFFF9500)
      ..strokeWidth = 3
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    // 水平翼
    canvas.drawLine(
      Offset(center.dx - symbolSize, center.dy),
      Offset(center.dx + symbolSize, center.dy),
      paint,
    );

    // 中央机身圆点
    canvas.drawCircle(
      center,
      4,
      Paint()
        ..color = const Color(0xFFFF9500)
        ..style = PaintingStyle.fill,
    );
  }

  /// 绘制外圈边框
  void _drawBorder(Canvas canvas, Offset center, double radius) {
    canvas.drawCircle(
      center,
      radius,
      Paint()
        ..color = colors.divider
        ..strokeWidth = 2
        ..style = PaintingStyle.stroke,
    );
  }

  @override
  bool shouldRepaint(covariant _AttitudeCompassPainter oldDelegate) {
    return oldDelegate.pitch != pitch ||
        oldDelegate.roll != roll ||
        oldDelegate.heading != heading;
  }
}
