import 'dart:async';

import 'calibration_models.dart';
import 'connection_models.dart';
import 'mission_models.dart';
import 'parameter_models.dart';
import 'flight_log_models.dart';
import 'vehicle_file_models.dart';
import 'flight_control_models.dart';
import 'safety_models.dart';
import 'fleet_models.dart';

abstract interface class ConnectionSessionBackend {
  Future<List<ConnectionSerialPort>> listSerialPorts();
  Future<ConnectionOperationResult> connect(ConnectionRequest request);
  Future<ConnectionOperationResult> disconnect();
  Future<ConnectionOperationResult> startParameterSync();
  Future<ConnectionSessionSnapshot> loadSessionSnapshot(
    ConnectionRequest request,
  );
  Future<FlightDashboardData> loadTelemetrySnapshot();
  Future<ConnectionBootstrapSnapshot> loadBootstrapSnapshot();
  void dispose();
}

abstract interface class FlightControllerMessageSource {
  Stream<FlightControllerMessage> get messages;
}

abstract interface class ParameterSessionBackend {
  Future<List<FlightControllerParameter>> loadParameters();
  Future<ParameterDescriptor> loadParameterDescriptor(String name);
  Future<List<ParameterDescriptor>> loadParameterDescriptors();
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  );
}

abstract interface class ParameterFileSessionBackend {
  Future<List<ParameterFileEntry>> parseParameterFile(String content);
  Future<String> createParameterFile(
    List<FlightControllerParameter> parameters,
  );
}

/// Optional semantic workflow for a cache-aware parameter change-set.
/// Adapters without this capability retain the single-write fallback.
abstract interface class ParameterChangeSetSessionBackend {
  Future<ParameterChangeSetPreview> prepareParameterChangeSet(
    List<ParameterFileEntry> entries,
  );
  Future<ParameterWriteSummary> commitParameterChangeSet(
    List<StagedParameterChange> changes,
  );
}

abstract interface class ParameterRefreshSessionBackend {
  Future<List<FlightControllerParameter>> refreshParameters();
}

abstract interface class ServoOutputSessionBackend {
  Future<ServoOutputSnapshot> loadServoOutputs();
}

abstract interface class RemoteControlInputSessionBackend {
  Future<RemoteControlInputSnapshot> loadRemoteControlInputs();
}

abstract interface class SerialHardwareSessionBackend {
  Future<List<SerialPortHardwareDescriptor>> loadSerialPortHardware();
}

abstract interface class MissionSessionBackend {
  Future<MissionPlanSnapshot> downloadMission();
  Future<ConnectionOperationResult> uploadMission(
    List<MissionItemSnapshot> items,
  );
}

/// Optional mission controls kept separate from the transfer contract so
/// lightweight/demo backends can still provide download/upload only.
abstract interface class MissionControlSessionBackend
    implements MissionSessionBackend {
  Future<ConnectionOperationResult> clearMission();

  Future<ConnectionOperationResult> setCurrentMission(int sequence);

  Future<ConnectionOperationResult> startMission();
}

abstract interface class MissionProgressSessionBackend {
  Future<MissionProgressSnapshot> loadMissionProgress();
}

abstract interface class MissionPlanningSessionBackend {
  Future<MissionGenerationResult> generateSurvey(SurveyConfig config);
  Future<MissionGenerationResult> generateCorridor(CorridorConfig config);
}

abstract interface class GeofenceSessionBackend {
  Future<GeofenceConfigSnapshot> loadGeofenceConfig();
  Future<List<FencePointSnapshot>> loadGeofencePoints();
  Future<ConnectionOperationResult> setGeofenceConfig(
    GeofenceConfigSnapshot config,
  );
  Future<ConnectionOperationResult> addGeofencePoint(FencePointSnapshot point);
  Future<ConnectionOperationResult> removeGeofencePoint(int index);
  Future<GeofenceValidationSnapshot> validateGeofence();
  Future<String> exportGeofenceJson();
  Future<String> exportGeofenceKml({String name = 'Geofence'});
  Future<ConnectionOperationResult> importGeofenceJson(String content);
  Future<ConnectionOperationResult> downloadGeofence();
  Future<ConnectionOperationResult> clearGeofence();
  Future<ConnectionOperationResult> enableGeofence(bool enabled);
}

