// Actual FleetFfiBackend + FleetController against two isolated loopback UDP
// sessions. No flight command, user port, SITL or hardware is touched.
// MGCS_FLEET_LIBRARY=/absolute/path/to/libmgcore.dylib flutter test --no-pub test/fleet_ffi_smoke_test.dart
import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_ffi_backend.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

const _heartbeats = <List<int>>[
  [253, 9, 0, 0, 0, 1, 1, 0, 0, 0, 4, 0, 0, 0, 2, 3, 129, 4, 3, 3, 86],
  [253, 9, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 2, 3, 129, 4, 3, 225, 62],
];

List<int> _freshSignedAutoDetectHeartbeat() {
  final packet = <int>[
    253,
    9,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    3,
    1,
    4,
    3,
    22,
    51,
    42,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
  ];
  final key = <int>[
    0x00,
    0x11,
    0x22,
    0x33,
    0x44,
    0x55,
    0x66,
    0x77,
    0x88,
    0x99,
    0xaa,
    0xbb,
    0xcc,
    0xdd,
    0xee,
    0xff,
    0x10,
    0x20,
    0x30,
    0x40,
    0x50,
    0x60,
    0x70,
    0x80,
    0x90,
    0xa0,
    0xb0,
    0xc0,
    0xd0,
    0xe0,
    0xf0,
    0x01,
  ];
  final ticks =
      (DateTime.now().microsecondsSinceEpoch - 1420070400000000) ~/ 10;
  for (var index = 0; index < 6; index++) {
    packet[22 + index] = (ticks >> (8 * index)) & 0xff;
  }
  final digest = sha256.convert(
    Uint8List.fromList(<int>[
      ...key,
      ...packet.sublist(0, 21),
      ...packet.sublist(21, 28),
    ]),
  );
  packet.setRange(28, 34, digest.bytes.take(6));
  return packet;
}

