import 'connection_models.dart';

final class FlightModeOption {
  const FlightModeOption({required this.name, this.label});

  final String name;
  final String? label;

  String get displayName => label ?? name;
}

final class FlightModeStatus {
  const FlightModeStatus({
    required this.currentMode,
    required this.armed,
    this.inFlight = false,
    this.baseMode = 0,
    this.customMode = 0,
  });

  final String currentMode;
  final bool armed;
  final bool inFlight;
  final int baseMode;
  final int customMode;
}

final class PreflightCheckItem {
  const PreflightCheckItem({
    required this.name,
    required this.ok,
    required this.message,
  });

  final String name;
  final bool ok;
  final String message;
}

final class PreflightCheckResult {
  const PreflightCheckResult({
    required this.ready,
    required this.message,
    this.items = const [],
  });

  final bool ready;
  final String message;
  final List<PreflightCheckItem> items;
}

final class GuidedPointRequest {
  const GuidedPointRequest({
    required this.latitude,
    required this.longitude,
    required this.altitudeMeters,
    this.yawDegrees = 0,
    this.speedMetersPerSecond = -1,
  });

  final double latitude;
  final double longitude;
  final double altitudeMeters;
  final double yawDegrees;
  final double speedMetersPerSecond;
}

final class FlightControlSnapshot {
  const FlightControlSnapshot({
    this.modes = const [],
    this.modeStatus,
    this.lastResult,
    this.preflight,
    this.busy = false,
  });

  final List<FlightModeOption> modes;
  final FlightModeStatus? modeStatus;
  final ConnectionOperationResult? lastResult;
  final PreflightCheckResult? preflight;
  final bool busy;
}
