import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/presentation/controllers/fleet_controller.dart';
import 'package:mgcs/application/fleet_models.dart';
import 'package:mgcs/application/flight_control_models.dart';

void main() {
  test('loads vehicles and keeps selection across refresh', () async {
    final fleet = _FakeFleet();
    final controller = FleetController(
      fleet: fleet,
      flightControl: _FakeControl(),
    );
    addTearDown(controller.dispose);

    await controller.initialize();
    controller.setSelected(2, true);
    fleet.vehicles = [fleet.vehicles.last];
    await controller.refresh();

    expect(controller.snapshot.vehicles.single.vehicleId, 2);
    expect(controller.snapshot.selectedVehicleIds, {2});
    expect(controller.snapshot.activeVehicleId, 1);
  });

  test(
    'batch command returns per-vehicle results and restores active vehicle',
    () async {
      final fleet = _FakeFleet();
      final control = _FakeControl();
      fleet.onSelect = (id) => control.activeVehicleId = id;
      final controller = FleetController(fleet: fleet, flightControl: control);
      addTearDown(controller.dispose);

      await controller.initialize();
      controller.selectAll(true);
      final results = await controller.executeBatch(
        const FleetCommandRequest(kind: FleetCommandKind.rtl),
      );

      expect(results.map((result) => result.vehicleId), [1, 2]);
      expect(results.map((result) => result.success), [true, false]);
      expect(fleet.selectedIds, [1, 2, 1]);
      expect(controller.snapshot.activeVehicleId, 1);
      expect(controller.snapshot.busy, isFalse);
    },
  );
}

final class _FakeFleet implements FleetSessionBackend {
  List<FleetVehicleSnapshot> vehicles = const [
    FleetVehicleSnapshot(
      vehicleId: 1,
      systemId: 1,
      name: 'Alpha',
      endpoint: 'udp://a',
      firmwareType: 'ArduPilot',
      vehicleType: 'Copter',
      connected: true,
    ),
    FleetVehicleSnapshot(
      vehicleId: 2,
      systemId: 2,
      name: 'Bravo',
      endpoint: 'udp://b',
      firmwareType: 'ArduPilot',
      vehicleType: 'Copter',
      connected: true,
    ),
  ];
  final selectedIds = <int>[];
  int active = 1;
  void Function(int id)? onSelect;

  @override
  Future<List<FleetVehicleSnapshot>> loadVehicles() async => vehicles;

  @override
  Future<int?> loadActiveVehicleId() async => active;

  @override
  Future<ConnectionOperationResult> selectVehicle(int vehicleId) async {
    selectedIds.add(vehicleId);
    active = vehicleId;
    onSelect?.call(vehicleId);
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionOperationResult> closeVehicle(int vehicleId) async =>
      const ConnectionOperationResult(success: true, message: 'ok');
}

final class _FakeControl implements FlightControlSessionBackend {
  int? activeVehicleId;
  @override
  Future<ConnectionOperationResult> arm({bool force = false}) async =>
      _result();

  @override
  Future<ConnectionOperationResult> disarm({bool force = false}) async =>
      _result();

  @override
  Future<ConnectionOperationResult> setFlightMode(String mode) async =>
      _result();

  @override
  Future<ConnectionOperationResult> returnToLaunch() async =>
      activeVehicleId == 2
      ? const ConnectionOperationResult(
          success: false,
          message: 'vehicle 2 rejected',
        )
      : _result();

  @override
  Future<ConnectionOperationResult> takeoff(double altitudeMeters) async =>
      _result();

  @override
  Future<ConnectionOperationResult> land() async => _result();

  @override
  Future<ConnectionOperationResult> emergencyStop() async => _result();

  @override
  Future<ConnectionOperationResult> guidedPoint(
    GuidedPointRequest request,
  ) async => _result();

  @override
  Future<PreflightCheckResult> preflightCheck() async =>
      const PreflightCheckResult(ready: true, message: 'ok', items: []);

  @override
  Future<List<FlightModeOption>> loadFlightModes() async => const [
    FlightModeOption(name: 'Guided'),
    FlightModeOption(name: 'Auto'),
  ];

  @override
  Future<FlightModeStatus> loadFlightModeStatus() async =>
      const FlightModeStatus(currentMode: 'Stabilize', armed: false);

  ConnectionOperationResult _result() =>
      const ConnectionOperationResult(success: true, message: 'ok');
}
