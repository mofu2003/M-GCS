import 'dart:math' as math;

import 'package:flutter/material.dart';

enum MgStatusIconKind { gps, battery }

/// Short, data-driven transitions settle between telemetry updates.
class MgStatusTelemetryIcon extends StatelessWidget {
  const MgStatusTelemetryIcon({
    super.key,
    required this.kind,
    required this.value,
    required this.color,
  });

  final MgStatusIconKind kind;
  final double value;
  final Color color;

  @override
  Widget build(BuildContext context) => TweenAnimationBuilder<double>(
    tween: Tween(end: value.clamp(0, 1)),
    duration: MediaQuery.disableAnimationsOf(context)
        ? Duration.zero
        : const Duration(milliseconds: 320),
    curve: Curves.easeOutCubic,
    builder: (context, level, _) => CustomPaint(
      size: const Size(24, 20),
      painter: _StatusIconPainter(kind, level, color),
    ),
  );
}

class _StatusIconPainter extends CustomPainter {
  const _StatusIconPainter(this.kind, this.level, this.color);

  final MgStatusIconKind kind;
  final double level;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 26, size.height / 22);
    final edge = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.6
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;
    final fill = Paint()..color = color;
    if (kind == MgStatusIconKind.battery) {
      final shell = RRect.fromRectAndRadius(
        const Rect.fromLTWH(1, 4, 21, 14),
        const Radius.circular(3),
      );
      canvas.drawRRect(shell, edge);
      canvas.drawLine(const Offset(24.5, 9), const Offset(24.5, 13), edge);
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          const Rect.fromLTWH(3.5, 6.5, 16, 9),
          const Radius.circular(1),
        ),
        Paint()..color = color.withValues(alpha: 0.12),
      );
      if (level > 0) {
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            Rect.fromLTWH(3.5, 6.5, 16 * level, 9),
            const Radius.circular(1),
          ),
          fill,
        );
      }
    } else {
      canvas.save();
      canvas.translate(9, 9);
      canvas.rotate(-math.pi / 4);
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          const Rect.fromLTWH(-3, -3, 6, 6),
          const Radius.circular(1),
        ),
        edge,
      );
      for (final x in [-8.0, 5.0]) {
        canvas.drawRect(Rect.fromLTWH(x, -4, 3, 8), edge);
      }
      canvas.drawLine(const Offset(0, 3), const Offset(0, 6), edge);
      canvas.restore();
      for (var index = 0; index < 3; index++) {
        final strength = (level * 3 - index).clamp(0.0, 1.0);
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            Rect.fromLTWH(16 + index * 4, 19 - index * 4, 2, 3 + index * 4),
            const Radius.circular(1),
          ),
          Paint()..color = color.withValues(alpha: 0.18 + strength * 0.82),
        );
      }
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(_StatusIconPainter oldDelegate) =>
      oldDelegate.kind != kind ||
      oldDelegate.level != level ||
      oldDelegate.color != color;
}
