import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs_frontend/mission/mission_geo.dart';

void main() {
  test('mission distance uses Mission Planner Haversine radius', () {
    final distance = wgs84DistanceMeters(
      const LatLng(0, 0),
      const LatLng(1, 0),
    );

    expect(distance, closeTo(111194.9, 0.5));
  });

  test('mission distance formatting switches to kilometres', () {
    expect(formatMissionDistance(89.4), '89 m');
    expect(formatMissionDistance(1240), '1.24 km');
  });
}
