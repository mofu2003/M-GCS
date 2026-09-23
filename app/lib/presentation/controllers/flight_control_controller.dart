import 'package:flutter/foundation.dart';

import '../../application/connection_models.dart';
import '../../application/connection_session_backend.dart';
import '../../application/flight_control_models.dart';

final class FlightControlController extends ChangeNotifier {
  FlightControlController({required this.backend});

  final FlightControlSessionBackend? backend;
  FlightControlSnapshot _snapshot = const FlightControlSnapshot();
  bool _disposed = false;

  FlightControlSnapshot get snapshot => _snapshot;
  bool get available => backend != null;

  Future<void> initialize() async {
    final adapter = backend;
    if (adapter == null) return;
    try {
      final modes = await adapter.loadFlightModes();
      final status = await adapter.loadFlightModeStatus();
      _snapshot = FlightControlSnapshot(modes: modes, modeStatus: status);
      _notify();
    } catch (error) {
      _setResult(ConnectionOperationResult(success: false, message: '$error'));
    }
  }

  Future<void> refreshStatus() async {
    final adapter = backend;
    if (adapter == null) return;
    try {
      final status = await adapter.loadFlightModeStatus();
      _snapshot = FlightControlSnapshot(
        modes: _snapshot.modes,
        modeStatus: status,
        lastResult: _snapshot.lastResult,
        preflight: _snapshot.preflight,
      );
      _notify();
    } catch (_) {}
  }

  Future<void> setMode(String mode) => _run(() => backend!.setFlightMode(mode));
  Future<void> arm() => _run(() => backend!.arm());
  Future<void> disarm() => _run(() => backend!.disarm());
  Future<void> rtl() => _run(() => backend!.returnToLaunch());
  Future<void> takeoff(double altitudeMeters) =>
      _run(() => backend!.takeoff(altitudeMeters));
  Future<void> land() => _run(() => backend!.land());
  Future<void> emergencyStop() => _run(() => backend!.emergencyStop());
  Future<void> guidedPoint(GuidedPointRequest request) =>
      _run(() => backend!.guidedPoint(request));

  Future<void> preflightCheck() async {
    final adapter = backend;
    if (adapter == null || _snapshot.busy) return;
    _snapshot = FlightControlSnapshot(
      modes: _snapshot.modes,
      modeStatus: _snapshot.modeStatus,
      lastResult: _snapshot.lastResult,
      preflight: _snapshot.preflight,
      busy: true,
    );
    _notify();
    try {
      final result = await adapter.preflightCheck();
      _snapshot = FlightControlSnapshot(
        modes: _snapshot.modes,
        modeStatus: _snapshot.modeStatus,
        lastResult: _snapshot.lastResult,
        preflight: result,
      );
      _notify();
    } catch (error) {
      _setResult(ConnectionOperationResult(success: false, message: '$error'));
    }
  }

  Future<void> _run(
    Future<ConnectionOperationResult> Function() command,
  ) async {
    if (backend == null || _snapshot.busy) return;
    _snapshot = FlightControlSnapshot(
      modes: _snapshot.modes,
      modeStatus: _snapshot.modeStatus,
      lastResult: _snapshot.lastResult,
      preflight: _snapshot.preflight,
      busy: true,
    );
    _notify();
    try {
      final result = await command();
      _setResult(result);
      await refreshStatus();
    } catch (error) {
      _setResult(ConnectionOperationResult(success: false, message: '$error'));
    }
  }

  void _setResult(ConnectionOperationResult result) {
    _snapshot = FlightControlSnapshot(
      modes: _snapshot.modes,
      modeStatus: _snapshot.modeStatus,
      lastResult: result,
      preflight: _snapshot.preflight,
    );
    _notify();
  }

  void _notify() {
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }
}
