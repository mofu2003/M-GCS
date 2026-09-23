import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs/presentation/features/map/mg_map_catalog.dart';
import 'package:mgcs/presentation/features/map/mg_map_surface.dart';

void main() {
  testWidgets('map resolves system location once per mounted surface', (
    WidgetTester tester,
  ) async {
    final controller = MapController();
    var locationCalls = 0;

    Widget build(MgMapStyle style) {
      return MaterialApp(
        home: MgMapSurface(
          style: style,
          controller: controller,
          locationResolver: () async {
            locationCalls++;
            return const LatLng(30, 120);
          },
        ),
      );
    }

    await tester.pumpWidget(build(MgMapStyle.standard));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 20));

    expect(locationCalls, 1);
    expect(controller.camera.center.latitude, closeTo(30, 0.000001));
    expect(controller.camera.center.longitude, closeTo(120, 0.000001));

    await tester.pumpWidget(build(MgMapStyle.terrain));
    await tester.pump(const Duration(milliseconds: 20));

    expect(locationCalls, 1);
  });
}
