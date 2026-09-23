import 'package:geolocator/geolocator.dart';
import 'package:latlong2/latlong.dart';

enum MgLocationPermission { denied, deniedForever, granted }

typedef MgLocationServiceCheck = Future<bool> Function();
typedef MgLocationPermissionCheck = Future<MgLocationPermission> Function();
typedef MgLocationReader = Future<LatLng> Function(Duration timeLimit);

final class MgDeviceLocator {
  const MgDeviceLocator({
    this.serviceEnabled,
    this.checkPermission,
    this.requestPermission,
    this.readLocation,
  });

  final MgLocationServiceCheck? serviceEnabled;
  final MgLocationPermissionCheck? checkPermission;
  final MgLocationPermissionCheck? requestPermission;
  final MgLocationReader? readLocation;

  Future<LatLng?> locateOnce() async {
    try {
      final isEnabled =
          await (serviceEnabled ?? Geolocator.isLocationServiceEnabled)();
      if (!isEnabled) {
        return null;
      }

      var permission = await (checkPermission ?? _checkPermission)();
      if (permission == MgLocationPermission.denied) {
        permission = await (requestPermission ?? _requestPermission)();
      }
      if (permission != MgLocationPermission.granted) {
        return null;
      }

      return await (readLocation ?? _readLocation)(const Duration(seconds: 5));
    } on Object {
      return null;
    }
  }
}

Future<MgLocationPermission> _checkPermission() async {
  return _mapPermission(await Geolocator.checkPermission());
}

Future<MgLocationPermission> _requestPermission() async {
  return _mapPermission(await Geolocator.requestPermission());
}

MgLocationPermission _mapPermission(LocationPermission permission) {
  if (permission == LocationPermission.whileInUse ||
      permission == LocationPermission.always) {
    return MgLocationPermission.granted;
  }
  if (permission == LocationPermission.deniedForever) {
    return MgLocationPermission.deniedForever;
  }
  return MgLocationPermission.denied;
}

Future<LatLng> _readLocation(Duration timeLimit) async {
  final position = await Geolocator.getCurrentPosition(
    locationSettings: LocationSettings(
      accuracy: LocationAccuracy.medium,
      timeLimit: timeLimit,
    ),
  );
  return LatLng(position.latitude, position.longitude);
}
