import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/fleet/fleet_backend.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

class MemoryFleetBackend implements FleetBackend {
  final vehicles = <int, FleetVehicleSummary>{};
  final links = <int, FleetLinkSummary>{};
  int? active;
  int nextId = 1;
  bool failOpen = false;
  bool failSelect = false;
  final closed = <int>[];

  @override
  int open(FleetConnectionSpec spec) {
    if (failOpen) throw StateError('open failed');
    final id = nextId++;
    links[id] = FleetLinkSummary(
      id: id,
      routeId: id,
      vehicleId: id,
      systemId: id,
      endpoint: spec.uri,
      connected: true,
      primary: true,
    );
    vehicles[id] = FleetVehicleSummary(
      id: id,
      systemId: id,
      endpoint: spec.uri,
      connected: true,
    );
    return id;
  }

  @override
  void close(int id) {
    closed.add(id);
    final link = links.remove(id);
    final vehicleId = link?.vehicleId ?? id;
    final hasOtherRoute = links.values.any(
      (candidate) => candidate.vehicleId == vehicleId,
    );
    if (!hasOtherRoute) vehicles.remove(vehicleId);
    if (active == vehicleId) active = vehicles.keys.firstOrNull;
  }

  @override
  void closeRoute(int routeId) {
    closed.add(routeId);
    int? linkId;
    for (final entry in links.entries) {
      if (entry.value.routeId == routeId) {
        linkId = entry.key;
        break;
      }
    }
    if (linkId == null) return;
    final link = links.remove(linkId);
    final vehicleId = link?.vehicleId;
    if (vehicleId != null &&
        !links.values.any((candidate) => candidate.vehicleId == vehicleId)) {
      vehicles.remove(vehicleId);
      if (active == vehicleId) active = vehicles.keys.firstOrNull;
    }
  }

  @override
  void select(int id) {
    if (failSelect) throw StateError('select failed');
    active = id;
  }

  @override
  FleetSnapshot snapshot() => FleetSnapshot(
    vehicles: vehicles.values,
    links: links.values,
    activeVehicleId: active,
  );
}

