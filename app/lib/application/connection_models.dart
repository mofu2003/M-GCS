enum ConnectionTransport { serial, tcp, udp }

enum ConnectionStatus { idle, connecting, connected, disconnecting, error }

enum ConnectionBootstrapState { idle, downloading, complete, failed }

final class ConnectionSerialPort {
  const ConnectionSerialPort({
    required this.name,
    required this.path,
    this.description,
    this.manufacturer,
  });

  final String name;
  final String path;
  final String? description;
  final String? manufacturer;
}

final class ConnectionRequest {
  const ConnectionRequest.serial({required this.port, this.baudRate = 115200})
    : transport = ConnectionTransport.serial,
      endpoint = null;

  const ConnectionRequest.network({
    required this.transport,
    required this.endpoint,
  }) : port = null,
       baudRate = null;

  final ConnectionTransport transport;
  final String? port;
  final int? baudRate;
  final String? endpoint;
}

final class ConnectionOperationResult {
  const ConnectionOperationResult({
    required this.success,
    required this.message,
    this.raw,
  });

  final bool success;
  final String message;
  final Map<String, Object?>? raw;
}

final class AppConnectionSummary {
  const AppConnectionSummary({
    required this.status,
    required this.label,
    required this.detail,
    this.activeVehicleId,
    this.heartbeatAgeMs,
  });

  final ConnectionStatus status;
  final String label;
  final String detail;
  final int? activeVehicleId;
  final int? heartbeatAgeMs;
}

final class FlightDashboardData {
  const FlightDashboardData({
    required this.valid,
    required this.mode,
    required this.armed,
    this.altitudeMeters,
    this.groundSpeedMetersPerSecond,
    this.batteryVoltage,
    this.batteryRemainingPercent,
    this.latitude,
    this.longitude,
    this.satellitesVisible,
    this.pitchDegrees,
    this.rollDegrees,
    this.headingDegrees,
    this.airspeedMetersPerSecond,
    this.climbMetersPerSecond,
  });

  final bool valid;
  final String mode;
  final bool armed;
  final double? altitudeMeters;
  final double? groundSpeedMetersPerSecond;
  final double? batteryVoltage;
  final int? batteryRemainingPercent;
  final double? latitude;
  final double? longitude;
  final int? satellitesVisible;
  final double? pitchDegrees;
  final double? rollDegrees;
  final double? headingDegrees;
  final double? airspeedMetersPerSecond;
  final double? climbMetersPerSecond;
}

final class ConnectionSessionSnapshot {
  const ConnectionSessionSnapshot({
    required this.connection,
    required this.dashboard,
    this.bootstrap = const ConnectionBootstrapSnapshot(),
    this.rawDiagnostics,
  });

  final AppConnectionSummary connection;
  final FlightDashboardData dashboard;
  final ConnectionBootstrapSnapshot bootstrap;
  final Map<String, Object?>? rawDiagnostics;
}

final class ParameterSyncProgress {
  const ParameterSyncProgress({this.received = 0, this.total = 0});

  final int received;
  final int total;

  bool get hasTotal => total > 0;
  bool get isComplete => hasTotal && received >= total;
  int? get percent => hasTotal ? (received * 100 ~/ total).clamp(0, 100) : null;
}

final class FlightControllerIdentity {
  const FlightControllerIdentity({
    this.name = '已连接飞控',
    this.airframe = '机架待确认',
    this.firmwareType = '固件待确认',
    this.firmwareMajor,
    this.firmwareMinor,
    this.firmwarePatch,
  });

  final String name;
  final String airframe;
  final String firmwareType;
  final int? firmwareMajor;
  final int? firmwareMinor;
  final int? firmwarePatch;

  String get versionLabel =>
      firmwareMajor == null || firmwareMinor == null || firmwarePatch == null
      ? '版本待确认'
      : '$firmwareMajor.$firmwareMinor.$firmwarePatch';
}

final class LinkQuality {
  const LinkQuality({
    this.measuring = true,
    this.received = 0,
    this.lost = 0,
    this.percent,
  });

  final bool measuring;
  final int received;
  final int lost;
  final int? percent;
}

final class RemoteControlSignal {
  const RemoteControlSignal({this.percent, this.rawRssi});

  final int? percent;
  final int? rawRssi;

  bool get isAvailable => rawRssi != null;
}

final class ConnectionBootstrapSnapshot {
  const ConnectionBootstrapSnapshot({
    this.parameterProgress = const ParameterSyncProgress(),
    this.identity = const FlightControllerIdentity(),
    this.linkQuality = const LinkQuality(),
    this.remoteControlSignal = const RemoteControlSignal(),
  });

  final ParameterSyncProgress parameterProgress;
  final FlightControllerIdentity identity;
  final LinkQuality linkQuality;
  final RemoteControlSignal remoteControlSignal;
}

final class FlightControllerMessage {
  const FlightControllerMessage({
    required this.text,
    required this.severity,
    required this.receivedAt,
  });

  final String text;
  final String severity;
  final DateTime receivedAt;
}
