import 'dart:async';

import 'calibration_models.dart';
import 'connection_models.dart';
import 'mission_models.dart';
import 'parameter_models.dart';

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

abstract interface class ConnectionBackendAvailability {
  String get unavailableReason;
}
