import 'package:flutter/foundation.dart';

import 'connection_models.dart';
import 'connection_session_backend.dart';
import 'fleet_models.dart';
import 'flight_control_models.dart';

/// Application-level coordinator for multi-vehicle identity and commands.
///
/// The active vehicle is deliberately treated as a temporary routing detail.
/// Batch commands select one vehicle at a time, record its result, and restore
/// the original active vehicle before returning. This keeps the UI independent
/// from MgCore/FFI while preserving per-vehicle failure information.
final class FleetController extends ChangeNotifier {
  FleetController({required this.fleet, required this.flightControl});

  final FleetSessionBackend? fleet;
  final FlightControlSessionBackend? flightControl;

  FleetSnapshot _snapshot = const FleetSnapshot();
  bool _disposed = false;

  FleetSnapshot get snapshot => _snapshot;
  bool get available => fleet != null;

  Future<void> initialize() => refresh();

  Future<void> refresh() async {
    final backend = fleet;
    if (backend == null || _snapshot.busy) return;
    _set(_snapshot.copyWith(busy: true, error: null));
    try {
      final vehicles = await backend.loadVehicles();
      final active = await backend.loadActiveVehicleId();
      final selected = _snapshot.selectedVehicleIds
          .where((id) => vehicles.any((vehicle) => vehicle.vehicleId == id))
          .toSet();
      _set(
        _snapshot.copyWith(
          busy: false,
          vehicles: vehicles,
          activeVehicleId: active,
          selectedVehicleIds: selected,
          error: null,
        ),
      );
    } on Object catch (error) {
      _set(_snapshot.copyWith(busy: false, error: '$error'));
    }
  }

  Future<ConnectionOperationResult> selectVehicle(int vehicleId) async {
    final backend = fleet;
    if (backend == null) return _unavailable('多机能力不可用');
    return _run(() async {
      final result = await backend.selectVehicle(vehicleId);
      if (result.success) {
        _set(
          _snapshot.copyWith(activeVehicleId: vehicleId, lastResult: result),
        );
      }
      return result;
    });
  }

  Future<ConnectionOperationResult> closeVehicle(int vehicleId) async {
    final backend = fleet;
    if (backend == null) return _unavailable('多机能力不可用');
    return _run(() async {
      final result = await backend.closeVehicle(vehicleId);
      if (result.success) {
        final vehicles = _snapshot.vehicles
            .where((vehicle) => vehicle.vehicleId != vehicleId)
            .toList(growable: false);
        final selected = {..._snapshot.selectedVehicleIds}..remove(vehicleId);
        _set(
          _snapshot.copyWith(
            vehicles: vehicles,
            selectedVehicleIds: selected,
            clearActiveVehicleId: _snapshot.activeVehicleId == vehicleId,
            lastResult: result,
          ),
        );
      }
      return result;
    });
  }

  void setSelected(int vehicleId, bool selected) {
    final next = {..._snapshot.selectedVehicleIds};
    if (selected) {
      next.add(vehicleId);
    } else {
      next.remove(vehicleId);
    }
    _set(_snapshot.copyWith(selectedVehicleIds: next));
  }

  void selectAll(bool selected) {
    _set(
      _snapshot.copyWith(
        selectedVehicleIds: selected
            ? _snapshot.vehicles.map((vehicle) => vehicle.vehicleId).toSet()
            : const {},
      ),
    );
  }

  Future<List<FlightModeOption>> loadFlightModes() async {
    final backend = flightControl;
    if (backend == null) return const [];
    return backend.loadFlightModes();
  }

