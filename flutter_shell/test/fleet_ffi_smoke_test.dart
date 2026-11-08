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

const _attitudeFrame = <int>[
  253,
  28,
  0,
  0,
  0,
  1,
  1,
  30,
  0,
  0,
  232,
  3,
  0,
  0,
  0,
  0,
  128,
  62,
  205,
  204,
  204,
  189,
  195,
  245,
  200,
  63,
  10,
  215,
  35,
  60,
  10,
  215,
  163,
  188,
  143,
  194,
  245,
  60,
  16,
  209,
];
const _vfrFrame = <int>[
  253,
  19,
  0,
  0,
  1,
  1,
  1,
  74,
  0,
  0,
  0,
  0,
  64,
  65,
  0,
  0,
  32,
  65,
  0,
  0,
  200,
  66,
  0,
  0,
  192,
  63,
  90,
  0,
  55,
  218,
  201,
];
const _rcFrame = <int>[
  253,
  42,
  0,
  0,
  2,
  1,
  1,
  65,
  0,
  0,
  232,
  3,
  0,
  0,
  220,
  5,
  120,
  5,
  20,
  5,
  176,
  4,
  76,
  4,
  232,
  3,
  132,
  3,
  32,
  3,
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
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  8,
  80,
  98,
  234,
];
const _servoFrame = <int>[
  253,
  37,
  0,
  0,
  3,
  1,
  1,
  36,
  0,
  0,
  64,
  66,
  15,
  0,
  233,
  3,
  234,
  3,
  235,
  3,
  236,
  3,
  237,
  3,
  238,
  3,
  239,
  3,
  240,
  3,
  0,
  241,
  3,
  242,
  3,
  243,
  3,
  244,
  3,
  245,
  3,
  246,
  3,
  247,
  3,
  248,
  3,
  217,
  237,
];

