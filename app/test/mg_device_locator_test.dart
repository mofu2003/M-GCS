import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:mgcs/features/map/mg_device_locator.dart';

void main() {
  test(
    'returns null without requesting permission when service is disabled',
    () async {
      var permissionChecks = 0;
      final locator = MgDeviceLocator(
        serviceEnabled: () async => false,
        checkPermission: () async {
          permissionChecks++;
          return MgLocationPermission.granted;
        },
        requestPermission: () async => MgLocationPermission.granted,
        readLocation: (_) async => const LatLng(30, 120),
      );

      expect(await locator.locateOnce(), isNull);
      expect(permissionChecks, 0);
    },
  );

  test('returns null when permission remains denied', () async {
    var requests = 0;
    final locator = MgDeviceLocator(
      serviceEnabled: () async => true,
      checkPermission: () async => MgLocationPermission.denied,
      requestPermission: () async {
        requests++;
        return MgLocationPermission.denied;
      },
      readLocation: (_) async => const LatLng(30, 120),
    );

    expect(await locator.locateOnce(), isNull);
    expect(requests, 1);
  });

  test('returns the system location after permission is granted', () async {
    Duration? receivedTimeout;
    final locator = MgDeviceLocator(
      serviceEnabled: () async => true,
      checkPermission: () async => MgLocationPermission.granted,
      requestPermission: () async => MgLocationPermission.granted,
      readLocation: (timeout) async {
        receivedTimeout = timeout;
        return const LatLng(30, 120);
      },
    );

    final location = await locator.locateOnce();

    expect(location, const LatLng(30, 120));
    expect(receivedTimeout, const Duration(seconds: 5));
  });

  test('returns null when the platform location call throws', () async {
    final locator = MgDeviceLocator(
      serviceEnabled: () async => true,
      checkPermission: () async => MgLocationPermission.granted,
      requestPermission: () async => MgLocationPermission.granted,
      readLocation: (_) async => throw TimeoutException('location timeout'),
    );

    expect(await locator.locateOnce(), isNull);
  });
}
