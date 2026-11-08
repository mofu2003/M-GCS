import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs_frontend/mission/mission_geo.dart';
import 'package:mgcs_frontend/mission/quick_mission_geometry.dart';

void main() {
  const center = LatLng(32.5, 114.7);
  test('circle preserves radius, direction and exact closing point', () {
    for (final clockwise in [true, false]) {
      final points = generateQuickMissionPoints(
        shape: QuickMissionShape.circle,
        controls: [center],
        radiusMeters: 123,
        circlePoints: 12,
        clockwise: clockwise,
      );
      expect(points.length, 13);
      expect(points.first, points.last);
      for (final point in points) {
        expect(wgs84DistanceMeters(center, point), closeTo(123, 0.001));
      }
      expect(points[1].longitude > center.longitude, clockwise);
    }
  });
  test('polyline keeps corners and limits inter-point distance', () {
    final end = quickMissionDestination(center, 90, 121);
    final turn = quickMissionDestination(end, 0, 89);
    final points = generateQuickMissionPoints(
      shape: QuickMissionShape.polyline,
      controls: [center, end, turn],
      spacingMeters: 50,
      closed: false,
    );
    expect(points, contains(end));
    expect(points.first, center);
    expect(points.last, turn);
    for (var index = 1; index < points.length; index++) {
      expect(
        wgs84DistanceMeters(points[index - 1], points[index]),
        lessThanOrEqualTo(50.001),
      );
    }
  });
  test('rectangle crossing date line takes the short boundary', () {
    final points = generateQuickMissionPoints(
      shape: QuickMissionShape.rectangle,
      controls: [const LatLng(10, 179.999), const LatLng(10.001, -179.999)],
      spacingMeters: 50,
    );
    expect(points.length, lessThan(20));
    expect(points.first, points.last);
  });
  test('invalid inputs and excessive density are rejected', () {
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.polyline,
        controls: [center, quickMissionDestination(center, 0, 111)],
        spacingMeters: 1e-309,
        closed: false,
      ),
      throwsArgumentError,
    );
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.polygon,
        controls: [
          const LatLng(32, 114),
          const LatLng(32, 115),
          const LatLng(32, 116),
        ],
      ),
      throwsArgumentError,
    );
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.circle,
        controls: [center],
        radiusMeters: double.nan,
      ),
      throwsArgumentError,
    );
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.circle,
        controls: [center],
        circlePoints: 1000,
      ),
      throwsArgumentError,
    );
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.rectangle,
        controls: [center, center],
      ),
      throwsArgumentError,
    );
    expect(
      () => generateQuickMissionPoints(
        shape: QuickMissionShape.polyline,
        controls: [center, quickMissionDestination(center, 0, 2000)],
        spacingMeters: 0.01,
        closed: false,
      ),
      throwsArgumentError,
    );
  });
}