abstract interface class RallySessionBackend {
  Future<ConnectionOperationResult> downloadRally();
  Future<MissionPlanSnapshot> loadRallyPlan();
  Future<ConnectionOperationResult> clearRally();
}

/// Optional onboard DataFlash log capability.
///
/// Kept separate from `ConnectionSessionBackend` so lightweight backends and
/// future frontends do not have to implement log transfer to connect.
abstract interface class FlightLogSessionBackend {
  Future<List<FlightLogEntry>> loadFlightLogs();

  Stream<FlightLogDownloadProgress> downloadFlightLog(
    FlightLogEntry log,
    String destinationPath,
  );

  Future<void> cancelFlightLogDownload();
}

/// Optional MAVLink FTP filesystem capability.
///
/// DataFlash logs and black-box files are both exposed as files by the
/// vehicle filesystem, so this contract deliberately stays independent from
/// the legacy DataFlash log API above.
abstract interface class VehicleFileSessionBackend {
  Future<List<VehicleFileEntry>> listDirectory(String path);

  Stream<VehicleFileDownloadProgress> downloadFile(
    VehicleFileEntry file,
    String destinationPath,
  );

  Stream<VehicleFileUploadProgress> uploadFile(
    String localPath,
    String remotePath,
  );

  Future<void> cancelFileDownload();

  Future<void> cancelFileTransfer();
}

/// Multi-vehicle discovery and active-session routing.
///
/// Commands remain on the existing flight-control contract. This interface
/// only owns vehicle identity and selection, which keeps it small and easy to
/// replace in another frontend.
abstract interface class FleetSessionBackend {
  Future<List<FleetVehicleSnapshot>> loadVehicles();
  Future<int?> loadActiveVehicleId();
  Future<ConnectionOperationResult> selectVehicle(int vehicleId);
  Future<ConnectionOperationResult> closeVehicle(int vehicleId);
}

abstract interface class CalibrationSessionBackend {
  Future<ConnectionOperationResult> startCalibration(
    SensorCalibrationType type, {
    AccelerometerCalibrationMode accelerometerMode =
        AccelerometerCalibrationMode.sixFace,
  });
  Future<ConnectionOperationResult> confirmAccelerometerOrientation(int step);
  Future<ConnectionOperationResult> cancelCalibration();
  Future<CalibrationStatusSnapshot> loadCalibrationStatus();
}

/// Optional calibration audit data. Kept separate so lightweight/demo
/// backends can still provide the interactive calibration workflow.
abstract interface class CalibrationHistorySessionBackend {
  Future<List<CalibrationResultSnapshot>> loadCalibrationHistory(
    SensorCalibrationType type,
  );
  Future<CalibrationRetrySnapshot> loadCalibrationRetryInfo();
}

abstract interface class SensorTelemetrySessionBackend {
  Future<SensorTelemetrySnapshot> loadSensorTelemetry();
}

/// Optional high-level vehicle control capability. Implementations should
/// keep command acknowledgement/error semantics in the adapter, leaving UI
/// shells to consume stable domain results.
abstract interface class FlightControlSessionBackend {
  Future<List<FlightModeOption>> loadFlightModes();
  Future<FlightModeStatus> loadFlightModeStatus();
  Future<ConnectionOperationResult> arm({bool force = false});
  Future<ConnectionOperationResult> disarm({bool force = false});
  Future<ConnectionOperationResult> setFlightMode(String mode);
  Future<ConnectionOperationResult> returnToLaunch();
  Future<ConnectionOperationResult> takeoff(double altitudeMeters);
  Future<ConnectionOperationResult> land();
  Future<ConnectionOperationResult> emergencyStop();
  Future<ConnectionOperationResult> guidedPoint(GuidedPointRequest request);
  Future<PreflightCheckResult> preflightCheck();
}

abstract interface class ConnectionBackendAvailability {
  String get unavailableReason;
}
