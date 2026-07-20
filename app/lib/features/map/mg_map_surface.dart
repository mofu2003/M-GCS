import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import 'mg_map_catalog.dart';
import 'mg_device_locator.dart';

const LatLng mgDefaultMapCenter = LatLng(31.2304, 121.4737);
const double mgDefaultMapZoom = 12;

typedef MgLocationResolver = Future<LatLng?> Function();

final class MgMapSurface extends StatefulWidget {
  const MgMapSurface({
    required this.style,
    required this.controller,
    this.locationResolver,
    super.key,
  });

  final MgMapStyle style;
  final MapController controller;
  final MgLocationResolver? locationResolver;

  @override
  State<MgMapSurface> createState() => _MgMapSurfaceState();
}

final class _MgMapSurfaceState extends State<MgMapSurface> {
  bool _locationRequested = false;

  void _handleMapReady() {
    if (_locationRequested) {
      return;
    }
    _locationRequested = true;
    unawaited(_moveToSystemLocation());
  }

  Future<void> _moveToSystemLocation() async {
    final resolver = widget.locationResolver;
    final location =
        await (resolver?.call() ?? const MgDeviceLocator().locateOnce());
    if (!mounted || location == null) {
      return;
    }
    widget.controller.move(location, widget.controller.camera.zoom);
  }

  @override
  Widget build(BuildContext context) {
    final source = MgMapCatalog.sourceFor(widget.style);
    return Stack(
      key: const ValueKey('mg-map-surface'),
      fit: StackFit.expand,
      children: [
        const _MgMapFallbackBackground(),
        FlutterMap(
          mapController: widget.controller,
          options: MapOptions(
            initialCenter: mgDefaultMapCenter,
            initialZoom: mgDefaultMapZoom,
            minZoom: 3,
            maxZoom: 19,
            onMapReady: _handleMapReady,
          ),
          children: [
            TileLayer(
              urlTemplate: source.urlTemplate,
              fallbackUrl: source.fallbackUrlTemplate,
              maxNativeZoom: source.maxNativeZoom,
              userAgentPackageName: 'com.mgcs.app',
            ),
          ],
        ),
        Positioned(
          key: const ValueKey('mg-map-attribution'),
          right: 18,
          bottom: 10,
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: Colors.white.withValues(alpha: 0.72),
              borderRadius: BorderRadius.circular(9),
              border: Border.all(color: Colors.white.withValues(alpha: 0.82)),
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
              child: Text(
                source.attribution,
                style: const TextStyle(
                  color: Color(0xff323944),
                  fontSize: 11,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

final class _MgMapFallbackBackground extends StatelessWidget {
  const _MgMapFallbackBackground();

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      key: const ValueKey('mg-map-fallback-background'),
      painter: _MgMapFallbackPainter(),
    );
  }
}

final class _MgMapFallbackPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final base = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topLeft,
        end: Alignment.bottomRight,
        colors: [Color(0xffd7e3d3), Color(0xffedf1e8), Color(0xffcbdceb)],
      ).createShader(Offset.zero & size);
    canvas.drawRect(Offset.zero & size, base);

    _drawWater(canvas, size);
    _drawTerrainBands(canvas, size);
    _drawRoads(canvas, size);
  }

  void _drawWater(Canvas canvas, Size size) {
    final water = Paint()
      ..color = const Color(0xff8fb8ce).withValues(alpha: 0.46)
      ..style = PaintingStyle.fill;
    final river = ui.Path()
      ..moveTo(size.width * 0.68, -24)
      ..cubicTo(
        size.width * 0.52,
        size.height * 0.22,
        size.width * 0.82,
        size.height * 0.42,
        size.width * 0.58,
        size.height * 0.72,
      )
      ..cubicTo(
        size.width * 0.48,
        size.height * 0.86,
        size.width * 0.58,
        size.height,
        size.width * 0.44,
        size.height + 32,
      )
      ..lineTo(size.width * 0.58, size.height + 32)
      ..cubicTo(
        size.width * 0.72,
        size.height * 0.84,
        size.width * 0.62,
        size.height * 0.74,
        size.width * 0.76,
        size.height * 0.56,
      )
      ..cubicTo(
        size.width * 0.96,
        size.height * 0.28,
        size.width * 0.7,
        size.height * 0.14,
        size.width * 0.82,
        -24,
      )
      ..close();
    canvas.drawPath(river, water);
  }

  void _drawTerrainBands(Canvas canvas, Size size) {
    final bandPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.2
      ..color = const Color(0xff6f8f71).withValues(alpha: 0.22);

    for (var i = 0; i < 9; i++) {
      final y = size.height * (0.12 + i * 0.1);
      final path = ui.Path()..moveTo(-24, y);
      path.cubicTo(
        size.width * 0.22,
        y - 48 + i * 5,
        size.width * 0.44,
        y + 36,
        size.width * 0.66,
        y - 14,
      );
      path.cubicTo(
        size.width * 0.82,
        y - 50,
        size.width + 32,
        y + 20,
        size.width + 48,
        y - 8,
      );
      canvas.drawPath(path, bandPaint);
    }
  }

  void _drawRoads(Canvas canvas, Size size) {
    final minorRoad = Paint()
      ..color = Colors.white.withValues(alpha: 0.46)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.4
      ..strokeCap = StrokeCap.round;
    final majorRoad = Paint()
      ..color = const Color(0xfffff1b8).withValues(alpha: 0.76)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 5
      ..strokeCap = StrokeCap.round;

    for (var i = 0; i < 6; i++) {
      final x = size.width * (0.08 + i * 0.18);
      final path = ui.Path()
        ..moveTo(x, -20)
        ..cubicTo(
          x + 48,
          size.height * 0.25,
          x - 42,
          size.height * 0.54,
          x + 18,
          size.height + 24,
        );
      canvas.drawPath(path, minorRoad);
    }

    final highway = ui.Path()
      ..moveTo(-24, size.height * 0.7)
      ..cubicTo(
        size.width * 0.28,
        size.height * 0.6,
        size.width * 0.48,
        size.height * 0.78,
        size.width + 24,
        size.height * 0.62,
      );
    canvas.drawPath(highway, majorRoad);
    canvas.drawPath(
      highway,
      Paint()
        ..color = Colors.white.withValues(alpha: 0.52)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.5
        ..strokeCap = StrokeCap.round,
    );
  }

  @override
  bool shouldRepaint(covariant _MgMapFallbackPainter oldDelegate) => false;
}
