import '../connection_models.dart';
import '../connection_session_backend.dart';
import '../parameter_models.dart';
import 'mgcore_connection_client.dart';
import 'mgcore_json_decoder.dart';

final class MgCoreConnectionSessionBackend
    implements
        ConnectionSessionBackend,
        FlightControllerMessageSource,
        ParameterSessionBackend {
  MgCoreConnectionSessionBackend({
    required this.client,
    MgCoreJsonDecoder? decoder,
  }) : _decoder = decoder ?? MgCoreJsonDecoder();

  final MgCoreConnectionClient client;
  final MgCoreJsonDecoder _decoder;

  @override
  Stream<FlightControllerMessage> get messages => client.events
      .where((event) => event.type == 17)
      .map((event) => _decoder.decodeFlightControllerMessage(event.payload));

  @override
  Future<List<ConnectionSerialPort>> listSerialPorts() async {
    return _decoder.decodePorts(client.listPorts());
  }

  @override
  Future<ConnectionOperationResult> connect(ConnectionRequest request) async {
    if (request.transport != ConnectionTransport.serial) {
      return const ConnectionOperationResult(
        success: false,
        message: '第一版前端只接入串口连接',
      );
    }
    final port = request.port;
    if (port == null || port.isEmpty) {
      return const ConnectionOperationResult(success: false, message: '请选择串口');
    }
    return _decoder.decodeOperation(
      client.connectSerial(port, request.baudRate ?? 115200),
    );
  }

  @override
  Future<ConnectionOperationResult> disconnect() async {
    return _decoder.decodeOperation(client.disconnect());
  }

  @override
  Future<ConnectionOperationResult> startParameterSync() async {
    return _decoder.decodeOperation(client.requestAllParameters());
  }

  @override
  Future<List<FlightControllerParameter>> loadParameters() async {
    return _decoder.decodeParameters(client.allParameters());
  }

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async {
    return _decoder.decodeParameterDescriptor(client.parameterDescriptor(name));
  }

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async {
    final descriptors = <ParameterDescriptor>[];
    for (final group in _decoder.decodeParameterGroupNames(
      client.parameterDescriptorGroups(),
    )) {
      descriptors.addAll(
        _decoder.decodeParameterGroup(client.parameterDescriptorGroup(group)),
      );
    }
    return descriptors;
  }

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    return _decoder.decodeOperation(
      client.setParameter(parameter.name, value, parameter.mavlinkType),
    );
  }

  @override
  Future<ConnectionSessionSnapshot> loadSessionSnapshot(
    ConnectionRequest request,
  ) async {
    final diagnostics = client.connectionDiagnostics();
    return _decoder.decodeSession(
      diagnostics: diagnostics,
      dashboard: client.dashboardSnapshot(),
      bootstrap: _decodeBootstrap(diagnostics),
    );
  }

  @override
  Future<FlightDashboardData> loadTelemetrySnapshot() async {
    return _decoder.decodeDashboard(client.dashboardSnapshot());
  }

  @override
  Future<ConnectionBootstrapSnapshot> loadBootstrapSnapshot() async {
    return _decodeBootstrap(client.connectionDiagnostics());
  }

  ConnectionBootstrapSnapshot _decodeBootstrap(
    Map<String, Object?> diagnostics,
  ) {
    return _decoder.decodeBootstrap(
      diagnostics: diagnostics,
      parameterProgress: client.parameterProgress(),
      remoteControl: client.remoteControlChannels(),
      firmwareType: client.firmwareType(),
      firmwareVersion: client.firmwareVersion(),
    );
  }

  @override
  void dispose() {
    client.dispose();
  }
}
