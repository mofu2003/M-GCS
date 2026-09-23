import 'dart:math' as math;

import 'package:latlong2/latlong.dart';

import 'mg_map_catalog.dart';

/// Converts domain coordinates (MAVLink/GPS WGS-84) to the coordinate system
/// required by a map source. Keep this at the rendering boundary so vehicle,
/// mission, and log data remain in their original WGS-84 representation.
LatLng mgCoordinateForMap(LatLng coordinate, MgMapCoordinateSystem system) {
  return switch (system) {
    MgMapCoordinateSystem.wgs84 => coordinate,
    MgMapCoordinateSystem.gcj02 => mgWgs84ToGcj02(coordinate),
  };
}

/// Converts a coordinate returned by the map renderer back to the domain
/// coordinate system used by MAVLink and mission storage.
///
/// Map providers such as Gaode render GCJ-02 coordinates, while mission data
/// remains WGS-84. Keeping the inverse conversion here prevents map-specific
/// details from leaking into mission editing code.
LatLng mgCoordinateFromMap(LatLng coordinate, MgMapCoordinateSystem system) {
  return switch (system) {
    MgMapCoordinateSystem.wgs84 => coordinate,
    MgMapCoordinateSystem.gcj02 => mgGcj02ToWgs84(coordinate),
  };
}

LatLng mgWgs84ToGcj02(LatLng coordinate) {
  if (_outOfChina(coordinate.latitude, coordinate.longitude)) {
    return coordinate;
  }
  final d = _delta(coordinate.latitude, coordinate.longitude);
  return LatLng(
    coordinate.latitude + d.latitude,
    coordinate.longitude + d.longitude,
  );
}

LatLng mgGcj02ToWgs84(LatLng coordinate) {
  if (_outOfChina(coordinate.latitude, coordinate.longitude)) {
    return coordinate;
  }
  final d = _delta(coordinate.latitude, coordinate.longitude);
  return LatLng(
    coordinate.latitude - d.latitude,
    coordinate.longitude - d.longitude,
  );
}

bool _outOfChina(double latitude, double longitude) {
  return longitude < 72.004 ||
      longitude > 137.8347 ||
      latitude < 0.8293 ||
      latitude > 55.8271;
}

LatLng _delta(double latitude, double longitude) {
  final dLat = _transformLatitude(longitude - 105.0, latitude - 35.0);
  final dLon = _transformLongitude(longitude - 105.0, latitude - 35.0);
  final radLat = latitude / 180.0 * math.pi;
  var magic = math.sin(radLat);
  magic = 1 - 0.00669342162296594323 * magic * magic;
  final sqrtMagic = math.sqrt(magic);
  return LatLng(
    dLat *
        180.0 /
        ((6335552.717000426 * (1 - 0.00669342162296594323)) /
            (magic * sqrtMagic) *
            math.pi),
    dLon * 180.0 / (6378245.0 / sqrtMagic * math.cos(radLat) * math.pi),
  );
}

double _transformLatitude(double x, double y) {
  var ret =
      -100.0 +
      2.0 * x +
      3.0 * y +
      0.2 * y * y +
      0.1 * x * y +
      0.2 * math.sqrt(x.abs());
  ret +=
      (20.0 * math.sin(6.0 * x * math.pi) +
          20.0 * math.sin(2.0 * x * math.pi)) *
      2.0 /
      3.0;
  ret +=
      (20.0 * math.sin(y * math.pi) + 40.0 * math.sin(y / 3.0 * math.pi)) *
      2.0 /
      3.0;
  ret +=
      (160.0 * math.sin(y / 12.0 * math.pi) +
          320 * math.sin(y * math.pi / 30.0)) *
      2.0 /
      3.0;
  return ret;
}

double _transformLongitude(double x, double y) {
  var ret =
      300.0 +
      x +
      2.0 * y +
      0.1 * x * x +
      0.1 * x * y +
      0.1 * math.sqrt(x.abs());
  ret +=
      (20.0 * math.sin(6.0 * x * math.pi) +
          20.0 * math.sin(2.0 * x * math.pi)) *
      2.0 /
      3.0;
  ret +=
      (20.0 * math.sin(x * math.pi) + 40.0 * math.sin(x / 3.0 * math.pi)) *
      2.0 /
      3.0;
  ret +=
      (150.0 * math.sin(x / 12.0 * math.pi) +
          300.0 * math.sin(x / 30.0 * math.pi)) *
      2.0 /
      3.0;
  return ret;
}
