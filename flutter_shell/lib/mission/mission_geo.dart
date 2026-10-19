import 'dart:math' as math;

import 'package:latlong2/latlong.dart';

import 'mission_document_models.dart';

/// Mission Planner uses the Haversine formula with a 6371 km earth radius
/// for mission leg distances. Keep the same spherical model so the values in
/// M-GCS do not disagree with MP.
const double _missionEarthRadiusMeters = 6371000.0;

double wgs84DistanceMeters(LatLng start, LatLng end) {
  final lat1 = start.latitude * math.pi / 180.0;
  final lon1 = start.longitude * math.pi / 180.0;
  final lat2 = end.latitude * math.pi / 180.0;
  final lon2 = end.longitude * math.pi / 180.0;
  final deltaLat = lat2 - lat1;
  final deltaLon = lon2 - lon1;
  final a =
      math.pow(math.sin(deltaLat / 2.0), 2.0) +
      math.cos(lat1) * math.cos(lat2) * math.pow(math.sin(deltaLon / 2.0), 2.0);
  final c = 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a));
  return _missionEarthRadiusMeters * c;
}

String formatMissionDistance(double meters) {
  if (meters >= 1000) {
    final decimals = meters >= 10000 ? 1 : 2;
    return '${(meters / 1000).toStringAsFixed(decimals)} km';
  }
  return '${meters.round()} m';
}

/// Where a mission entity should be drawn on the map.
///
/// Takeoff commands do not fly to their stored MAVLink reference coordinate:
/// ArduPilot takes off from the current/Home position. Prefer the mission Home
/// for those items, but keep a stored or just-clicked reference as a fallback
/// when the document has no Home yet.
LatLng? missionEntityMapPosition(
  MissionEntity entity,
  MissionHome home, {
  LatLng? preview,
}) {
  if (preview != null) return preview;

  if (entity.isTakeoff) {
    final homeLatitude = home.latitude;
    final homeLongitude = home.longitude;
    if (home.set &&
        homeLatitude != null &&
        homeLongitude != null &&
        homeLatitude.isFinite &&
        homeLongitude.isFinite) {
      return LatLng(homeLatitude, homeLongitude);
    }
  }

  final latitude = entity.latitude;
  final longitude = entity.longitude;
  if (latitude == null ||
      longitude == null ||
      !latitude.isFinite ||
      !longitude.isFinite) {
    return null;
  }
  return LatLng(latitude, longitude);
}
