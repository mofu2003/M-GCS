import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/flight_command_ffi_backend.dart';

void main() {
  test(
    'typed commands dispatch with explicit vehicle id and JSON args',
    () async {
      final calls = <Map<String, Object?>>[];
      var beforeCalls = 0;
      final backend = FlightCommandFfiBackend(
        DynamicLibrary.process(),
        nullptr,
        beforeCommand: () => beforeCalls++,
        targetVehicleId: () => 42,
        freeStringOverride: malloc.free,
        executeForVehicleOverride: (vehicleId, commandJson) {
          calls.add({
            'vehicle_id': vehicleId,
            'command': jsonDecode(commandJson.toDartString()),
          });
          return jsonEncode({'success': true}).toNativeUtf8();
        },
      );

      expect((await backend.rtl())['success'], isTrue);
      expect((await backend.takeoff(30.5))['success'], isTrue);
      expect(
        (await backend.guidedPoint(31.2, 121.4, 50, -1, 5))['success'],
        isTrue,
      );
      expect(
        (await backend.orbit(
          latitude: 31.2,
          longitude: 121.4,
          radiusMeters: 80,
          velocityMetersPerSecond: 12,
          altitudeMeters: 120,
        ))['success'],
        isTrue,
      );

      expect(beforeCalls, 4);
      expect(calls.map((call) => call['vehicle_id']), everyElement(42));
      expect(calls[0]['command'], {
        'command': 'rtl',
        'args': <String, Object?>{},
      });
      expect(calls[1]['command'], {
        'command': 'takeoff',
        'args': {'altitude_m': 30.5},
      });
      expect(calls[2]['command'], {
        'command': 'guided_point',
        'args': {
          'lat': 31.2,
          'lon': 121.4,
          'alt': 50.0,
          'yaw': -1.0,
          'speed': 5.0,
        },
      });
      expect(calls[3]['command'], {
        'command': 'orbit',
        'args': {
          'lat': 31.2,
          'lon': 121.4,
          'radius': 80.0,
          'velocity': 12.0,
          'altitude': 120.0,
        },
      });
    },
  );

  test('missing active vehicle blocks native invocation', () async {
    var invoked = false;
    final backend = FlightCommandFfiBackend(
      DynamicLibrary.process(),
      nullptr,
      targetVehicleId: () => null,
      freeStringOverride: malloc.free,
      executeForVehicleOverride: (vehicleId, commandJson) {
        invoked = true;
        return jsonEncode({'success': true}).toNativeUtf8();
      },
    );
    await expectLater(backend.land(), throwsStateError);
    expect(invoked, isFalse);
  });

  test('explicit vehicle command bypasses active focus', () async {
    final calls = <Map<String, Object?>>[];
    var beforeCalls = 0;
    final backend = FlightCommandFfiBackend(
      DynamicLibrary.process(),
      nullptr,
      beforeCommand: () => beforeCalls++,
      targetVehicleId: () => null,
      freeStringOverride: malloc.free,
      executeForVehicleOverride: (vehicleId, commandJson) {
        calls.add({
          'vehicle_id': vehicleId,
          'command': jsonDecode(commandJson.toDartString()),
        });
        return jsonEncode({'success': true}).toNativeUtf8();
      },
    );

    final result = await backend.executeRawForVehicle(7, 'rtl');

    expect(result['success'], isTrue);
    expect(beforeCalls, 0);
    expect(calls, [
      {
        'vehicle_id': 7,
        'command': {'command': 'rtl', 'args': <String, Object?>{}},
      },
    ]);
  });
  test(
    'pending command yields to the event loop and reports rejected result',
    () async {
      var polls = 0;
      var timerRan = false;
      final backend = FlightCommandFfiBackend(
        DynamicLibrary.process(),
        nullptr,
        targetVehicleId: () => 7,
        freeStringOverride: malloc.free,
        executeForVehicleOverride: (_, _) =>
            jsonEncode({'success': true, 'ticket': 12}).toNativeUtf8(),
        pollForTicketOverride: (ticket) {
          expect(ticket, 12);
          polls++;
          return jsonEncode(
            polls < 2
                ? {'success': true, 'state': 'pending'}
                : {
                    'success': true,
                    'state': 'complete',
                    'result': {'success': false, 'result_str': 'denied'},
                  },
          ).toNativeUtf8();
        },
      );
      Future<void>.delayed(Duration.zero, () => timerRan = true);
      final result = await backend.rtl();
      expect(timerRan, isTrue);
      expect(result['success'], isFalse);
      expect(result['result_str'], 'denied');
      backend.dispose();
      await expectLater(backend.rtl(), throwsStateError);
    },
  );
}
