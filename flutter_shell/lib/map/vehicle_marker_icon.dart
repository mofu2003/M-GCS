import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../connection/connection_controller.dart';

/// 本机在地图上的机型剪影。
///
/// 取代原先"圆形徽标 + 纸飞机图标"的画法：多旋翼画交叉机臂 + 四个桨盘，固定翼画
/// 机身 + 后掠翼 + 尾翼，VTOL 在固定翼剪影上叠四个桨盘。MP / QGC 的地图标记也是按
/// 机型画对应剪影，而不是所有机型共用一个箭头。
enum VehicleMarkerKind { multirotor, fixedWing, vtol, rover, unknown }

/// 由控制档位（固件/机型归类）与 MAV_TYPE 决定剪影种类。
///
/// MAV_TYPE 取值来自官方 MAVLink 定义：1 固定翼、2/3/13/14/15/16 多旋翼族、
/// 4 直升机、10/11 地面车辆、19–25 VTOL 族。
VehicleMarkerKind vehicleMarkerKindFor(
  VehicleControlProfile profile,
  int mavType,
) {
  switch (mavType) {
    case 1:
      return VehicleMarkerKind.fixedWing;
    case 2:
    case 3:
    case 4:
    case 13:
    case 14:
    case 15:
    case 16:
      return VehicleMarkerKind.multirotor;
    case 10:
    case 11:
      return VehicleMarkerKind.rover;
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
      return VehicleMarkerKind.vtol;
  }
  return switch (profile) {
    VehicleControlProfile.fixedWing => VehicleMarkerKind.fixedWing,
    VehicleControlProfile.planeVtol => VehicleMarkerKind.vtol,
    VehicleControlProfile.copter => VehicleMarkerKind.multirotor,
    VehicleControlProfile.unknown => VehicleMarkerKind.unknown,
  };
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
      VehicleMarkerKind.vtol => _VtolPainter(fill, outline),
      VehicleMarkerKind.rover => _RoverPainter(fill, outline),
      VehicleMarkerKind.unknown => _FixedWingPainter(fill, outline),
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
    // 机臂：中心到四个桨盘。
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

class _VtolPainter extends _VehiclePainter {
  const _VtolPainter(super.fill, super.outline);

  @override
  void paint(Canvas canvas, Size size) {
    _FixedWingPainter(fill, outline).paint(canvas, size);

    final scale = math.min(size.width, size.height) / 100.0;
    canvas.save();
    canvas.translate(
      (size.width - 100 * scale) / 2,
      (size.height - 100 * scale) / 2,
    );
    canvas.scale(scale);
    // 四个升力桨盘叠在翼面上，和 VTOL 的实际构型一致。
    for (final rotor in const <Offset>[
      Offset(30, 44),
      Offset(70, 44),
      Offset(30, 66),
      Offset(70, 66),
    ]) {
      _circle(canvas, rotor, 11);
    }
    canvas.restore();
  }
}

class _RoverPainter extends _VehiclePainter {
  const _RoverPainter(super.fill, super.outline);

  @override
  void paint(Canvas canvas, Size size) {
    final scale = math.min(size.width, size.height) / 100.0;
    canvas.save();
    canvas.translate(
      (size.width - 100 * scale) / 2,
      (size.height - 100 * scale) / 2,
    );
    canvas.scale(scale);
    final body = Path()
      ..addRRect(
        RRect.fromRectAndRadius(
          Rect.fromCenter(center: const Offset(50, 50), width: 46, height: 74),
          const Radius.circular(10),
        ),
      );
    _finish(canvas, body);
    for (final axle in const [Offset(20, 34), Offset(20, 66), Offset(80, 34), Offset(80, 66)]) {
      _circle(canvas, axle, 9);
    }
    canvas.restore();
  }
}
