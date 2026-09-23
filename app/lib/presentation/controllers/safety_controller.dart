import 'package:flutter/foundation.dart';

import '../../application/connection_models.dart';
import '../../application/connection_session_backend.dart';
import '../../application/mission_models.dart';
import '../../application/safety_models.dart';

/// Application-level orchestration for geofence and rally workflows.
///
/// The controller deliberately exposes snapshots and semantic operations only;
/// pages do not need to know whether the backend is MgCore, demo, or another
/// frontend adapter.
final class SafetyController extends ChangeNotifier {
  SafetyController({required this.geofence, required this.rally});

  final GeofenceSessionBackend? geofence;
  final RallySessionBackend? rally;

  SafetySnapshot _snapshot = const SafetySnapshot();
  bool _disposed = false;

  SafetySnapshot get snapshot => _snapshot;
  bool get available => geofence != null || rally != null;
  bool get busy => _snapshot.busy;

  Future<void> initialize() async {
    if (!available || busy) return;
    _set(_snapshot.copyWith(busy: true, error: null));
    try {
      final fence = geofence;
      final rallyBackend = rally;
      final config = fence == null
          ? _snapshot.geofenceConfig
          : await fence.loadGeofenceConfig();
      final points = fence == null
          ? _snapshot.fencePoints
          : await fence.loadGeofencePoints();
      final rallyPlan = rallyBackend == null
          ? _snapshot.rallyPlan
          : await rallyBackend.loadRallyPlan();
      _set(
        _snapshot.copyWith(
          busy: false,
          geofenceConfig: config,
          fencePoints: points,
          rallyPlan: rallyPlan,
          error: null,
        ),
      );
    } on Object catch (error) {
      _set(_snapshot.copyWith(busy: false, error: '$error'));
    }
  }

  Future<void> refreshFence() async {
    final backend = geofence;
    if (backend == null || busy) return;
    _set(_snapshot.copyWith(busy: true, error: null));
    try {
      await _refreshFenceSnapshot(backend);
      _set(_snapshot.copyWith(busy: false, error: null));
    } on Object catch (error) {
      _set(_snapshot.copyWith(busy: false, error: '$error'));
    }
  }

  Future<ConnectionOperationResult> setFenceConfig(
    GeofenceConfigSnapshot config,
  ) => _run(() async {
    final backend = geofence;
    if (backend == null) return _unavailable('电子围栏能力不可用');
    final result = await backend.setGeofenceConfig(config);
    if (result.success) {
      _set(_snapshot.copyWith(geofenceConfig: config));
    }
    return result;
  });

  Future<ConnectionOperationResult> enableFence(bool enabled) => _run(() async {
    final backend = geofence;
    if (backend == null) return _unavailable('电子围栏能力不可用');
    final result = await backend.enableGeofence(enabled);
    if (result.success) {
      _set(
        _snapshot.copyWith(
          geofenceConfig: _snapshot.geofenceConfig.copyWith(enabled: enabled),
        ),
      );
    }
    return result;
  });

  Future<ConnectionOperationResult> addFencePoint(FencePointSnapshot point) =>
      _run(() async {
        final backend = geofence;
        if (backend == null) return _unavailable('电子围栏能力不可用');
        final result = await backend.addGeofencePoint(point);
        if (result.success) {
          _set(
            _snapshot.copyWith(fencePoints: [..._snapshot.fencePoints, point]),
          );
        }
        return result;
      });

  Future<ConnectionOperationResult> removeFencePoint(int index) =>
      _run(() async {
        final backend = geofence;
        if (backend == null) return _unavailable('电子围栏能力不可用');
        final result = await backend.removeGeofencePoint(index);
        if (result.success &&
            index >= 0 &&
            index < _snapshot.fencePoints.length) {
          final points = [..._snapshot.fencePoints]..removeAt(index);
          _set(_snapshot.copyWith(fencePoints: points));
        }
        return result;
      });

  Future<GeofenceValidationSnapshot> validateFence() async {
    final backend = geofence;
    if (backend == null) {
      return const GeofenceValidationSnapshot(valid: false, error: '电子围栏能力不可用');
    }
    try {
      final validation = await backend.validateGeofence();
      _set(_snapshot.copyWith(validation: validation, error: null));
      return validation;
    } on Object catch (error) {
      final validation = GeofenceValidationSnapshot(
        valid: false,
        error: '$error',
      );
      _set(_snapshot.copyWith(validation: validation, error: '$error'));
      return validation;
    }
  }