void main() {
  late MemoryFleetBackend backend;
  late FleetController fleet;
  setUp(() {
    backend = MemoryFleetBackend();
    fleet = FleetController(backend);
  });
  tearDown(() => fleet.dispose());

  int open(int port) =>
      fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:$port'));

  test('link handle remains distinct from vehicle identity', () {
    open(14550);
    open(14551);
    backend.links[3] = const FleetLinkSummary(
      id: 3,
      routeId: 3,
      vehicleId: 2,
      systemId: 2,
      endpoint: 'udp://127.0.0.1:14552',
      connected: true,
    );
    fleet.refresh();
    expect(fleet.snapshot.link(3)?.vehicleId, 2);
    fleet.selectLink(3);
    expect(fleet.snapshot.activeVehicleId, 2);
    fleet.closeLink(3);
    expect(fleet.snapshot.vehicle(2), isNotNull);
    expect(fleet.snapshot.link(3), isNull);
  });

  test('first session is selected, second does not steal focus', () {
    final a = open(14550);
    final b = open(14551);
    expect(fleet.snapshot.activeVehicleId, a);
    expect(fleet.snapshot.vehicles.map((v) => v.id), [a, b]);
  });

  test('duplicate endpoint never opens another native socket', () {
    open(14550);
    expect(() => open(14550), throwsStateError);
    expect(backend.nextId, 2);
  });

  test('same serial device with a different baud is still exclusive', () {
    fleet.openLink(FleetConnectionSpec('serial:///dev/cu.test?baud=115200'));
    expect(
      () => fleet.openLink(
        FleetConnectionSpec('serial:///dev/cu.test?baud=57600'),
      ),
      throwsStateError,
    );
    expect(backend.nextId, 2);
  });

  test(
    'native focus not yet bound to UI cannot become confirmation target',
    () {
      final a = open(14550);
      final b = open(14551);
      fleet.selectVehicle(b);
      expect(() => fleet.captureTarget(expectedVehicleId: a), throwsStateError);
    },
  );

  test('closing a non-active link does not affect active', () {
    final a = open(14550);
    final b = open(14551);
    fleet.closeLink(b);
    expect(fleet.snapshot.activeVehicleId, a);
    expect(backend.closed, [b]);
  });

  test('closed active selects replacement and invalidates pending target', () {
    final a = open(14550);
    final target = fleet.captureTarget();
    final b = open(14551);
    fleet.closeLink(a);
    expect(fleet.snapshot.activeVehicleId, b);
    expect(() => fleet.validateTarget(target), throwsStateError);
  });

  test('A to B to A cannot resurrect an old confirmation', () {
    final a = open(14550);
    final b = open(14551);
    final oldTarget = fleet.captureTarget();
    fleet.selectVehicle(b);
    fleet.selectVehicle(a);
    expect(() => fleet.validateTarget(oldTarget), throwsStateError);
    expect(() => fleet.validateTarget(fleet.captureTarget()), returnsNormally);
  });

  test('new link and list refresh do not invalidate current target', () {
    open(14550);
    final target = fleet.captureTarget();
    open(14551);
    fleet.refresh();
    expect(() => fleet.validateTarget(target), returnsNormally);
  });

  test('async command pins focus and rejects switch or close', () async {
    final a = open(14550);
    final b = open(14551);
    final gate = Completer<void>();
    final command = fleet.runForTarget(
      fleet.captureTarget(),
      () => gate.future,
    );
    expect(fleet.targetOperationActive, isTrue);
    expect(() => fleet.selectVehicle(b), throwsStateError);
    expect(() => fleet.closeLink(a), throwsStateError);
    gate.complete();
    await command;
    expect(fleet.targetOperationActive, isFalse);
    fleet.selectVehicle(b);
  });

  test(
    'external loss invalidates operation instead of retargeting it',
    () async {
      final a = open(14550);
      open(14551);
      final gate = Completer<void>();
      final command = fleet.runForTarget(
        fleet.captureTarget(),
        () => gate.future,
      );
      final result = expectLater(command, throwsStateError);
      backend.close(a);
      fleet.refresh();
      gate.complete();
      await result;
      expect(fleet.targetOperationActive, isFalse);
    },
  );

  test('command error releases pin and preserves backend error', () async {
    open(14550);
    await expectLater(
      fleet.runForTarget(
        fleet.captureTarget(),
        () async => throw StateError('denied'),
      ),
      throwsA(isA<StateError>().having((e) => e.message, 'error', 'denied')),
    );
    expect(fleet.targetOperationActive, isFalse);
  });

  test('failed open does not discard existing connections', () {
    final a = open(14550);
    backend.failOpen = true;
    expect(() => open(14551), throwsStateError);
    expect(fleet.snapshot.activeVehicleId, a);
    expect(backend.closed, isEmpty);
  });

  test('failed first selection cleans up only the new link', () {
    backend.failSelect = true;
    expect(() => open(14550), throwsStateError);
    expect(backend.closed, [1]);
    expect(fleet.snapshot.vehicles, isEmpty);
  });

  test(
    'snapshot list is immutable and waiting session cannot be commanded',
    () {
      backend.vehicles[1] = const FleetVehicleSummary(
        id: 1,
        systemId: 0,
        endpoint: 'udp://0.0.0.0:14550',
        connected: false,
      );
      backend.active = 1;
      fleet.refresh();
      expect(() => fleet.snapshot.vehicles.clear(), throwsUnsupportedError);
      expect(() => fleet.captureTarget(), throwsStateError);
    },
  );

  test(
    'concurrent same-target operations release the pin out of order',
    () async {
      final a = open(14550);
      final b = open(14551);
      final first = Completer<void>(), second = Completer<void>();
      final target = fleet.captureTarget();
      final op1 = fleet.runForTarget(target, () => first.future);
      final op2 = fleet.runForTarget(target, () => second.future);
      first.complete();
      await op1;
      expect(fleet.targetOperationActive, isTrue);
      expect(() => fleet.selectVehicle(b), throwsStateError);
      second.complete();
      await op2;
      fleet.selectVehicle(b);
      expect(fleet.snapshot.activeVehicleId, isNot(a));
      expect(() => fleet.validatePinnedTarget(), returnsNormally);
    },
  );

  test('same session loss and reconnect cannot revive old confirmation', () {
    final id = open(14550);
    final old = fleet.captureTarget();
    final original = backend.vehicles[id]!;
    backend.vehicles[id] = FleetVehicleSummary(
      id: id,
      systemId: id,
      endpoint: original.endpoint,
      connected: false,
    );
    fleet.refresh();
    backend.vehicles[id] = original;
    fleet.refresh();
    expect(() => fleet.validateTarget(old), throwsStateError);
  });

  test(
    'same session with replaced system identity invalidates confirmation',
    () {
      final id = open(14550);
      final old = fleet.captureTarget();
      backend.vehicles[id] = FleetVehicleSummary(
        id: id,
        systemId: 22,
        endpoint: backend.vehicles[id]!.endpoint,
        connected: true,
      );
      fleet.refresh();
      expect(() => fleet.validateTarget(old), throwsStateError);
    },
  );

  test(
    'host gate blocks premature UI switching without changing native focus',
    () {
      var allowed = true;
      final guarded = FleetController(backend, canChangeFocus: () => allowed);
      final a = guarded.openLink(FleetConnectionSpec('udp://0.0.0.0:14550'));
      final b = guarded.openLink(FleetConnectionSpec('udp://0.0.0.0:14551'));
      allowed = false;
      expect(() => guarded.selectVehicle(b), throwsStateError);
      expect(backend.active, a);
      guarded.dispose();
    },
  );

  test('invalid IDs and invalid connection addresses are rejected', () {
    expect(() => fleet.selectVehicle(99), throwsStateError);
    expect(
      () => FleetConnectionSpec('http://localhost:80'),
      throwsArgumentError,
    );
    expect(() => FleetConnectionSpec('udp://localhost:0'), throwsArgumentError);
    expect(() => FleetConnectionSpec('tcp://localhost'), throwsArgumentError);
    expect(
      () => FleetConnectionSpec('serial:///?baud=115200'),
      throwsArgumentError,
    );
    expect(
      () => FleetConnectionSpec('serial:///dev/cu.test?baud=-1'),
      throwsArgumentError,
    );
    expect(
      FleetConnectionSpec('serial:///dev/cu.test?baud=115200').uri,
      'serial:///dev/cu.test?baud=115200',
    );
    expect(
      FleetConnectionSpec('serial://COM3?baud=115200').uri,
      'serial://COM3?baud=115200',
    );
  });
}
