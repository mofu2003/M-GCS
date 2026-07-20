import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_tokens.dart';

final class MacGlassAttitudePanel extends StatelessWidget {
  const MacGlassAttitudePanel({required this.model, super.key});

  final FrontstageAttitudeModel model;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      key: const ValueKey('mg-attitude-panel'),
      borderRadius: BorderRadius.circular(24),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.58),
            border: Border.all(color: Colors.white.withValues(alpha: 0.86)),
            borderRadius: BorderRadius.circular(24),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Padding(
            padding: const EdgeInsets.all(10),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    _ReferenceAttitudeCompass(
                      roll: model.rollDegrees,
                      pitch: model.pitchDegrees,
                      heading: model.headingDegrees,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: SizedBox(
                        key: const ValueKey('mg-attitude-metric-region'),
                        height: 196,
                        child: _FlightMetricGrid(
                          heading: model.headingDegrees,
                          pitch: model.pitchDegrees,
                          roll: model.rollDegrees,
                          airspeed: model.airspeedLabel,
                          groundSpeed: model.groundSpeedLabel,
                          verticalSpeed: model.verticalSpeedLabel,
                        ),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

final class _ReferenceAttitudeCompass extends StatelessWidget {
  const _ReferenceAttitudeCompass({
    required this.roll,
    required this.pitch,
    required this.heading,
  });

  final double roll;
  final double pitch;
  final double heading;

  @override
  Widget build(BuildContext context) {
    return _AttitudeCompassLayers(
      values: _AttitudeValues(roll: roll, pitch: pitch, heading: heading),
    );
  }
}

final class _AttitudeCompassLayers extends StatelessWidget {
  const _AttitudeCompassLayers({required this.values});

  static const _glassSize = 184.0;
  static const _contentSize = 178.0;

  final _AttitudeValues values;

  @override
  Widget build(BuildContext context) {
    return SizedBox.square(
      key: const ValueKey('mg-attitude-instrument'),
      dimension: _glassSize,
      child: DecoratedBox(
        key: const ValueKey('mg-attitude-reference-dial'),
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: Colors.white.withValues(alpha: 0.62),
          border: Border.all(color: Colors.white.withValues(alpha: 0.84)),
          boxShadow: [
            BoxShadow(
              color: Colors.black.withValues(alpha: 0.14),
              blurRadius: 22,
              offset: const Offset(0, 10),
            ),
            BoxShadow(
              color: Colors.white.withValues(alpha: 0.76),
              blurRadius: 8,
              offset: const Offset(-4, -5),
            ),
          ],
        ),
        child: Center(
          child: SizedBox.square(
            dimension: _contentSize,
            child: Stack(
              clipBehavior: Clip.none,
              fit: StackFit.expand,
              children: [
                RepaintBoundary(
                  key: const ValueKey('mg-compass-dial-layer'),
                  child: CustomPaint(
                    painter: _CompassDialPainter(values.heading),
                  ),
                ),
                RepaintBoundary(
                  key: const ValueKey('mg-attitude-ball-layer'),
                  child: CustomPaint(
                    painter: _AttitudeBallPainter(
                      roll: values.roll,
                      pitch: values.pitch,
                    ),
                  ),
                ),
                const RepaintBoundary(
                  key: ValueKey('mg-heading-pointer-layer'),
                  child: CustomPaint(painter: _HeadingPointerPainter()),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

final class _AttitudeValues {
  const _AttitudeValues({
    required this.roll,
    required this.pitch,
    required this.heading,
  });

  final double roll;
  final double pitch;
  final double heading;
}

final class _AttitudeCompassGeometry {
  const _AttitudeCompassGeometry._();

  static const attitudeInset = 36.0;
  static const outerRingInset = 8.0;
  static const tickOuterInset = outerRingInset + 2.0;
  static const majorTickLength = 8.0;
  static const cardinalLabelRadiusInset = 26.0;
  static const bearingLabelRadiusInset = 4.0;

  static double attitudeRadius(double radius) => radius - attitudeInset;
  static double outerRingRadius(double radius) => radius - outerRingInset;

  static double cardinalLabelRadius(double radius) {
    return radius - cardinalLabelRadiusInset;
  }

  static double bearingLabelRadius(double radius) {
    return radius - bearingLabelRadiusInset;
  }
}

final class _CompassDialPainter extends CustomPainter {
  const _CompassDialPainter(this.heading);

  final double heading;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final image = _CompassDialImageCache.get(size);

    canvas.save();
    canvas.translate(center.dx, center.dy);
    canvas.rotate(-heading * math.pi / 180);
    canvas.translate(-center.dx, -center.dy);
    canvas.drawImageRect(
      image,
      Rect.fromLTWH(0, 0, image.width.toDouble(), image.height.toDouble()),
      Offset.zero & size,
      Paint()..filterQuality = FilterQuality.high,
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(_CompassDialPainter oldDelegate) {
    return oldDelegate.heading != heading;
  }
}

final class _CompassDialImageCache {
  const _CompassDialImageCache._();

  static const scale = 3.0;
  static final Map<Size, ui.Image> _images = {};

  static ui.Image get(Size size) {
    return _images.putIfAbsent(size, () {
      final imageSize = Size(size.width * scale, size.height * scale);
      final recorder = ui.PictureRecorder();
      final canvas = Canvas(recorder, Offset.zero & imageSize);
      canvas.scale(scale);
      _CompassStaticDialPainter.paintDial(canvas, size);
      final picture = recorder.endRecording();
      final image = picture.toImageSync(
        imageSize.width.round(),
        imageSize.height.round(),
      );
      picture.dispose();
      return image;
    });
  }
}

final class _CompassStaticDialPainter {
  const _CompassStaticDialPainter._();

  static void paintDial(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;
    _drawTicks(canvas, center, radius);
    _drawLabels(canvas, center, radius);
  }

  static void _drawTicks(Canvas canvas, Offset center, double radius) {
    final outerRadius = _AttitudeCompassGeometry.outerRingRadius(radius);
    final ringPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.1
      ..color = const Color(0x55FFFFFF);
    final tickPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    canvas.drawCircle(center, outerRadius, ringPaint);

    for (var i = 0; i < 72; i++) {
      final isMajor = i % 6 == 0;
      final angle = (i * 5 - 90) * math.pi / 180;
      final outer = radius - _AttitudeCompassGeometry.tickOuterInset;
      final inner =
          outer - (isMajor ? _AttitudeCompassGeometry.majorTickLength : 6);
      final p1 =
          center + Offset(math.cos(angle) * outer, math.sin(angle) * outer);
      final p2 =
          center + Offset(math.cos(angle) * inner, math.sin(angle) * inner);

      tickPaint
        ..strokeWidth = isMajor ? 1.7 : 1.0
        ..color = isMajor ? const Color(0xCC171A21) : const Color(0x55171A21);
      canvas.drawLine(p1, p2, tickPaint);
    }
  }

  static void _drawLabels(Canvas canvas, Offset center, double radius) {
    for (var bearing = 0; bearing < 360; bearing += 30) {
      _drawBearingLabel(canvas, center, radius, bearing);
    }

    _drawCardinalLabel(canvas, center, radius, 'N', 0);
    _drawCardinalLabel(canvas, center, radius, 'E', 90);
    _drawCardinalLabel(canvas, center, radius, 'S', 180);
    _drawCardinalLabel(canvas, center, radius, 'W', 270);
  }

  static void _drawCardinalLabel(
    Canvas canvas,
    Offset center,
    double radius,
    String label,
    int bearing,
  ) {
    _drawRadialText(
      canvas,
      center: center,
      radius: _AttitudeCompassGeometry.cardinalLabelRadius(radius),
      bearing: bearing.toDouble(),
      text: label,
      style: const TextStyle(
        fontSize: 10.5,
        height: 1,
        fontWeight: FontWeight.w800,
        color: Color(0xDD171A21),
      ),
    );
  }

  static void _drawBearingLabel(
    Canvas canvas,
    Offset center,
    double radius,
    int bearing,
  ) {
    _drawRadialText(
      canvas,
      center: center,
      radius: _AttitudeCompassGeometry.bearingLabelRadius(radius),
      bearing: bearing.toDouble(),
      text: bearing.toString(),
      style: const TextStyle(
        fontSize: 10.5,
        height: 1,
        fontWeight: FontWeight.w700,
        color: Color(0xA6171A21),
        fontFeatures: [ui.FontFeature.tabularFigures()],
      ),
    );
  }

  static void _drawRadialText(
    Canvas canvas, {
    required Offset center,
    required double radius,
    required double bearing,
    required String text,
    required TextStyle style,
  }) {
    final angle = (bearing - 90) * math.pi / 180;
    final labelCenter =
        center + Offset(math.cos(angle) * radius, math.sin(angle) * radius);
    final painter = _CompassLabelTextPainterCache.get(text, style);

    canvas.save();
    canvas.translate(labelCenter.dx, labelCenter.dy);
    canvas.rotate(bearing * math.pi / 180);
    painter.paint(canvas, Offset(-painter.width / 2, -painter.height / 2));
    canvas.restore();
  }
}

final class _CompassLabelTextPainterCache {
  const _CompassLabelTextPainterCache._();

  static final Map<_CompassLabelTextPainterKey, TextPainter> _painters = {};

  static TextPainter get(String text, TextStyle style) {
    final key = _CompassLabelTextPainterKey(text, style);
    return _painters.putIfAbsent(key, () {
      return TextPainter(
        text: TextSpan(text: text, style: style),
        textAlign: TextAlign.center,
        textDirection: TextDirection.ltr,
        maxLines: 1,
      )..layout();
    });
  }
}

final class _CompassLabelTextPainterKey {
  const _CompassLabelTextPainterKey(this.text, this.style);

  final String text;
  final TextStyle style;

  @override
  bool operator ==(Object other) {
    return other is _CompassLabelTextPainterKey &&
        other.text == text &&
        other.style == style;
  }

  @override
  int get hashCode => Object.hash(text, style);
}

final class _AttitudeBallPainter extends CustomPainter {
  const _AttitudeBallPainter({required this.roll, required this.pitch});

  final double roll;
  final double pitch;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;
    _drawInnerAttitude(canvas, center, radius);
  }

  void _drawInnerAttitude(Canvas canvas, Offset center, double radius) {
    final attitudeRadius = _AttitudeCompassGeometry.attitudeRadius(radius);
    final borderPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.1
      ..color = const Color(0x55FFFFFF);

    final rollRad = roll * math.pi / 180;
    final pitchOffset = pitch.clamp(-20, 20) / 20 * 22;

    canvas.save();
    canvas.clipPath(
      Path()..addOval(Rect.fromCircle(center: center, radius: attitudeRadius)),
    );
    canvas.translate(center.dx, center.dy);
    canvas.rotate(rollRad);

    final skyPaint = Paint()..color = const Color(0x8A8FC6EA);
    final groundPaint = Paint()..color = const Color(0x8AD9C6AA);
    final attitudeBounds = Rect.fromCircle(
      center: Offset.zero,
      radius: attitudeRadius,
    );
    canvas.drawRect(
      Rect.fromLTRB(
        attitudeBounds.left,
        attitudeBounds.top,
        attitudeBounds.right,
        pitchOffset,
      ),
      skyPaint,
    );
    canvas.drawRect(
      Rect.fromLTRB(
        attitudeBounds.left,
        pitchOffset,
        attitudeBounds.right,
        attitudeBounds.bottom,
      ),
      groundPaint,
    );

    final horizonPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.4
      ..strokeCap = StrokeCap.round
      ..color = const Color(0xEFFFFFFF);
    canvas.drawLine(
      Offset(-attitudeRadius + 16, pitchOffset),
      Offset(attitudeRadius - 16, pitchOffset),
      horizonPaint,
    );

    final pitchPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1
      ..strokeCap = StrokeCap.round
      ..color = const Color(0x55FFFFFF);
    for (final offset in const [-32.0, -16.0, 16.0, 32.0]) {
      canvas.drawLine(
        Offset(-18, pitchOffset + offset),
        Offset(18, pitchOffset + offset),
        pitchPaint,
      );
    }
    canvas.restore();

    canvas.drawCircle(center, attitudeRadius, borderPaint);
    _drawAircraftReference(canvas, center);
  }

  void _drawAircraftReference(Canvas canvas, Offset center) {
    final aircraftPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.2
      ..strokeCap = StrokeCap.round
      ..color = const Color(0xE8171A21);

    canvas.drawLine(
      center + const Offset(-32, 20),
      center + const Offset(-10, 20),
      aircraftPaint,
    );
    canvas.drawLine(
      center + const Offset(10, 20),
      center + const Offset(32, 20),
      aircraftPaint,
    );
    canvas.drawCircle(center, 3.4, Paint()..color = const Color(0xE8171A21));
  }

  @override
  bool shouldRepaint(_AttitudeBallPainter oldDelegate) {
    return oldDelegate.roll != roll || oldDelegate.pitch != pitch;
  }
}

final class _HeadingPointerPainter extends CustomPainter {
  const _HeadingPointerPainter();

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;
    final pointerPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = const Color(0xFF171A21);
    final path = Path()
      ..moveTo(center.dx, center.dy - radius + 16)
      ..lineTo(center.dx - 6, center.dy - radius + 30)
      ..lineTo(center.dx + 6, center.dy - radius + 30)
      ..close();
    canvas.drawPath(path, pointerPaint);
  }

  @override
  bool shouldRepaint(_HeadingPointerPainter oldDelegate) => false;
}

final class _FlightMetricGrid extends StatelessWidget {
  const _FlightMetricGrid({
    required this.heading,
    required this.pitch,
    required this.roll,
    required this.airspeed,
    required this.groundSpeed,
    required this.verticalSpeed,
  });

  final double heading;
  final double pitch;
  final double roll;
  final String airspeed;
  final String groundSpeed;
  final String verticalSpeed;

  @override
  Widget build(BuildContext context) {
    return Column(
      key: const ValueKey('mg-attitude-flight-metrics'),
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        _FlightMetricRow(
          left: _TelemetryTile(
            id: 'heading',
            label: '航向',
            value: '${_formatHeading(heading)}°',
          ),
          right: _TelemetryTile(id: 'airspeed', label: '空速', value: airspeed),
        ),
        _FlightMetricRow(
          left: _TelemetryTile(
            id: 'pitch',
            label: '俯仰',
            value: _signedDegrees(pitch),
          ),
          right: _TelemetryTile(
            id: 'groundspeed',
            label: '地速',
            value: groundSpeed,
          ),
        ),
        _FlightMetricRow(
          left: _TelemetryTile(
            id: 'roll',
            label: '横滚',
            value: _signedDegrees(roll),
          ),
          right: _TelemetryTile(
            id: 'vertical-speed',
            label: '升降速度',
            value: verticalSpeed,
          ),
        ),
      ],
    );
  }
}

final class _FlightMetricRow extends StatelessWidget {
  const _FlightMetricRow({required this.left, required this.right});

  final Widget left;
  final Widget right;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(child: left),
        const SizedBox(width: 4),
        Expanded(child: right),
      ],
    );
  }
}

final class _TelemetryTile extends StatelessWidget {
  const _TelemetryTile({
    required this.id,
    required this.label,
    required this.value,
  });

  final String id;
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      key: ValueKey('mg-telemetry-tile-$id'),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.58),
        border: Border.all(color: Colors.white.withValues(alpha: 0.78)),
        borderRadius: BorderRadius.circular(10),
      ),
      child: SizedBox(
        height: 60,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  color: MacGlassTokens.muted,
                  fontSize: 9,
                  fontWeight: FontWeight.w700,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                value,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  fontSize: 12,
                  fontWeight: FontWeight.w900,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

String _formatHeading(double value) {
  return _normalizeHeading(value).round().toString().padLeft(3, '0');
}

String _signedDegrees(double value) {
  final rounded = value.round();
  if (rounded > 0) return '+$rounded°';
  return '$rounded°';
}

double _normalizeHeading(double value) {
  final normalized = value % 360;
  if (normalized < 0) return normalized + 360;
  return normalized;
}
