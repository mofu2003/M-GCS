import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

// Independent loopback autopilot: no SITL, serial device or user port is used.
List<int> _frame(int id, List<int> payload, int extra, {int systemId = 1}) {
  var length = payload.length;
  while (length > 1 && payload[length - 1] == 0) {
    length--;
  }
  final bytes = [
    253,
    length,
    0,
    0,
    0,
    systemId,
    1,
    id & 255,
    (id >> 8) & 255,
    0,
    ...payload.take(length),
  ];
  var crc = 0xffff;
  for (final byte in [...bytes.skip(1), extra]) {
    var tmp = byte ^ (crc & 255);
    tmp ^= (tmp << 4) & 255;
    crc = ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xffff;
  }
  return [...bytes, crc & 255, crc >> 8];
}

Future<void> _waitFor(bool Function() ready, [int seconds = 8]) async {
  final end = DateTime.now().add(Duration(seconds: seconds));
  while (!ready() && DateTime.now().isBefore(end)) {
    await Future<void>.delayed(const Duration(milliseconds: 50));
  }
  expect(ready(), isTrue);
}

void main() {
  final library = Platform.environment['MGCS_FLEET_LIBRARY'];
  test(
    'inactive aircraft positions update at telemetry cadence without switching focus',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final probe = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      final port = probe.port;
      probe.close();
      controller.selectTransport(ConnectionTransport.udp);
      controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
      await controller.connect();
      final sender = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      addTearDown(sender.close);
      var tick = 0;
      void transmit() {
        tick++;
        for (final systemId in [1, 4]) {
          sender.send(
            _frame(0, [0, 0, 0, 0, 2, 3, 1, 3, 3], 50, systemId: systemId),
            InternetAddress.loopbackIPv4,
            port,
          );
          final position = ByteData(28)
            ..setInt32(4, 325000000 + tick * 100, Endian.little)
            ..setInt32(8, 1147000000 + systemId * 100, Endian.little)
            ..setInt32(12, 30000, Endian.little)
            ..setUint16(26, tick * 100, Endian.little);
          sender.send(
            _frame(33, position.buffer.asUint8List(), 104, systemId: systemId),
            InternetAddress.loopbackIPv4,
            port,
          );
        }
      }

      transmit();
      final timer = Timer.periodic(
        const Duration(milliseconds: 50),
        (_) => transmit(),
      );
      addTearDown(timer.cancel);
      await _waitFor(
        () =>
            controller.fleetSnapshot.vehicles.length == 2 &&
            controller.isConnected,
      );
      final active = controller.sessionId;
      final other = controller.fleetSnapshot.vehicles
          .firstWhere((v) => v.id != active)
          .id;
      final revision = controller.fleet.focusRevision;
      final positions = <double>{};
      final headings = <double?>{};
      void sample() {
        final fix = controller.fleetPositions.value[other];
        if (fix != null) {
          positions.add(fix.latitude);
          headings.add(fix.headingDegrees);
        }
      }

      controller.fleetPositions.addListener(sample);
      await Future<void>.delayed(const Duration(milliseconds: 650));
      controller.fleetPositions.removeListener(sample);
      expect(positions.length, greaterThanOrEqualTo(6));
      expect(headings.length, greaterThanOrEqualTo(6));
      expect(controller.sessionId, active);
      expect(controller.fleet.focusRevision, revision);
      timer.cancel();
      await controller.disconnect();
    },
    skip: library == null
        ? 'Set MGCS_FLEET_LIBRARY for native connection test'
        : false,
  );
  test(
    'missing Home stream is requested again for its vehicle',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final probe = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      final port = probe.port;
      probe.close();
      controller.selectTransport(ConnectionTransport.udp);
      controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
      await controller.connect();
      final sender = await RawDatagramSocket.bind(
        InternetAddress.loopbackIPv4,
        0,
      );
      addTearDown(sender.close);
      var homeStreams = 0;
      final subscription = sender.listen((event) {
        if (event != RawSocketEvent.read) return;
        Datagram? packet;
        while ((packet = sender.receive()) != null) {
          final bytes = packet!.data;
          if (bytes.length < 12) continue;
          final v2 = bytes[0] == 253;
          final id = v2 ? bytes[7] | bytes[8] << 8 | bytes[9] << 16 : bytes[5];
          final offset = v2 ? 10 : 6;
          if (id != 76 || bytes.length < offset + 32) continue;
          final data = ByteData.sublistView(bytes, offset);
          if (data.getUint16(28, Endian.little) == 511 &&
              data.getFloat32(0, Endian.little) == 242) {
            expect(bytes[offset + 30], 1);
            homeStreams++;
          }
        }
      });
      addTearDown(subscription.cancel);
      void heartbeat() => sender.send(
        _frame(0, [0, 0, 0, 0, 1, 3, 1, 3, 3], 50),
        InternetAddress.loopbackIPv4,
        port,
      );
      heartbeat();
      final timer = Timer.periodic(
        const Duration(milliseconds: 100),
        (_) => heartbeat(),
      );
      addTearDown(timer.cancel);
      await _waitFor(() => homeStreams >= 2, 13);
      expect(controller.fleetSnapshot.vehicles.single.systemId, 1);
      timer.cancel();
      await controller.disconnect();
    },
    timeout: const Timeout(Duration(seconds: 20)),
    skip: library == null
        ? 'Set MGCS_FLEET_LIBRARY for native connection test'
        : false,
  );
  for (final scenario in ['route', 'armed_route', 'empty', 'no_parameters']) {
    final empty = scenario == 'empty';
    final noParameters = scenario == 'no_parameters';
    test(
      'connection automatically imports $scenario vehicle route into map document',
      () async {
        final controller = ConnectionController();
        addTearDown(controller.dispose);
        final probe = await RawDatagramSocket.bind(
          InternetAddress.loopbackIPv4,
          0,
        );
        final port = probe.port;
        probe.close();
        controller.selectTransport(ConnectionTransport.udp);
        controller.updateNetworkEndpoint(host: '127.0.0.1', port: '$port');
        await controller.connect();
        final sender = await RawDatagramSocket.bind(
          InternetAddress.loopbackIPv4,
          0,
        );
        var listRequests = 0;
        void send(List<int> bytes) =>
            sender.send(bytes, InternetAddress.loopbackIPv4, port);
        final subscription = sender.listen((event) {
          if (event != RawSocketEvent.read) return;
          Datagram? packet;
          while ((packet = sender.receive()) != null) {
            final bytes = packet!.data;
            if (bytes.length < 8) continue;
            final v2 = bytes[0] == 253;
            final id = v2
                ? bytes[7] | bytes[8] << 8 | bytes[9] << 16
                : bytes[5];
            final payloadOffset = v2 ? 10 : 6;
            final sourceSystem = bytes[v2 ? 5 : 3];
            final sourceComponent = bytes[v2 ? 6 : 4];
            if (id == 21 && !noParameters) {
              final param = ByteData(25)
                ..setFloat32(0, 1, Endian.little)
                ..setUint16(4, 1, Endian.little);
              param.buffer.asUint8List().setRange(8, 16, 'TEST_VAL'.codeUnits);
              param.setUint8(24, 9);
              send(_frame(22, param.buffer.asUint8List(), 220));
            } else if (id == 43) {
              listRequests++;
              final count = _frame(44, [
                empty ? 0 : 2,
                0,
                sourceSystem,
                sourceComponent,
                0,
              ], 221);
              send(count);
            } else if (id == 51 || id == 40) {
              final seq = bytes[payloadOffset] | bytes[payloadOffset + 1] << 8;
              final item = ByteData(38)
                ..setInt32(16, 325000000 + seq * 1000, Endian.little)
                ..setInt32(20, 1147000000 + seq * 1000, Endian.little)
                ..setFloat32(24, seq == 0 ? 30 : 50, Endian.little)
                ..setUint16(28, seq, Endian.little)
                ..setUint16(30, 16, Endian.little)
                ..setUint8(32, sourceSystem)
                ..setUint8(33, sourceComponent)
                ..setUint8(34, seq == 0 ? 0 : 3)
                ..setUint8(36, 1);
              send(_frame(73, item.buffer.asUint8List(), 38));
            }
          }
        });
        void heartbeat() => send(
          _frame(0, [
            0,
            0,
            0,
            0,
            2,
            3,
            scenario == 'armed_route' ? 129 : 1,
            3,
            3,
          ], 50),
        );
        heartbeat();
        final timer = Timer.periodic(
          const Duration(milliseconds: 100),
          (_) => heartbeat(),
        );
        try {
          await _waitFor(() => controller.isConnected);
          if (!noParameters) await _waitFor(() => controller.parametersLoaded);
          await _waitFor(
            () =>
                controller.missionPlanning.initialized &&
                controller.missionPlanning.document != null &&
                controller.missionPlanning.status?.startsWith('已从飞控读取') == true,
            noParameters ? 35 : 8,
          );
          if (noParameters) expect(controller.parametersLoaded, isFalse);
          expect(listRequests, 1);
          final document = controller.missionPlanning.document!;
          expect(document.entities.length, empty ? 0 : 1);
          if (!empty) {
            expect(
              document.entities.single.latitude,
              closeTo(32.5001, 0.000001),
            );
            expect(
              document.entities.single.longitude,
              closeTo(114.7001, 0.000001),
            );
          }
          expect(controller.missionPlanning.dirty, isFalse);
          await Future<void>.delayed(const Duration(seconds: 1));
          expect(listRequests, 1);
        } finally {
          timer.cancel();
          await subscription.cancel();
          sender.close();
          await controller.disconnect();
        }
      },
      timeout: const Timeout(Duration(seconds: 45)),
      skip: library == null
          ? 'Set MGCS_FLEET_LIBRARY for native connection test'
          : false,
    );
  }
}
