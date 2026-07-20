import 'dart:async';

import 'connection_models.dart';
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

abstract interface class ConnectionBackendAvailability {
  String get unavailableReason;
}
