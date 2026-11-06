import 'dart:math' as math;
import 'package:flutter/material.dart';

import 'package:mgcs_frontend/theme/mg_colors.dart';

/// 飞控加速度计校准步骤对应的摆放方向。
enum AccelOrientation {
  level('水平放置'),
  left('左侧朝下'),
  right('右侧朝下'),
  noseDown('机头朝下'),
  noseUp('机头朝上'),
  inverted('倒放');

  const AccelOrientation(this.label);

  final String label;

  static AccelOrientation fromStep(int step) => switch (step) {
    2 => left,
    3 => right,
    4 => noseDown,
    5 => noseUp,
    6 => inverted,
    _ => level,
  };
}

/// 加速度计校准步骤的摆放方向提示。
class CalibrationAircraft extends StatelessWidget {
  const CalibrationAircraft({super.key, required this.orientation});

  final AccelOrientation? orientation;

  @override
  Widget build(BuildContext context) {
    // 统一走色板：边框和文字取强调色，底色取其淡化的同色。
    //
    // 不用 MacosTheme.primaryColor：它跟随窗口焦点变化，窗口不是主窗口时
    // 会退化成灰色，图框一失焦就变色。
    final colors = context.mgColors;
    final pose = orientation ?? AccelOrientation.level;
    return Semantics(
      label: '飞机摆放示意：${pose.label}',
      child: Container(
        width: 142,
        height: 128,
        decoration: BoxDecoration(
          color: colors.accentSurface,
          border: Border.all(color: colors.hairline),
          borderRadius: BorderRadius.circular(14),
        ),
        child: AnimatedSwitcher(
          duration: MediaQuery.disableAnimationsOf(context)
              ? Duration.zero
              : const Duration(milliseconds: 180),
          child: CustomPaint(
            key: ValueKey(pose),
            size: const Size(142, 128),
            painter: _AircraftPosePainter(
              pose,
              colors.accent,
              colors.warning,
              colors.textTertiary,
            ),
          ),
        ),
      ),
    );
  }
}

class _AircraftPosePainter extends CustomPainter {
  const _AircraftPosePainter(
    this.pose,
    this.color,
    this.noseColor,
    this.groundColor,
  );
  final AccelOrientation pose;
  final Color color;
  final Color noseColor;
  final Color groundColor;

  Offset project(double x, double y, double z) {
    final roll = switch (pose) {
      AccelOrientation.left => -math.pi / 2,
      AccelOrientation.right => math.pi / 2,
      AccelOrientation.inverted => math.pi,
      _ => 0.0,
    };
    final pitch = switch (pose) {
      AccelOrientation.noseDown => -math.pi / 2,
      AccelOrientation.noseUp => math.pi / 2,
      _ => 0.0,
    };
    final rx = x * math.cos(roll) + z * math.sin(roll);
    final rz = -x * math.sin(roll) + z * math.cos(roll);
    final ry = y * math.cos(pitch) - rz * math.sin(pitch);
    final height = y * math.sin(pitch) + rz * math.cos(pitch);
    return Offset(
      71 + rx * 0.9 + ry * 0.38,
      62 + rx * 0.16 - ry * 0.4 - height * 0.85,
    );
  }

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 142, size.height / 128);
    canvas.drawLine(
      const Offset(18, 110),
      const Offset(124, 110),
      Paint()
        ..color = groundColor.withValues(alpha: 0.35)
        ..strokeWidth = 1.5,
    );
    const outline = <(double, double)>[
      (0, 43),
      (7, 12),
      (36, -5),
      (36, -14),
      (7, -5),
      (6, -28),
      (20, -35),
      (20, -41),
      (0, -35),
      (-20, -41),
      (-20, -35),
      (-6, -28),
      (-7, -5),
      (-36, -14),
      (-36, -5),
      (-7, 12),
    ];
    Path shape(double z) {
      final path = Path();
      for (var i = 0; i < outline.length; i++) {
        final point = project(outline[i].$1, outline[i].$2, z);
        if (i == 0) {
          path.moveTo(point.dx, point.dy);
        } else {
          path.lineTo(point.dx, point.dy);
        }
      }
      return path..close();
    }

    canvas.drawPath(shape(-4), Paint()..color = color.withValues(alpha: 0.25));
    canvas.drawPath(shape(4), Paint()..color = color.withValues(alpha: 0.65));
    canvas.drawPath(
      shape(4),
      Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5
        ..strokeJoin = StrokeJoin.round,
    );
    canvas.drawLine(
      project(0, -30, 4),
      project(0, -30, 17),
      Paint()
        ..color = color
        ..strokeWidth = 3
        ..strokeCap = StrokeCap.round,
    );
    final nose = project(0, 34, 5);
    canvas.drawCircle(nose, 3.5, Paint()..color = noseColor);
    canvas.restore();
  }

  @override
  bool shouldRepaint(_AircraftPosePainter old) =>
      old.pose != pose ||
      old.color != color ||
      old.noseColor != noseColor ||
      old.groundColor != groundColor;
}