  Future<List<FleetCommandResult>> executeBatch(
    FleetCommandRequest request,
  ) async {
    final fleetBackend = fleet;
    final controlBackend = flightControl;
    if (fleetBackend == null || controlBackend == null) {
      return const [];
    }
    if (_snapshot.busy || _snapshot.selectedVehicleIds.isEmpty) return const [];

    _set(_snapshot.copyWith(busy: true, error: null, lastResults: const []));
    final results = <FleetCommandResult>[];
    String? batchError;
    int? originalActive;
    try {
      originalActive =
          _snapshot.activeVehicleId ?? await fleetBackend.loadActiveVehicleId();
      final targets = _snapshot.selectedVehicleIds.toList()..sort();
      for (final vehicleId in targets) {
        try {
          var result = await fleetBackend.selectVehicle(vehicleId);
          if (result.success) {
            result = await _executeCommand(controlBackend, request);
          }
          results.add(FleetCommandResult(vehicleId: vehicleId, result: result));
        } on Object catch (error) {
          results.add(
            FleetCommandResult(
              vehicleId: vehicleId,
              result: ConnectionOperationResult(
                success: false,
                message: '$error',
              ),
            ),
          );
        }
      }
    } on Object catch (error) {
      batchError = '$error';
    } finally {
      if (originalActive != null) {
        try {
          final restore = await fleetBackend.selectVehicle(originalActive);
          if (!restore.success && batchError == null) {
            batchError = '批量操作完成，但恢复活动飞行器失败：${restore.message}';
          }
        } on Object catch (error) {
          batchError ??= '批量操作完成，但恢复活动飞行器失败：$error';
        }
      }
      _set(
        _snapshot.copyWith(
          busy: false,
          activeVehicleId: originalActive,
          lastResults: results,
          error: batchError,
        ),
      );
    }
    return results;
  }

  Future<ConnectionOperationResult> _executeCommand(
    FlightControlSessionBackend backend,
    FleetCommandRequest request,
  ) {
    return switch (request.kind) {
      FleetCommandKind.arm => backend.arm(),
      FleetCommandKind.disarm => backend.disarm(),
      FleetCommandKind.setMode =>
        request.mode == null
            ? Future.value(
                const ConnectionOperationResult(
                  success: false,
                  message: '批量切换模式缺少目标模式',
                ),
              )
            : backend.setFlightMode(request.mode!),
      FleetCommandKind.rtl => backend.returnToLaunch(),
      FleetCommandKind.land => backend.land(),
    };
  }

  Future<ConnectionOperationResult> _run(
    Future<ConnectionOperationResult> Function() operation,
  ) async {
    if (_snapshot.busy) {
      return const ConnectionOperationResult(success: false, message: '操作进行中');
    }
    _set(_snapshot.copyWith(busy: true, error: null));
    try {
      final result = await operation();
      _set(_snapshot.copyWith(busy: false, lastResult: result));
      return result;
    } on Object catch (error) {
      final result = ConnectionOperationResult(
        success: false,
        message: '$error',
      );
      _set(
        _snapshot.copyWith(busy: false, error: '$error', lastResult: result),
      );
      return result;
    }
  }

  ConnectionOperationResult _unavailable(String message) =>
      ConnectionOperationResult(success: false, message: message);

  void _set(FleetSnapshot next) {
    _snapshot = next;
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }
}

final class FleetSnapshot {
  const FleetSnapshot({
    this.busy = false,
    this.vehicles = const [],
    this.activeVehicleId,
    this.selectedVehicleIds = const {},
    this.lastResults = const [],
    this.lastResult,
    this.error,
  });

  final bool busy;
  final List<FleetVehicleSnapshot> vehicles;
  final int? activeVehicleId;
  final Set<int> selectedVehicleIds;
  final List<FleetCommandResult> lastResults;
  final ConnectionOperationResult? lastResult;
  final String? error;

  FleetSnapshot copyWith({
    bool? busy,
    List<FleetVehicleSnapshot>? vehicles,
    int? activeVehicleId,
    bool clearActiveVehicleId = false,
    Set<int>? selectedVehicleIds,
    List<FleetCommandResult>? lastResults,
    ConnectionOperationResult? lastResult,
    String? error,
  }) => FleetSnapshot(
    busy: busy ?? this.busy,
    vehicles: vehicles ?? this.vehicles,
    activeVehicleId: clearActiveVehicleId
        ? null
        : activeVehicleId ?? this.activeVehicleId,
    selectedVehicleIds: selectedVehicleIds ?? this.selectedVehicleIds,
    lastResults: lastResults ?? this.lastResults,
    lastResult: lastResult ?? this.lastResult,
    error: error,
  );
}