void main() {
  final libraryPath = Platform.environment['MGCS_FLEET_LIBRARY'];
  test(
    'actual Fleet FFI round trip over two isolated UDP sockets',
    () async {
      await runSmoke(libraryPath!);
    },
    skip: libraryPath == null
        ? 'Set MGCS_FLEET_LIBRARY for actual native smoke'
        : false,
  );
  test(
    'one UDP endpoint discovers two sysids as separate vehicles',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final rawFleet = controller.fleet;
      final senders = <RawDatagramSocket>[];
      final timers = <Timer>[];
      try {
        final probe = await RawDatagramSocket.bind(
          InternetAddress.loopbackIPv4,
          0,
        );
        final port = probe.port;
        probe.close();

        controller.selectTransport(ConnectionTransport.udp);
        controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
        await controller.connect();
        final linkId = controller.activeLinkId;
        expect(linkId, isNotNull);
        expect(rawFleet.snapshot.links.length, 1);
        expect(rawFleet.snapshot.vehicles, isEmpty);

        for (var index = 0; index < 2; index++) {
          final sender = await RawDatagramSocket.bind(
            InternetAddress.loopbackIPv4,
            0,
          );
          senders.add(sender);
          void heartbeat() => sender.send(
            _heartbeats[index],
            InternetAddress.loopbackIPv4,
            port,
          );
          heartbeat();
          timers.add(
            Timer.periodic(
              const Duration(milliseconds: 100),
              (_) => heartbeat(),
            ),
          );
        }

        final deadline = DateTime.now().add(const Duration(seconds: 4));
        while (DateTime.now().isBefore(deadline)) {
          rawFleet.refresh();
          final ids = rawFleet.snapshot.vehicles
              .map((vehicle) => vehicle.systemId)
              .toSet();
          if (ids.containsAll({1, 2})) break;
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }

        final vehicles = rawFleet.snapshot.vehicles;
        expect(vehicles.length, 2);
        expect(vehicles.map((vehicle) => vehicle.systemId), {1, 2});
        expect(vehicles.every((vehicle) => vehicle.connected), isTrue);
        expect(
          vehicles.map((vehicle) => vehicle.id),
          contains(rawFleet.snapshot.activeVehicleId),
        );

        final activeId = rawFleet.snapshot.activeVehicleId!;
        final other = vehicles.firstWhere((vehicle) => vehicle.id != activeId);
        rawFleet.selectVehicle(other.id);
        final switchDeadline = DateTime.now().add(const Duration(seconds: 3));
        while (controller.sessionId != other.id &&
            DateTime.now().isBefore(switchDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
        expect(controller.sessionId, other.id);
        final expectedMode = other.systemId == 2 ? 'Stabilize' : 'Guided';
        while (controller.vehicleFlightMode != expectedMode &&
            DateTime.now().isBefore(switchDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        expect(controller.vehicleFlightMode, expectedMode);

        // Closing one vehicle must leave the shared UDP socket usable for the
        // other vehicle on the same endpoint.
        final activeRoute = rawFleet.snapshot.links.firstWhere(
          (link) => link.vehicleId == activeId,
        );
        await controller.disconnectFleetLink(activeRoute);
        expect(controller.sessionId, other.id);
        expect(controller.isConnected, isTrue);
        expect(rawFleet.snapshot.vehicles.length, 1);
        expect(controller.vehicleFlightMode, expectedMode);

        final otherRoute = rawFleet.snapshot.links.firstWhere(
          (link) => link.vehicleId == other.id,
        );
        await controller.disconnectFleetLink(otherRoute);
        expect(controller.sessionId, isNull);
        expect(controller.isConnected, isFalse);
        expect(rawFleet.snapshot.vehicles, isEmpty);
      } finally {
        for (final timer in timers) {
          timer.cancel();
        }
        for (final sender in senders) {
          sender.close();
        }
      }
    },
    skip: libraryPath == null
        ? 'Set MGCS_FLEET_LIBRARY for actual native smoke'
        : false,
  );
  test(
    'ConnectionController isolates tasks, switches safely and preserves a healthy link',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final rawFleet = controller.fleet;
      final senders = <RawDatagramSocket>[];
      final timers = <Timer>[];
      final vehicleIds = <int>[];
      try {
        for (var index = 0; index < 2; index++) {
          final probe = await RawDatagramSocket.bind(
            InternetAddress.loopbackIPv4,
            0,
          );
          final port = probe.port;
          probe.close();
          // Exercise the actual connection form lifecycle, not only Fleet.open.
          controller.selectTransport(ConnectionTransport.udp);
          controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
          await controller.connect();
          expect(controller.fleetLinks.length, index + 1);
          final sender = await RawDatagramSocket.bind(
            InternetAddress.loopbackIPv4,
            0,
          );
          senders.add(sender);
          void heartbeat() => sender.send(
            _heartbeats[index],
            InternetAddress.loopbackIPv4,
            port,
          );
          heartbeat();
          timers.add(
            Timer.periodic(
              const Duration(milliseconds: 100),
              (_) => heartbeat(),
            ),
          );
          final discoveryDeadline = DateTime.now().add(
            const Duration(seconds: 3),
          );
          while (rawFleet.snapshot.vehicles.length < index + 1 &&
              DateTime.now().isBefore(discoveryDeadline)) {
            rawFleet.refresh();
            await Future<void>.delayed(const Duration(milliseconds: 20));
          }
          expect(rawFleet.snapshot.vehicles.length, index + 1);
          vehicleIds.add(rawFleet.snapshot.vehicles.last.id);
        }
        final deadline = DateTime.now().add(const Duration(seconds: 4));
        while (DateTime.now().isBefore(deadline)) {
          rawFleet.refresh();
          if (rawFleet.snapshot.vehicles.length == 2 &&
              rawFleet.snapshot.vehicles.every((v) => v.connected)) {
            break;
          }
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        while (controller.vehicleFlightMode != 'Guided' &&
            DateTime.now().isBefore(deadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        expect(controller.sessionId, vehicleIds.first);
        expect(controller.vehicleFlightMode, 'Guided');
        controller.waveformImuPointCount = 17;
        controller.flightTimeSeconds = 42.5;
        controller.flightDistanceMeters = 321.0;
        final missionA = controller.missionPlanning;
        await missionA.initialize();
        await missionA.create({
          'metadata': {'name': 'Aircraft A draft'},
        });
        expect(missionA.error, isNull);
        controller.guidedTargetValid = true;
        controller.guidedTargetPosition = const VehicleFix(
          latitude: 30,
          longitude: 120,
        );

        rawFleet.selectVehicle(vehicleIds.last);
        final switchDeadline = DateTime.now().add(const Duration(seconds: 3));
        while (controller.sessionId != vehicleIds.last &&
            DateTime.now().isBefore(switchDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
        expect(controller.sessionId, vehicleIds.last);
        while (controller.vehicleFlightMode != 'Stabilize' &&
            DateTime.now().isBefore(switchDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        expect(controller.vehicleFlightMode, 'Stabilize');
        expect(controller.waveformImuPointCount, 0);
        expect(controller.flightTimeSeconds, 0);
        expect(controller.flightDistanceMeters, 0);
        controller.waveformImuPointCount = 23;
        controller.flightTimeSeconds = 84.0;
        controller.flightDistanceMeters = 654.0;
        final missionB = controller.missionPlanning;
        await missionB.initialize();
        expect(missionB.document?.metadata.name, isNot('Aircraft A draft'));
        await missionB.create({
          'metadata': {'name': 'Aircraft B draft'},
        });
        expect(missionB.error, isNull);
        expect(identical(missionA, missionB), isFalse);
        // A detached editing callback must not replace B's native document.
        await missionA.create({
          'metadata': {'name': 'Wrong aircraft'},
        });
        expect(missionA.error, contains('飞机已切换'));
        expect(
          (await missionB.backend.getMissionDocument()).metadata.name,
          'Aircraft B draft',
        );
        // Busy task operations block user-requested focus changes.
        missionB.busy = true;
        expect(
          () => rawFleet.selectVehicle(vehicleIds.first),
          throwsStateError,
        );
        expect(controller.sessionId, vehicleIds.last);
        missionB.busy = false;

        rawFleet.selectVehicle(vehicleIds.first);
        while (controller.sessionId != vehicleIds.first &&
            DateTime.now().isBefore(switchDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
        expect(controller.sessionId, vehicleIds.first);
        expect(controller.vehicleFlightMode, 'Guided');
        // Waveform samples are intentionally not cached across a switch;
        // flight statistics are restored from the vehicle runtime snapshot.
        expect(controller.waveformImuPointCount, 0);
        // Flight time is derived from a running stopwatch, so it advances by a
        // few milliseconds between arming and the assertion. Compare with a
        // tolerance instead of exact equality.
        expect(controller.flightTimeSeconds, closeTo(42.5, 0.5));
        expect(controller.flightDistanceMeters, 321.0);
        expect(controller.guidedTargetValid, isFalse);
        expect(controller.guidedTargetPosition, isNull);
        expect(identical(controller.missionPlanning, missionA), isTrue);
        expect(
          (await missionA.backend.getMissionDocument()).metadata.name,
          'Aircraft A draft',
        );

        // MV6: the production backend must dispatch an explicit batch action
        // to both simulated vehicles without moving active focus.
        final activeBeforeBatch = rawFleet.snapshot.activeVehicleId;
        rawFleet.selectAllVehicles();
        final batchTarget = rawFleet.captureBatchTarget(
          FleetBatchAction.startMission,
        );
        final batchResult = await rawFleet.runBatch(batchTarget);
        // The loopback smoke sender publishes a fixed Guided heartbeat and
        // does not model the post-MISSION_START mode transition. The backend
        // must report that state-confirmation failure rather than a false
        // success; a real simulator/FC can complete this workflow.
        expect(batchResult.allSucceeded, isFalse);
        expect(batchResult.results.every((item) => !item.success), isTrue);
        expect(batchResult.results.map((item) => item.vehicleId), vehicleIds);
        expect(rawFleet.snapshot.activeVehicleId, activeBeforeBatch);
        rawFleet.clearVehicleSelection();

        // Attempting to reopen an existing endpoint must not drop active A.
        final endpoint = Uri.parse(
          controller.fleetSnapshot.vehicles.first.endpoint,
        );
        controller.updateNetworkEndpoint(
          host: endpoint.host,
          port: '${endpoint.port}',
        );
        await controller.connect();
        expect(controller.sessionId, vehicleIds.first);
        expect(controller.isConnected, isTrue);
        expect(controller.fleetSnapshot.vehicles.length, 2);
        // Draft form edits must not change the active link's status label.
        controller.selectTransport(ConnectionTransport.tcp);
        expect(controller.selectedTransportLabel, 'UDP');

        final firstRoute = rawFleet.snapshot.links.firstWhere(
          (link) => link.vehicleId == vehicleIds.first,
        );
        await controller.disconnectFleetLink(firstRoute);
        expect(controller.sessionId, vehicleIds.last);
        expect(controller.isConnected, isTrue);
        expect(controller.vehicleFlightMode, 'Stabilize');
        expect(
          (await controller.missionPlanning.backend.getMissionDocument())
              .metadata
              .name,
          'Aircraft B draft',
        );
        expect(controller.fleetSnapshot.vehicles.length, 1);
        final lastRoute = rawFleet.snapshot.links.firstWhere(
          (link) => link.vehicleId == vehicleIds.last,
        );
        await controller.disconnectFleetLink(lastRoute);
        expect(controller.sessionId, isNull);
        expect(controller.isConnected, isFalse);
        expect(controller.vehicleFlightMode, isEmpty);
        expect(controller.fleetSnapshot.vehicles, isEmpty);
      } finally {
        for (final timer in timers) {
          timer.cancel();
        }
        for (final sender in senders) {
          sender.close();
        }
      }
    },
    skip: libraryPath == null
        ? 'Set MGCS_FLEET_LIBRARY for actual native smoke'
        : false,
  );
  test(
    'native signing candidates auto-detect a signed heartbeat',
    () async {
      await runSigningAutoDetectSmoke(libraryPath!);
    },
    skip: libraryPath == null
        ? 'Set MGCS_FLEET_LIBRARY for actual native smoke'
        : false,
  );
}

Future<void> runSigningAutoDetectSmoke(String libraryPath) async {
  final library = DynamicLibrary.open(libraryPath);
  final create = library
      .lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>(
        'mg_create',
      );
  final destroy = library
      .lookupFunction<
        Void Function(Pointer<Void>),
        void Function(Pointer<Void>)
      >('mg_destroy');
  final freeString = library
      .lookupFunction<
        Void Function(Pointer<Utf8>),
        void Function(Pointer<Utf8>)
      >('mg_free_string');
  final setCandidates = library
      .lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
      >('mg_vehicle_set_signing_candidates');
  final getSigningStatus = library
      .lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>, Int32),
        Pointer<Utf8> Function(Pointer<Void>, int)
      >('mg_vehicle_get_signing_status');
  final core = create();
  final fleet = FleetController(FleetFfiBackend(library, core));
  RawDatagramSocket? sender;
  Timer? heartbeatTimer;
  int? linkId;
  try {
    final probe = await RawDatagramSocket.bind(InternetAddress.loopbackIPv4, 0);
    final port = probe.port;
    probe.close();
    linkId = fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:$port'));
    sender = await RawDatagramSocket.bind(InternetAddress.loopbackIPv4, 0);
    void heartbeat() =>
        sender!.send(_heartbeats[0], InternetAddress.loopbackIPv4, port);
    heartbeat();
    heartbeatTimer = Timer.periodic(
      const Duration(milliseconds: 100),
      (_) => heartbeat(),
    );

    final discoveryDeadline = DateTime.now().add(const Duration(seconds: 4));
    while (fleet.snapshot.vehicles.isEmpty &&
        DateTime.now().isBefore(discoveryDeadline)) {
      fleet.refresh();
      await Future<void>.delayed(const Duration(milliseconds: 50));
    }
    expect(fleet.snapshot.vehicles, isNotEmpty);
    final id = fleet.snapshot.vehicles.first.id;

    final candidatesJson = jsonEncode([
      {
        'name': 'AutoKey',
        'key_hex':
            '00112233445566778899aabbccddeeff'
            '102030405060708090a0b0c0d0e0f001',
        'timestamp_ticks': 0,
      },
    ]);
    final nativeJson = candidatesJson.toNativeUtf8();
    try {
      final response = setCandidates(core, id, nativeJson);
      if (response == nullptr) throw StateError('empty candidate response');
      try {
        final decoded = jsonDecode(response.toDartString());
        expect(
          decoded,
          isA<Map>().having((m) => m['success'], 'success', true),
        );
      } finally {
        freeString(response);
      }
    } finally {
      malloc.free(nativeJson);
    }

    heartbeatTimer.cancel();
    heartbeatTimer = null;
    sender.send(
      _freshSignedAutoDetectHeartbeat(),
      InternetAddress.loopbackIPv4,
      port,
    );

    Map<Object?, Object?>? status;
    final detectDeadline = DateTime.now().add(const Duration(seconds: 3));
    while (DateTime.now().isBefore(detectDeadline)) {
      final raw = getSigningStatus(core, id);
      if (raw == nullptr) break;
      try {
        final decoded = jsonDecode(raw.toDartString());
        if (decoded is Map) {
          status = decoded;
          if (decoded['auto_detected'] == true) break;
        }
      } finally {
        freeString(raw);
      }
      await Future<void>.delayed(const Duration(milliseconds: 50));
    }
    expect(status?['auto_detected'], isTrue);
    expect(status?['enabled'], isTrue);
    expect(status?['key_name'], 'AutoKey');
  } finally {
    heartbeatTimer?.cancel();
    sender?.close();
    if (linkId != null) {
      try {
        fleet.closeLink(linkId);
      } on Object {
        // Best effort during teardown.
      }
    }
    fleet.dispose();
    destroy(core);
  }
}

Future<void> runSmoke(String libraryPath) async {
  final library = DynamicLibrary.open(libraryPath);
  final create = library
      .lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>(
        'mg_create',
      );
  final destroy = library
      .lookupFunction<
        Void Function(Pointer<Void>),
        void Function(Pointer<Void>)
      >('mg_destroy');
  final core = create();
  final fleet = FleetController(FleetFfiBackend(library, core));
  final senders = <RawDatagramSocket>[];
  final timers = <Timer>[];
  final linkIds = <int>[];
  void check(bool condition, String label) {
    if (!condition) throw StateError(label);
    stdout.writeln('PASS: $label');
  }

  try {
    for (var index = 0; index < 2; index++) {
      final probe = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      final port = probe.port;
      probe.close();
      linkIds.add(fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:$port')));
      final sender = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      senders.add(sender);
      void heartbeat() =>
          sender.send(_heartbeats[index], InternetAddress.loopbackIPv4, port);
      heartbeat();
      timers.add(
        Timer.periodic(const Duration(milliseconds: 100), (_) => heartbeat()),
      );
    }
    final deadline = DateTime.now().add(const Duration(seconds: 4));
    while (DateTime.now().isBefore(deadline)) {
      fleet.refresh();
      if (fleet.snapshot.vehicles.length == 2 &&
          fleet.snapshot.vehicles.every((v) => v.connected && v.identified)) {
        break;
      }
      await Future<void>.delayed(const Duration(milliseconds: 50));
    }
    check(
      linkIds.length == 2 &&
          linkIds.every(
            (id) => fleet.snapshot.links.any((link) => link.id == id),
          ),
      'two physical links are reported separately',
    );
    check(
      fleet.snapshot.vehicles.length == 2 &&
          fleet.snapshot.vehicles.map((v) => v.systemId).toSet().containsAll({
            1,
            2,
          }),
      'two actual sessions have independent sysid identities',
    );
    final vehicleIds = fleet.snapshot.vehicles
        .map((vehicle) => vehicle.id)
        .toList(growable: false);
    check(
      fleet.snapshot.activeVehicleId == vehicleIds.first,
      'second connection does not steal focus',
    );
    final oldTarget = fleet.captureTarget();
    fleet.selectVehicle(vehicleIds.last);
    check(
      fleet.snapshot.activeVehicleId == vehicleIds.last,
      'native focus switching',
    );
    fleet.selectVehicle(vehicleIds.first);
    var rejected = false;
    try {
      fleet.validateTarget(oldTarget);
    } on StateError {
      rejected = true;
    }
    check(rejected, 'A-B-A invalidates old confirmation');
    final freshTarget = fleet.captureTarget();
    await fleet.runForTarget(freshTarget, () async {
      await Future<void>.delayed(const Duration(milliseconds: 50));
      fleet.refresh();
      stdout.writeln(
        'target diagnostic: target=$freshTarget revision=${fleet.focusRevision} active=${fleet.snapshot.activeVehicleId} vehicles=${fleet.snapshot.vehicles}',
      );
      fleet.validatePinnedTarget();
    });
    final removedRoute = fleet.snapshot.links.firstWhere(
      (link) => link.vehicleId == vehicleIds.last,
    );
    fleet.closeRoute(removedRoute.routeId);
    check(
      fleet.snapshot.activeVehicleId == vehicleIds.first &&
          fleet.snapshot.vehicles.length == 1,
      'non-active close does not disconnect active',
    );
    var remaining = fleet.snapshot.activeVehicle;
    final remainingDeadline = DateTime.now().add(const Duration(seconds: 2));
    while (remaining?.connected != true &&
        DateTime.now().isBefore(remainingDeadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 50));
      fleet.refresh();
      remaining = fleet.snapshot.activeVehicle;
    }
    if (remaining?.connected != true) {
      stdout.writeln(
        'remaining vehicle diagnostic: id=${remaining?.id}, state=${remaining?.state}, summary=${fleet.snapshot.vehicles}',
      );
    }
    check(remaining?.connected == true, 'remaining heartbeat alive');
  } finally {
    for (final timer in timers) {
      timer.cancel();
    }
    for (final sender in senders) {
      sender.close();
    }
    fleet.dispose();
    destroy(core);
  }
  stdout.writeln('PASS: real Dart Fleet seam, native library and teardown');
}
