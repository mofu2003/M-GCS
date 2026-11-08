import 'dart:math' as math;

import 'package:latlong2/latlong.dart';

import 'mission_geo.dart';

enum QuickMissionShape {
  circle('航点圈'),
  polyline('等距折线'),
  rectangle('矩形航线'),
  polygon('多边形边界');

  const QuickMissionShape(this.label);
  final String label;
}

const int maxQuickMissionPoints = 1000;
const double _earthRadius = 6371000;

double _normalizeLongitude(double degrees) => (degrees + 180) % 360 - 180;

LatLng quickMissionDestination(LatLng origin, double bearing, double meters) {
  final latitude = origin.latitude * math.pi / 180;
  final longitude = origin.longitude * math.pi / 180;
  final angle = bearing * math.pi / 180;
  final distance = meters / _earthRadius;
  final resultLatitude = math.asin(
    (math.sin(latitude) * math.cos(distance) +
            math.cos(latitude) * math.sin(distance) * math.cos(angle))
        .clamp(-1.0, 1.0),
  );
  final resultLongitude =
      longitude +
      math.atan2(
        math.sin(angle) * math.sin(distance) * math.cos(latitude),
        math.cos(distance) - math.sin(latitude) * math.sin(resultLatitude),
      );
  return LatLng(
    resultLatitude * 180 / math.pi,
    _normalizeLongitude(resultLongitude * 180 / math.pi),
  );
}

double quickMissionBearing(LatLng from, LatLng to) {
  final start = from.latitude * math.pi / 180;
  final end = to.latitude * math.pi / 180;
  final delta =
      _normalizeLongitude(to.longitude - from.longitude) * math.pi / 180;
  return math.atan2(
        math.sin(delta) * math.cos(end),
        math.cos(start) * math.sin(end) -
            math.sin(start) * math.cos(end) * math.cos(delta),
      ) *
      180 /
      math.pi;
}

List<LatLng> generateQuickMissionPoints({
  required QuickMissionShape shape,
  required List<LatLng> controls,
  double radiusMeters = 50,
  int circlePoints = 20,
  double spacingMeters = 50,
  double startBearing = 0,
  bool clockwise = true,
  bool closed = true,
}) {
  for (final point in controls) {
    if (!point.latitude.isFinite ||
        !point.longitude.isFinite ||
        point.latitude.abs() > 90 ||
        point.longitude.abs() > 180) {
      throw ArgumentError('控制点经纬度无效');
    }
  }
  if (shape == QuickMissionShape.circle) {
    if (controls.isEmpty) throw ArgumentError('请先在地图上选择圆心');
    if (!radiusMeters.isFinite ||
        radiusMeters <= 0 ||
        radiusMeters > math.pi * _earthRadius ||
        !startBearing.isFinite) {
      throw ArgumentError('半径必须大于 0，首点角度必须有效');
    }
    if (circlePoints < 3 ||
        circlePoints + (closed ? 1 : 0) > maxQuickMissionPoints) {
      throw ArgumentError('圆周点数至少为 3，生成航点总数最多为 $maxQuickMissionPoints');
    }
    final points = List.generate(
      circlePoints,
      (index) => quickMissionDestination(
        controls.first,
        startBearing + (clockwise ? 1 : -1) * index * 360 / circlePoints,
        radiusMeters,
      ),
    );
    if (closed) points.add(points.first);
    return points;
  }
  if (!spacingMeters.isFinite || spacingMeters <= 0) {
    throw ArgumentError('航点间距必须大于 0');
  }
  var vertices = List<LatLng>.of(controls);
  if (shape == QuickMissionShape.rectangle) {
    if (vertices.length != 2) throw ArgumentError('请在地图上选择矩形的两个对角');
    final first = vertices.first;
    final last = vertices.last;
    if ((first.latitude - last.latitude).abs() < 1e-8 ||
        _normalizeLongitude(first.longitude - last.longitude).abs() < 1e-8) {
      throw ArgumentError('矩形需要有效的宽度和高度');
    }
    vertices = [
      first,
      LatLng(first.latitude, last.longitude),
      last,
      LatLng(last.latitude, first.longitude),
    ];
  }
  final minimum = shape == QuickMissionShape.polyline ? 2 : 3;
  if (vertices.length < minimum) throw ArgumentError('请至少选择 $minimum 个控制点');
  if (shape == QuickMissionShape.polygon) {
    final origin = vertices.first;
    var area = 0.0;
    for (var index = 0; index < vertices.length; index++) {
      final first = vertices[index];
      final next = vertices[(index + 1) % vertices.length];
      area +=
          _normalizeLongitude(first.longitude - origin.longitude) *
              (next.latitude - origin.latitude) -
          _normalizeLongitude(next.longitude - origin.longitude) *
              (first.latitude - origin.latitude);
    }
    if (area.abs() < 1e-12) throw ArgumentError('多边形边界不能共线，请重新选择控制点');
  }
  if (closed) vertices.add(vertices.first);
  final result = <LatLng>[vertices.first];
  for (var index = 1; index < vertices.length; index++) {
    final from = vertices[index - 1];
    final to = vertices[index];
    final distance = wgs84DistanceMeters(from, to);
    if (distance < 0.01) continue;
    final segmentCount = distance / spacingMeters;
    if (!segmentCount.isFinite ||
        segmentCount > maxQuickMissionPoints - result.length) {
      throw ArgumentError('生成航点超过 $maxQuickMissionPoints 个，请增大间距');
    }
    final segments = segmentCount.ceil();
    final bearing = quickMissionBearing(from, to);
    for (var part = 1; part < segments; part++) {
      result.add(
        quickMissionDestination(from, bearing, distance * part / segments),
      );
    }
    result.add(to);
  }
  if (result.length < minimum) throw ArgumentError('控制点距离过近，请重新绘制');
  return result;
}