// GLOBAL_POSITION_INT frames generated with the bundled pymavlink dialect.
// System 1: MSL 30 m, relative 0 m. System 2: MSL 42.5 m, relative 12.5 m.
const _globalPositions = <List<int>>[
  [
    253,
    28,
    0,
    0,
    0,
    1,
    1,
    33,
    0,
    0,
    232,
    3,
    0,
    0,
    0,
    163,
    225,
    17,
    0,
    140,
    134,
    71,
    48,
    117,
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
    255,
    255,
    79,
    169,
  ],
  [
    253,
    28,
    0,
    0,
    0,
    2,
    1,
    33,
    0,
    0,
    232,
    3,
    0,
    0,
    0,
    163,
    225,
    17,
    0,
    140,
    134,
    71,
    4,
    166,
    0,
    0,
    212,
    48,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    255,
    255,
    118,
    92,
  ],
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

/// 选一个当前空闲的回环 UDP 端口。
///
/// 不能 `bind(0)` 取号后立刻复用：Dart 的 `RawDatagramSocket.close()` 不保证同步
/// 释放 fd，紧随其后的原生 `socket()/bind()` 会偶发 `EADDRINUSE`（errno 48）。
/// 实测把「取号 → close → 立即重绑」连做 300 次失败 112 次（37%），close 后等
/// 2 ms 再重绑则 0 次。这里统一留出这个窗口，负载下的 flaky 由此消除。
Future<int> _freeLoopbackUdpPort() async {
  final probe = await RawDatagramSocket.bind(InternetAddress.loopbackIPv4, 0);
  final port = probe.port;
  probe.close();
  await Future<void>.delayed(const Duration(milliseconds: 5));
  return port;
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
        final port = await _freeLoopbackUdpPort();

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
          void heartbeat() {
            sender.send(_heartbeats[index], InternetAddress.loopbackIPv4, port);
            sender.send(
              _globalPositions[index],
              InternetAddress.loopbackIPv4,
              port,
            );
          }

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
          if (ids.containsAll({1, 2}) &&
              rawFleet.snapshot.vehicles.every((v) => v.positionValid)) {
            break;
          }
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }

        final vehicles = rawFleet.snapshot.vehicles;
        expect(vehicles.length, 2);
        expect(vehicles.map((vehicle) => vehicle.systemId), {1, 2});
        expect(vehicles.every((vehicle) => vehicle.connected), isTrue);
        final first = vehicles.singleWhere((v) => v.systemId == 1);
        final second = vehicles.singleWhere((v) => v.systemId == 2);
        expect(first.altitudeMeters, 30);
        expect(first.relativeAltitudeMeters, 0);
        expect(second.altitudeMeters, 42.5);
        expect(second.relativeAltitudeMeters, 12.5);
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
        final altitudeDeadline = DateTime.now().add(const Duration(seconds: 3));
        while (controller.relativeAltitudeMeters !=
                other.relativeAltitudeMeters &&
            DateTime.now().isBefore(altitudeDeadline)) {
          await Future<void>.delayed(const Duration(milliseconds: 20));
        }
        expect(controller.relativeAltitudeMeters, other.relativeAltitudeMeters);

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
          final port = await _freeLoopbackUdpPort();
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
        controller.vehiclePosition = const VehicleFix(
          latitude: 30,
          longitude: 120,
        );
        controller.vehiclePositionValid = true;
        controller.headingDegrees = null;
        await expectLater(
          controller.guidedSetHeading(30.001, 120.001),
          throwsA(
            isA<StateError>().having(
              (error) => error.message,
              'message',
              '设置航向失败：当前航向未知',
            ),
          ),
        );
        controller.waveform.imuPointCount = 17;
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
        expect(controller.waveform.imuPointCount, 0);
        expect(controller.flightTimeSeconds, 0);
        expect(controller.flightDistanceMeters, 0);
        controller.waveform.imuPointCount = 23;
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
        expect(controller.waveform.imuPointCount, 0);
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
          FleetBatchAction.returnToLaunch,
        );
        final batchResult = await rawFleet.runBatch(batchTarget);
        // The loopback sender never answers COMMAND_ACK, so each member must
        // report a timeout rather than a false success.
        expect(batchResult.allSucceeded, isFalse);
        expect(batchResult.results.every((item) => !item.success), isTrue);
        expect(batchResult.results.map((item) => item.vehicleId), vehicleIds);
        expect(rawFleet.snapshot.activeVehicleId, activeBeforeBatch);

        // 启动任务的执行器必须逐机校验任务回读：即使确认目标被构造出来，
        // 没有回读到任务的成员也只能是"未派发"，不能收到 MISSION_START。
        final missionTarget = FleetBatchTarget(
          action: FleetBatchAction.startMission,
          vehicleIds: vehicleIds,
          systemIds: batchTarget.systemIds,
        );
        final missionResult = await rawFleet.runBatch(missionTarget);
        expect(
          missionResult.results.every(
            (item) => item.status == FleetBatchCommandStatus.notSent,
          ),
          isTrue,
        );
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
    'dashboard aggregation updates attitude, VFR, RC and servo telemetry',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final rawFleet = controller.fleet;
      RawDatagramSocket? sender;
      Timer? telemetryTimer;
      try {
        final port = await _freeLoopbackUdpPort();
        controller.selectTransport(ConnectionTransport.udp);
        controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
        await controller.connect();
        sender = await RawDatagramSocket.bind(InternetAddress.loopbackIPv4, 0);
        final socket = sender;
        final flightDataBefore = controller.flightDataChanges.value;
        final attitudeBefore = controller.attitudeChanges.value;
        final ioBefore = controller.ioChanges.value;
        void sendTelemetry() {
          socket.send(_heartbeats[0], InternetAddress.loopbackIPv4, port);
          socket.send(_attitudeFrame, InternetAddress.loopbackIPv4, port);
          socket.send(_vfrFrame, InternetAddress.loopbackIPv4, port);
          socket.send(_rcFrame, InternetAddress.loopbackIPv4, port);
          socket.send(_servoFrame, InternetAddress.loopbackIPv4, port);
        }

        sendTelemetry();
        telemetryTimer = Timer.periodic(
          const Duration(milliseconds: 100),
          (_) => sendTelemetry(),
        );

        final deadline = DateTime.now().add(const Duration(seconds: 4));
        while (DateTime.now().isBefore(deadline)) {
          rawFleet.refresh();
          if (rawFleet.snapshot.vehicles.isNotEmpty &&
              controller.rollDegrees != null &&
              controller.rcChannelsValid &&
              controller.servoOutputsValid) {
            break;
          }
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }

        expect(controller.rollDegrees, closeTo(14.3239, 0.1));
        expect(controller.pitchDegrees, closeTo(-5.7296, 0.1));
        expect(controller.airspeedMetersPerSecond, closeTo(12.0, 0.01));
        expect(controller.groundSpeedMetersPerSecond, closeTo(10.0, 0.01));
        expect(controller.rcChannelsValid, isTrue);
        expect(controller.servoOutputsValid, isTrue);
        expect(controller.rcChannels.take(8), [
          1500,
          1400,
          1300,
          1200,
          1100,
          1000,
          900,
          800,
        ]);
        expect(controller.servoOutputs.take(8), [
          1001,
          1002,
          1003,
          1004,
          1005,
          1006,
          1007,
          1008,
        ]);
        expect(
          controller.flightDataChanges.value,
          greaterThan(flightDataBefore),
        );
        expect(controller.attitudeChanges.value, greaterThan(attitudeBefore));
        expect(controller.ioChanges.value, greaterThan(ioBefore));
      } finally {
        telemetryTimer?.cancel();
        sender?.close();
      }
    },
    skip: libraryPath == null
        ? 'Set MGCS_FLEET_LIBRARY for actual native smoke'
        : false,
  );
  test(
    'duplicate sysids stay isolated and cannot enter a batch',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final rawFleet = controller.fleet;
      final senders = <RawDatagramSocket>[];
      final timers = <Timer>[];
      try {
        for (var index = 0; index < 2; index++) {
          final port = await _freeLoopbackUdpPort();
          controller.selectTransport(ConnectionTransport.udp);
          controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
          await controller.connect();
          final sender = await RawDatagramSocket.bind(
            InternetAddress.loopbackIPv4,
            0,
          );
          senders.add(sender);
          void heartbeat() =>
              sender.send(_heartbeats[0], InternetAddress.loopbackIPv4, port);
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
          if (rawFleet.snapshot.vehicles.length == 2 &&
              rawFleet.snapshot.links.every((link) => link.connected)) {
            break;
          }
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }

        expect(rawFleet.snapshot.vehicles.length, 2);
        expect(rawFleet.snapshot.links.length, 2);
        expect(rawFleet.snapshot.vehicles.map((vehicle) => vehicle.systemId), [
          1,
          1,
        ]);
        expect(
          rawFleet.snapshot.vehicles
              .map((vehicle) => vehicle.id)
              .toSet()
              .length,
          2,
        );
        expect(
          rawFleet.snapshot.links.map((link) => link.vehicleId).toSet().length,
          2,
        );
        expect(
          rawFleet.snapshot.links.map((link) => link.endpoint).toSet().length,
          2,
        );
        for (final vehicle in rawFleet.snapshot.vehicles) {
          rawFleet.selectVehicle(vehicle.id);
          expect(rawFleet.captureTarget().vehicleId, vehicle.id);
        }
        rawFleet.selectAllVehicles();
        expect(
          () => rawFleet.captureBatchTarget(FleetBatchAction.arm),
          throwsStateError,
        );
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
    'active calibration blocks link-list focus switching',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final rawFleet = controller.fleet;
      final senders = <RawDatagramSocket>[];
      final timers = <Timer>[];
      try {
        for (var index = 0; index < 2; index++) {
          final port = await _freeLoopbackUdpPort();
          controller.selectTransport(ConnectionTransport.udp);
          controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
          await controller.connect();
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
        }

        final deadline = DateTime.now().add(const Duration(seconds: 4));
        while (DateTime.now().isBefore(deadline)) {
          rawFleet.refresh();
          if (rawFleet.snapshot.vehicles.length == 2 &&
              rawFleet.snapshot.vehicles.every(
                (vehicle) => vehicle.connected,
              )) {
            break;
          }
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        final vehicleIds = rawFleet.snapshot.vehicles
            .map((vehicle) => vehicle.id)
            .toList(growable: false);
        expect(vehicleIds.length, 2);
        expect(controller.sessionId, vehicleIds.first);

        await controller.startCalibration(CalibrationType.magnetometer);
        expect(controller.calibrationStatus.active, isTrue);

        final otherLink = rawFleet.snapshot.links.firstWhere(
          (link) => link.vehicleId == vehicleIds.last,
        );
        controller.selectLink(otherLink.id);

        expect(controller.sessionId, vehicleIds.first);
        expect(controller.calibrationStatus.active, isTrue);
        expect(controller.message, contains('未完成操作'));
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
    final port = await _freeLoopbackUdpPort();
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
      final port = await _freeLoopbackUdpPort();
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
