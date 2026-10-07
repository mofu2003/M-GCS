import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 航向罗盘。
///
/// 表盘固定（北在上），指针指向飞机当前航向——和地图上飞机标记同源
/// （`GLOBAL_POSITION_INT.hdg` / `VFR_HUD.heading`），不另算一套数据。
///
/// 设计取向：只保留"表盘 + 刻度 + 方位字 + 指针 + 顶部基准"五样东西，
/// 不做装饰性弧线、渐变或光效；指针做成标准的菱形指针（前半段强调色、
/// 后半段浅色带描边），比纯色三角形更容易辨认朝向。所有颜色、字号、尺寸
/// 都取 [MgColors] / [MgTypography] / [MgMetrics] 的 token。
class MgCompass extends StatelessWidget {
  const MgCompass({super.key, required this.headingDegrees});

  /// 航向角（度，0 = 正北，顺时针为正）；无效时只画表盘。
  final double? headingDegrees;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return SizedBox(
      width: MgMetrics.compassSize,
      height: MgMetrics.compassSize,
      child: CustomPaint(
        painter: _CompassPainter(
          headingDegrees: headingDegrees,
          dialColor: colors.floatingSurface,
          borderColor: colors.divider,
          hairlineColor: colors.hairline,
          tickColor: colors.textSecondary,
          needleColor: colors.accent,
          labelStyle: MgTypography.sans(
            MgTypography.caption.copyWith(height: 1, color: colors.textPrimary),
          ),
        ),
      ),
    );
  }
}

class _CompassPainter extends CustomPainter {
  _CompassPainter({
    required this.headingDegrees,
    required this.dialColor,
    required this.borderColor,
    required this.hairlineColor,
    required this.tickColor,
    required this.needleColor,
    required this.labelStyle,
  });

  final double? headingDegrees;
  final Color dialColor;
  final Color borderColor;
  final Color hairlineColor;
  final Color tickColor;
  final Color needleColor;
  final TextStyle labelStyle;

  /// 小刻度间隔；每三个小刻度一条中刻度，象限端点（方位字处）最长。
  static const double _minorStepDegrees = 15;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2 - 5;

    // 表盘：半透明底 + 描边，再描一圈内圈，让表盘有厚度感。
    canvas.drawCircle(center, radius, Paint()..color = dialColor);
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

    _paintTicks(canvas, center, radius);
    _paintCardinals(canvas, center, radius);
    _paintArrow(canvas, center, radius);
    _paintTopIndex(canvas, center, radius);
  }

  /// 刻度：10° 短、30° 中、90° 长，从内圈往外画。
  void _paintTicks(Canvas canvas, Offset center, double radius) {
    final minor = Paint()
      ..color = tickColor.withValues(alpha: 0.45)
      ..strokeWidth = 1
      ..strokeCap = StrokeCap.round;
    final medium = Paint()
      ..color = tickColor.withValues(alpha: 0.75)
      ..strokeWidth = 1.2
      ..strokeCap = StrokeCap.round;
    final major = Paint()
      ..color = tickColor
      ..strokeWidth = 1.8
      ..strokeCap = StrokeCap.round;

    final outer = radius - 7;
    for (var deg = 0.0; deg < 360; deg += _minorStepDegrees) {
      final angle = (deg - 90) * math.pi / 180;
      final isMajor = deg % 90 == 0;
      final isMedium = deg % 45 == 0;
      final inner = outer - (isMajor ? 12 : (isMedium ? 8 : 4.5));
      canvas.drawLine(
        center + Offset(math.cos(angle) * inner, math.sin(angle) * inner),
        center + Offset(math.cos(angle) * outer, math.sin(angle) * outer),
        isMajor ? major : (isMedium ? medium : minor),
      );
    }
  }

  /// 方位字：北/东/南/西，压在刻度内侧。
  void _paintCardinals(Canvas canvas, Offset center, double radius) {
    const cardinals = {0: '北', 90: '东', 180: '南', 270: '西'};
    cardinals.forEach((deg, label) {
      final angle = (deg - 90) * math.pi / 180;
      final text = TextPainter(
        text: TextSpan(text: label, style: labelStyle),
        textDirection: TextDirection.ltr,
      )..layout();
      final position =
          center +
          Offset(math.cos(angle), math.sin(angle)) * (radius - 26) -
          Offset(text.width / 2, text.height / 2);
      text.paint(canvas, position);
    });
  }

  /// 航向箭头：一根短杆 + 实心箭头，从中心指向当前航向。
  ///
  /// 刻意不用"菱形指针"那种双向造型——这里只需要表达"机头朝哪"，
  /// 单头箭头的信息量刚好，也不会和姿态仪的机翼符号混淆。
  void _paintArrow(Canvas canvas, Offset center, double radius) {
    final heading = headingDegrees;
    if (heading == null || !heading.isFinite) return;

    final angle = (heading - 90) * math.pi / 180;
    final direction = Offset(math.cos(angle), math.sin(angle));
    final length = radius - 24;
    final tip = center + direction * length;
    final shaftEnd = center + direction * (length - 13);

    canvas.drawLine(
      center + direction * 7,
      shaftEnd,
      Paint()
        ..color = needleColor
        ..strokeWidth = 4
        ..strokeCap = StrokeCap.round,
    );

    final perpendicular = Offset(-direction.dy, direction.dx);
    final head = ui.Path()
      ..moveTo(tip.dx, tip.dy)
      ..lineTo(
        shaftEnd.dx + perpendicular.dx * 9,
        shaftEnd.dy + perpendicular.dy * 9,
      )
      ..lineTo(
        shaftEnd.dx - perpendicular.dx * 9,
        shaftEnd.dy - perpendicular.dy * 9,
      )
      ..close();
    canvas.drawShadow(head, const Color(0x66000000), 2, false);
    canvas.drawPath(head, Paint()..color = needleColor);

    // 中心轴点，箭头根部有个落点。
    canvas.drawCircle(center, 3.5, Paint()..color = needleColor);
  }

  /// 顶部基准标线：表盘外侧的小三角，读数以它为准。
  void _paintTopIndex(Canvas canvas, Offset center, double radius) {
    final path = ui.Path()
      ..moveTo(center.dx, center.dy - radius)
      ..lineTo(center.dx - 5, center.dy - radius - 8)
      ..lineTo(center.dx + 5, center.dy - radius - 8)
      ..close();
    canvas.drawPath(path, Paint()..color = needleColor);
  }

  @override
  bool shouldRepaint(_CompassPainter old) =>
      old.headingDegrees != headingDegrees ||
      old.needleColor != needleColor ||
      old.dialColor != dialColor;
}
