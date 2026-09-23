import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs/features/map/mg_coordinate_transform.dart';
import 'package:mgcs/features/map/mg_map_catalog.dart';

void main() {
  test('does not transform coordinates outside mainland China', () {
    const coordinate = LatLng(37.7749, -122.4194);
    expect(mgWgs84ToGcj02(coordinate), coordinate);
  });

  test('converts Shanghai WGS-84 to GCJ-02 at the map boundary', () {
    const coordinate = LatLng(31.2304, 121.4737);
    final converted = mgCoordinateForMap(
      coordinate,
      MgMapCoordinateSystem.gcj02,
    );
    expect(converted.latitude, closeTo(31.228445, 0.00002));
    expect(converted.longitude, closeTo(121.478225, 0.00002));
  });

  test('converts a map coordinate back to the WGS-84 mission domain', () {
    const coordinate = LatLng(31.2304, 121.4737);
    final mapCoordinate = mgCoordinateForMap(
      coordinate,
      MgMapCoordinateSystem.gcj02,
    );
    final restored = mgCoordinateFromMap(
      mapCoordinate,
      MgMapCoordinateSystem.gcj02,
    );
    expect(restored.latitude, closeTo(coordinate.latitude, 0.00002));
    expect(restored.longitude, closeTo(coordinate.longitude, 0.00002));
  });
}
