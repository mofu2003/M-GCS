import 'connection_session_backend.dart';

/// Read-only discovery of optional application capabilities.
///
/// This keeps capability checks in one application-layer module. It does not
/// add a service locator, event bus, repository layer, or runtime state
/// machine; the existing capability interfaces remain the contracts.
final class SessionCapabilities {
  SessionCapabilities(Object backend)
    : connection = backend is ConnectionSessionBackend ? backend : null,
      parameters = backend is ParameterSessionBackend ? backend : null,
      parameterFiles = backend is ParameterFileSessionBackend ? backend : null,
      parameterChanges = backend is ParameterChangeSetSessionBackend
          ? backend
          : null,
      parameterRefresh = backend is ParameterRefreshSessionBackend
          ? backend
          : null,
      servoOutputs = backend is ServoOutputSessionBackend ? backend : null,
      remoteControlInputs = backend is RemoteControlInputSessionBackend
          ? backend
          : null,
      serialHardware = backend is SerialHardwareSessionBackend ? backend : null,
      mission = backend is MissionSessionBackend ? backend : null,
      missionProgress = backend is MissionProgressSessionBackend
          ? backend
          : null,
      missionPlanning = backend is MissionPlanningSessionBackend
          ? backend
          : null,
      geofence = backend is GeofenceSessionBackend ? backend : null,
      rally = backend is RallySessionBackend ? backend : null,
      calibration = backend is CalibrationSessionBackend ? backend : null,
      calibrationHistory = backend is CalibrationHistorySessionBackend
          ? backend
          : null,
      sensorTelemetry = backend is SensorTelemetrySessionBackend
          ? backend
          : null,
      flightLogs = backend is FlightLogSessionBackend ? backend : null,
      vehicleFiles = backend is VehicleFileSessionBackend ? backend : null,
      fleet = backend is FleetSessionBackend ? backend : null,
      flightControl = backend is FlightControlSessionBackend ? backend : null;

  final ConnectionSessionBackend? connection;
  final ParameterSessionBackend? parameters;
  final ParameterFileSessionBackend? parameterFiles;
  final ParameterChangeSetSessionBackend? parameterChanges;
  final ParameterRefreshSessionBackend? parameterRefresh;
  final ServoOutputSessionBackend? servoOutputs;
  final RemoteControlInputSessionBackend? remoteControlInputs;
  final SerialHardwareSessionBackend? serialHardware;
  final MissionSessionBackend? mission;
  final MissionProgressSessionBackend? missionProgress;
  final MissionPlanningSessionBackend? missionPlanning;
  final GeofenceSessionBackend? geofence;
  final RallySessionBackend? rally;
  final CalibrationSessionBackend? calibration;
  final CalibrationHistorySessionBackend? calibrationHistory;
  final SensorTelemetrySessionBackend? sensorTelemetry;
  final FlightLogSessionBackend? flightLogs;
  final VehicleFileSessionBackend? vehicleFiles;
  final FleetSessionBackend? fleet;
  final FlightControlSessionBackend? flightControl;

  bool get supportsMission => mission != null;
  bool get supportsParameterFiles => parameterFiles != null;
  bool get supportsCalibration => calibration != null;
  bool get supportsFlightLogs => flightLogs != null;
  bool get supportsVehicleFiles => vehicleFiles != null;
  bool get supportsFlightControl => flightControl != null;
}
