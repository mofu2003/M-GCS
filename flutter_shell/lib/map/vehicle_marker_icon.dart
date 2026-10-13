import 'dart:math' as math;

import 'package:flutter/material.dart';

/// 本机在地图上的机型剪影。
///
/// 只区分两类：多旋翼（交叉机臂 + 四个桨盘）与固定翼（机身 + 后掠翼 + 尾翼）。
/// VTOL 按固定翼画——它就是带升力桨的飞机；地面车辆等其它机型也回退到固定翼。
enum VehicleMarkerKind { multirotor, fixedWing }

/// 后端提供归一化 vehicle_class；Flutter 只选择对应的图形。
VehicleMarkerKind vehicleMarkerKindFor(String vehicleClass) {
  return vehicleClass == 'multirotor'
      ? VehicleMarkerKind.multirotor
      : VehicleMarkerKind.fixedWing;
}

/// 按机型剪影绘制本机标记，并按航向旋转（剪影默认朝正北）。
class VehicleMarkerIcon extends StatelessWidget {
  const VehicleMarkerIcon({
    super.key,
    required this.kind,
    required this.fill,
    required this.outline,
    this.headingDegrees,
  });

  final VehicleMarkerKind kind;

  /// 机体填充色，取主题强调色。
  final Color fill;

  /// 描边色，取面板底色：地图上任何背景下都能看出轮廓。
  final Color outline;

  final double? headingDegrees;

  @override
  Widget build(BuildContext context) {
    final painter = switch (kind) {
      VehicleMarkerKind.multirotor => _MultirotorPainter(fill, outline),
      VehicleMarkerKind.fixedWing => _FixedWingPainter(fill, outline),
    };
    final heading = headingDegrees;
    final body = CustomPaint(painter: painter, size: Size.infinite);
    if (heading == null || !heading.isFinite) return body;
    // 剪影朝上（正北），航向 0° 也是正北，所以直接按航向旋转。
    return Transform.rotate(angle: heading * math.pi / 180, child: body);
  }
}

/// 剪影统一按 100×100 的坐标系绘制，再按控件尺寸缩放；
/// 图形控制在中心 78×78 以内，旋转后不会超出方形控件被裁掉。
abstract class _VehiclePainter extends CustomPainter {
  const _VehiclePainter(this.fill, this.outline);

  final Color fill;
  final Color outline;

  Paint get _bodyPaint => Paint()
    ..color = fill
    ..style = PaintingStyle.fill;

  Paint get _edgePaint => Paint()
    ..color = outline
    ..style = PaintingStyle.stroke
    ..strokeWidth = 5
    ..strokeJoin = StrokeJoin.round
    ..strokeCap = StrokeCap.round;

  /// 先描边再填充：轮廓压在地图上，机体本身用强调色。
  void _finish(Canvas canvas, Path path) {
    canvas.drawPath(path, _edgePaint);
    canvas.drawPath(path, _bodyPaint);
  }

  void _circle(Canvas canvas, Offset center, double radius) {
    canvas.drawCircle(center, radius + 2, _edgePaint);
    canvas.drawCircle(center, radius, _bodyPaint);
    canvas.drawCircle(
      center,
      radius,
      Paint()
        ..color = outline
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );
  }

  @override
  bool shouldRepaint(covariant _VehiclePainter old) =>
      old.fill != fill || old.outline != outline;
}

class _MultirotorPainter extends _VehiclePainter {
  const _MultirotorPainter(super.fill, super.outline);

  @override
  void paint(Canvas canvas, Size size) {
    final scale = math.min(size.width, size.height) / 100.0;
    canvas.save();
    canvas.translate(
      (size.width - 100 * scale) / 2,
      (size.height - 100 * scale) / 2,
    );
    canvas.scale(scale);

    // 桨盘拉开、半径收小，机臂才露得出来；否则四个圆糊成一团看不出是四轴。
    const rotors = <Offset>[
      Offset(21, 24),
      Offset(79, 24),
      Offset(21, 76),
      Offset(79, 76),
    ];
    for (final rotor in rotors) {
      canvas.drawLine(
        const Offset(50, 50),
        rotor,
        Paint()
          ..color = outline
          ..strokeWidth = 8
          ..strokeCap = StrokeCap.round,
      );
    }
    for (final rotor in rotors) {
      canvas.drawLine(
        const Offset(50, 50),
        rotor,
        Paint()
          ..color = fill
          ..strokeWidth = 4
          ..strokeCap = StrokeCap.round,
      );
    }

    final body = Path()
      ..addRRect(
        RRect.fromRectAndRadius(
          Rect.fromCenter(center: const Offset(50, 50), width: 18, height: 24),
          const Radius.circular(6),
        ),
      );
    _finish(canvas, body);

    for (final rotor in rotors) {
      _circle(canvas, rotor, 11);
    }
    canvas.restore();
  }
}

class _FixedWingPainter extends _VehiclePainter {
  const _FixedWingPainter(super.fill, super.outline);

  @override
  void paint(Canvas canvas, Size size) {
    final scale = math.min(size.width, size.height) / 100.0;
    canvas.save();
    canvas.translate(
      (size.width - 100 * scale) / 2,
      (size.height - 100 * scale) / 2,
    );
    canvas.scale(scale);

    final plane = Path()
      // 机头
      ..moveTo(50, 8)
      ..lineTo(58, 30)
      // 右翼后缘（后掠）
      ..lineTo(92, 52)
      ..lineTo(92, 60)
      ..lineTo(58, 52)
      // 机身右侧
      ..lineTo(58, 78)
      // 右平尾
      ..lineTo(80, 86)
      ..lineTo(80, 92)
      ..lineTo(58, 90)
      ..lineTo(50, 96)
      // 左平尾
      ..lineTo(42, 90)
      ..lineTo(20, 92)
      ..lineTo(20, 86)
      ..lineTo(42, 78)
      // 机身左侧
      ..lineTo(42, 52)
      ..lineTo(8, 60)
      ..lineTo(8, 52)
      ..lineTo(42, 30)
      ..close();
    _finish(canvas, plane);
    canvas.restore();
  }
}
