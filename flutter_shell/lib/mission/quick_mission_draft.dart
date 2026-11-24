import 'package:flutter/foundation.dart';
import 'package:latlong2/latlong.dart';

import 'mission_geo.dart';
import 'quick_mission_geometry.dart';

class QuickMissionDraft extends ChangeNotifier {
  QuickMissionDraft(
    this.shape, {
    required this.altitudeMeters,
    required this.frame,
  }) : closed = shape != QuickMissionShape.polyline;

  final QuickMissionShape shape;
  bool frozen = false;
  final List<LatLng> _controls = [];
  List<LatLng> get controls => List.unmodifiable(_controls);
  double radiusMeters = 50;
  int circlePoints = 20;
  double spacingMeters = 50;
  double startBearing = 0;
  bool clockwise = true;
  bool closed;
  double altitudeMeters;
  int frame;

  void addPoint(LatLng point) {
    if (frozen) return;
    if ((shape == QuickMissionShape.circle ||
            shape == QuickMissionShape.rectangle) &&
        _controls.length == 2) {
      movePoint(1, point);
      return;
    }
    _controls.add(point);
    _updateCircle();
    notifyListeners();
  }

  void movePoint(int index, LatLng point) {
    if (frozen) return;
    if (index < 0 || index >= _controls.length) return;
    if (shape == QuickMissionShape.circle &&
        index == 0 &&
        _controls.length > 1) {
      final bearing = quickMissionBearing(_controls.first, _controls[1]);
      _controls[1] = quickMissionDestination(point, bearing, radiusMeters);
    }
    _controls[index] = point;
    _updateCircle();
    notifyListeners();
  }

  void _updateCircle() {
    if (shape == QuickMissionShape.circle && _controls.length == 2) {
      radiusMeters = wgs84DistanceMeters(_controls.first, _controls[1]);
      startBearing =
          (quickMissionBearing(_controls.first, _controls[1]) + 360) % 360;
    }
  }

  void removeLastPoint() {
    if (_controls.isNotEmpty) _controls.removeLast();
    notifyListeners();
  }

  void update() {
    if (shape == QuickMissionShape.circle && _controls.length == 2) {
      _controls[1] = quickMissionDestination(
        _controls.first,
        startBearing,
        radiusMeters,
      );
    }
    notifyListeners();
  }

  List<LatLng> generate() {
    if (!altitudeMeters.isFinite) throw ArgumentError('高度必须是有效数字');
    if (shape == QuickMissionShape.circle && _controls.length < 2) {
      throw ArgumentError('请先选择圆心，再点击第一个航点的位置');
    }
    final points = generateQuickMissionPoints(
      shape: shape,
      controls: _controls,
      radiusMeters: radiusMeters,
      circlePoints: circlePoints,
      spacingMeters: spacingMeters,
      startBearing: startBearing,
      clockwise: clockwise,
      closed: closed,
    );
    if (shape == QuickMissionShape.circle) {
      points[0] = _controls[1];
      if (closed) points[points.length - 1] = _controls[1];
    }
    return points;
  }

  String get instruction => switch (shape) {
    QuickMissionShape.circle =>
      _controls.isEmpty ? '先点击圆心，再点击第一个航点的位置' : '点击或拖动首个航点，确定半径和起始方向',
    QuickMissionShape.rectangle =>
      _controls.isEmpty ? '单击地图选择第一个角点' : '点击对角确定矩形，可拖动两个角点调整',
    QuickMissionShape.polyline => '依次单击地图添加转折点，可拖动控制点调整',
    QuickMissionShape.polygon => '依次单击地图描出边界，至少需要三个控制点',
  };
}
