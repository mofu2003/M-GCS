import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

import 'fleet_controller_test.dart' show MemoryFleetBackend;

void main() {
  test('progress is immutable and stop skips unsent members', () async {
    final backend = MemoryFleetBackend();
    final ack = Completer<FleetBatchCommandResult>();
    final calls = <int>[];
    final fleet = FleetController(
      backend,
      executeBatchCommand: (v, a) {
        calls.add(v.id);
        return ack.future;
      },
    );
    addTearDown(fleet.dispose);
    final first = fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14550'));
    fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14551'));
    fleet.selectAllVehicles();
    final target = fleet.captureBatchTarget(FleetBatchAction.arm);
    final pending = fleet.runBatch(target);
    final initial = fleet.batchProgress!;
    expect(initial.runningVehicleId, first);
    expect(initial.running, isTrue);
    expect(initial.results, isEmpty);
    await expectLater(fleet.runBatch(target), throwsStateError);
    fleet.stopBatchDispatch();
    expect(fleet.batchProgress!.stopRequested, isTrue);
    ack.complete(
      FleetBatchCommandResult.success(vehicleId: first, systemId: 1),
    );
    final result = await pending;
    expect(calls, [first]);
    expect(result.results.last.status, FleetBatchCommandStatus.notSent);
    expect(result.successCount, 1);
    expect(result.notSentCount, 1);
    expect(result.failureCount, 0);
    expect(result.allSucceeded, isFalse);
    expect(fleet.batchProgress!.running, isFalse);
    expect(fleet.batchProgress!.results.length, 2);
    expect(initial.results, isEmpty);
    expect(() => fleet.batchProgress!.results.clear(), throwsUnsupportedError);
  });

  test('next member identity is checked after previous ACK', () async {
    final backend = MemoryFleetBackend();
    final ack = Completer<FleetBatchCommandResult>();
    final calls = <int>[];
    final fleet = FleetController(
      backend,
      executeBatchCommand: (v, a) {
        calls.add(v.id);
        return ack.future;
      },
    );
    addTearDown(fleet.dispose);
    final first = fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14550'));
    final second = fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14551'));
    fleet.selectAllVehicles();
    final pending = fleet.runBatch(
      fleet.captureBatchTarget(FleetBatchAction.arm),
    );
    backend.vehicles[second] = FleetVehicleSummary(
      id: second,
      systemId: 7,
      endpoint: 'udp://0.0.0.0:14551',
      connected: true,
    );
    ack.complete(
      FleetBatchCommandResult.success(vehicleId: first, systemId: 1),
    );
    final result = await pending;
    expect(calls, [first]);
    expect(result.results.last.status, FleetBatchCommandStatus.error);
    expect(fleet.batchProgress!.running, isFalse);
  });

  test(
    'dispose stops remaining dispatch while preserving in-flight ACK',
    () async {
      final backend = MemoryFleetBackend();
      final ack = Completer<FleetBatchCommandResult>();
      final calls = <int>[];
      final fleet = FleetController(
        backend,
        executeBatchCommand: (v, a) {
          calls.add(v.id);
          return ack.future;
        },
      );
      final first = fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14550'));
      fleet.openLink(FleetConnectionSpec('udp://0.0.0.0:14551'));
      fleet.selectAllVehicles();
      final pending = fleet.runBatch(
        fleet.captureBatchTarget(FleetBatchAction.arm),
      );
      fleet.dispose();
      ack.complete(
        FleetBatchCommandResult.success(vehicleId: first, systemId: 1),
      );
      final result = await pending;
      expect(calls, [first]);
      expect(result.results.last.status, FleetBatchCommandStatus.notSent);
    },
  );
}