  Future<String?> exportFenceJson() async =>
      _export(() => geofence!.exportGeofenceJson());
  Future<String?> exportFenceKml({String name = 'Geofence'}) async =>
      _export(() => geofence!.exportGeofenceKml(name: name));

  Future<ConnectionOperationResult> importFenceJson(String content) =>
      _run(() async {
        final backend = geofence;
        if (backend == null) return _unavailable('电子围栏能力不可用');
        final result = await backend.importGeofenceJson(content);
        if (result.success) await _refreshFenceSnapshot(backend);
        return result;
      });

  Future<ConnectionOperationResult> downloadFence() => _run(() async {
    final backend = geofence;
    if (backend == null) return _unavailable('电子围栏能力不可用');
    final result = await backend.downloadGeofence();
    if (result.success) await _refreshFenceSnapshot(backend);
    return result;
  });

  Future<ConnectionOperationResult> clearFence() => _run(() async {
    final backend = geofence;
    if (backend == null) return _unavailable('电子围栏能力不可用');
    final result = await backend.clearGeofence();
    if (result.success) {
      _set(_snapshot.copyWith(fencePoints: const [], clearValidation: true));
    }
    return result;
  });

  Future<ConnectionOperationResult> downloadRally() => _run(() async {
    final backend = rally;
    if (backend == null) return _unavailable('Rally 能力不可用');
    final result = await backend.downloadRally();
    if (result.success) {
      _set(_snapshot.copyWith(rallyPlan: await backend.loadRallyPlan()));
    }
    return result;
  });

  Future<ConnectionOperationResult> clearRally() => _run(() async {
    final backend = rally;
    if (backend == null) return _unavailable('Rally 能力不可用');
    final result = await backend.clearRally();
    if (result.success) {
      _set(
        _snapshot.copyWith(
          rallyPlan: const MissionPlanSnapshot(items: [], valid: false),
        ),
      );
    }
    return result;
  });

  Future<ConnectionOperationResult> _run(
    Future<ConnectionOperationResult> Function() operation,
  ) async {
    if (busy) {
      return const ConnectionOperationResult(success: false, message: '操作进行中');
    }
    _set(_snapshot.copyWith(busy: true, error: null, lastResult: null));
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

  Future<void> _refreshFenceSnapshot(GeofenceSessionBackend backend) async {
    final config = await backend.loadGeofenceConfig();
    final points = await backend.loadGeofencePoints();
    _set(
      _snapshot.copyWith(
        geofenceConfig: config,
        fencePoints: points,
        error: null,
      ),
    );
  }

  Future<String?> _export(Future<String> Function() operation) async {
    try {
      return await operation();
    } on Object catch (error) {
      _set(_snapshot.copyWith(error: '$error'));
      return null;
    }
  }

  ConnectionOperationResult _unavailable(String message) =>
      ConnectionOperationResult(success: false, message: message);

  void _set(SafetySnapshot next) {
    _snapshot = next;
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }
}

final class SafetySnapshot {
  const SafetySnapshot({
    this.busy = false,
    this.geofenceConfig = const GeofenceConfigSnapshot(),
    this.fencePoints = const [],
    this.validation,
    this.rallyPlan = const MissionPlanSnapshot(items: [], valid: false),
    this.lastResult,
    this.error,
  });

  final bool busy;
  final GeofenceConfigSnapshot geofenceConfig;
  final List<FencePointSnapshot> fencePoints;
  final GeofenceValidationSnapshot? validation;
  final MissionPlanSnapshot rallyPlan;
  final ConnectionOperationResult? lastResult;
  final String? error;

  SafetySnapshot copyWith({
    bool? busy,
    GeofenceConfigSnapshot? geofenceConfig,
    List<FencePointSnapshot>? fencePoints,
    GeofenceValidationSnapshot? validation,
    bool clearValidation = false,
    MissionPlanSnapshot? rallyPlan,
    ConnectionOperationResult? lastResult,
    String? error,
  }) => SafetySnapshot(
    busy: busy ?? this.busy,
    geofenceConfig: geofenceConfig ?? this.geofenceConfig,
    fencePoints: fencePoints ?? this.fencePoints,
    validation: clearValidation ? null : validation ?? this.validation,
    rallyPlan: rallyPlan ?? this.rallyPlan,
    lastResult: lastResult ?? this.lastResult,
    error: error,
  );
}
