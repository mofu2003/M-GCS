import 'dart:async';

import '../connection_models.dart';
import '../calibration_models.dart';
import '../connection_session_backend.dart';
import '../mission_models.dart';
import '../parameter_models.dart';
import 'mgcore_connection_client.dart';
import 'mgcore_json_decoder.dart';

final class MgCoreConnectionSessionBackend
    implements
        ConnectionSessionBackend,
        FlightControllerMessageSource,
        ParameterSessionBackend,
        ParameterFileSessionBackend,
        ParameterChangeSetSessionBackend,
        ParameterRefreshSessionBackend,
        ServoOutputSessionBackend,
        RemoteControlInputSessionBackend,
        SerialHardwareSessionBackend,
        MissionSessionBackend,
        CalibrationSessionBackend,
        CalibrationHistorySessionBackend,
        SensorTelemetrySessionBackend {
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
  Future<List<FlightControllerParameter>> refreshParameters() async {
    const maxAttempts = 3;
    for (var attempt = 1; attempt <= maxAttempts; attempt++) {
      final request = _decoder.decodeOperation(client.requestAllParameters());
      if (!request.success) {
        throw StateError(request.message);
      }

      final deadline = DateTime.now().add(const Duration(seconds: 30));
      var lastReceived = -1;
      var lastTotal = -1;
      var lastProgressAt = DateTime.now();
      while (DateTime.now().isBefore(deadline)) {
        await Future<void>.delayed(const Duration(milliseconds: 100));
        final progress = client.parameterProgress();
        final received = (progress['received'] as num?)?.toInt() ?? 0;
        final total = (progress['total'] as num?)?.toInt() ?? 0;
        if (total > 0 && received >= total) {
          return loadParameters();
        }
        if (received != lastReceived || total != lastTotal) {
          lastReceived = received;
          lastTotal = total;
          lastProgressAt = DateTime.now();
        } else if (DateTime.now().difference(lastProgressAt) >=
            const Duration(seconds: 8)) {
          break;
        }
      }
    }
    throw TimeoutException('参数下载停滞，请检查飞控链路后重试');
  }

  @override
  Future<ServoOutputSnapshot> loadServoOutputs() async {
    return _decoder.decodeServoOutputs(client.servoOutputs());
  }

  @override
  Future<RemoteControlInputSnapshot> loadRemoteControlInputs() async {
    return _decoder.decodeRemoteControlInputs(client.remoteControlChannels());
  }

  @override
  Future<List<SerialPortHardwareDescriptor>> loadSerialPortHardware() async {
    final response = client.serialUartNames();
    if (response['success'] != true) {
      final reason = response['error'];
      throw StateError(
        reason is String && reason.isNotEmpty
            ? reason
            : '无法从飞控读取 @SYS/uarts.txt',
      );
    }
    final items = _decoder.decodeSerialPortHardware(response);
    if (items.isEmpty) {
      throw StateError('@SYS/uarts.txt 中没有可用的 SERIALn 到 UART/USART 映射');
    }
    return items;
  }

  @override
  Future<MissionPlanSnapshot> downloadMission() async {
    final request = _decoder.decodeOperation(client.missionDownload());
    if (!request.success) throw StateError(request.message);

    final deadline = DateTime.now().add(const Duration(seconds: 30));
    var transferObserved = false;
    var lastProgressAt = DateTime.now();
    var lastReceived = -1;
    while (DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 120));
      final progress = client.missionProgress();
      final state = progress['state'] as String? ?? 'idle';
      final received = (progress['received'] as num?)?.toInt() ?? 0;
      if (state != 'idle') transferObserved = true;
      if (received != lastReceived) {
        lastReceived = received;
        lastProgressAt = DateTime.now();
      }
      if (transferObserved && state == 'idle') {
        return _decoder.decodeMissionPlan(client.missionPlan());
      }
      if (transferObserved &&
          DateTime.now().difference(lastProgressAt) >=
              const Duration(seconds: 8)) {
        throw TimeoutException('航点下载停滞，请检查飞控链路后重试');
      }
      if (!transferObserved &&
          DateTime.now().difference(lastProgressAt) >=
              const Duration(seconds: 3)) {
        return _decoder.decodeMissionPlan(client.missionPlan());
      }
    }
    throw TimeoutException('航点下载超时，请检查飞控链路后重试');
  }

  @override
  Future<ConnectionOperationResult> uploadMission(
    List<MissionItemSnapshot> items,
  ) async {
    if (items.isEmpty) {
      return const ConnectionOperationResult(
        success: false,
        message: '任务中没有航点',
      );
    }
    return _decoder.decodeOperation(
      client.missionUpload(
        items
            .asMap()
            .entries
            .map(
              (entry) =>
                  entry.value.copyWith(sequence: entry.key).toTransferJson(),
            )
            .toList(growable: false),
      ),
    );
  }

  @override
  Future<ConnectionOperationResult> startCalibration(
    SensorCalibrationType type, {
    AccelerometerCalibrationMode accelerometerMode =
        AccelerometerCalibrationMode.sixFace,
  }) async {
    if (type == SensorCalibrationType.airspeed) {
      return _decoder.decodeOperation(client.startAirspeedCalibration());
    }
    final nativeType = type == SensorCalibrationType.accelerometer ? 1 : 2;
    return _decoder.decodeOperation(
      client.startCalibration(nativeType, accelerometerMode.nativeValue),
    );
  }

  @override
  Future<ConnectionOperationResult> confirmAccelerometerOrientation(
    int step,
  ) async {
    return _decoder.decodeOperation(
      client.confirmAccelerometerOrientation(step),
    );
  }

  @override
  Future<ConnectionOperationResult> cancelCalibration() async {
    return _decoder.decodeOperation(client.cancelCalibration());
  }

  @override
  Future<CalibrationStatusSnapshot> loadCalibrationStatus() async {
    return _decoder.decodeCalibrationStatus(client.calibrationStatus());
  }

  @override
  Future<List<CalibrationResultSnapshot>> loadCalibrationHistory(
    SensorCalibrationType type,
  ) async {
    final nativeType = switch (type) {
      SensorCalibrationType.accelerometer => 1,
      SensorCalibrationType.magnetometer => 2,
      SensorCalibrationType.airspeed => 3,
    };
    return _decoder.decodeCalibrationHistory(
      client.calibrationHistory(nativeType),
    );
  }

  @override
  Future<CalibrationRetrySnapshot> loadCalibrationRetryInfo() async {
    return _decoder.decodeCalibrationRetryInfo(client.calibrationRetryInfo());
  }

  @override
  Future<SensorTelemetrySnapshot> loadSensorTelemetry() async {
    return _decoder.decodeSensorTelemetry(client.highresImu());
  }

  @override
  Future<List<ParameterFileEntry>> parseParameterFile(String content) async {
    final result = client.importParameterFile(content);
    final items = result['items'];
    if (result['success'] != true || items is! List<Object?>) {
      throw const FormatException('文件中没有可读取的参数');
    }
    return items
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final value = item['value'];
          return ParameterFileEntry(
            name: item['name'] as String? ?? '',
            value: value is num ? value.toDouble() : double.nan,
            mavlinkType: (item['type'] as num?)?.toInt() ?? 9,
          );
        })
        .where((entry) => entry.name.isNotEmpty && entry.value.isFinite)
        .toList(growable: false);
  }

  @override
  Future<String> createParameterFile(
    List<FlightControllerParameter> parameters,
  ) async {
    final content = client.exportParameterFile(
      parameters
          .map(
            (parameter) => <String, Object?>{
              'name': parameter.name,
              'value': parameter.value,
            },
          )
          .toList(growable: false),
    );
    if (content.isEmpty) {
      throw StateError('参数文件生成失败');
    }
    return content;
  }

  @override
  Future<ParameterChangeSetPreview> prepareParameterChangeSet(
    List<ParameterFileEntry> entries,
  ) async {
    final response = client.prepareParameterChangeSet(
      entries
          .map(
            (entry) => <String, Object?>{
              'name': entry.name,
              'value': entry.value,
              'type': entry.mavlinkType,
            },
          )
          .toList(growable: false),
    );
    if (response['success'] == false) {
      throw StateError(response['error']?.toString() ?? '参数变更集预览失败');
    }
    return _decoder.decodeParameterChangeSetPreview(response);
  }

  @override
  Future<ParameterWriteSummary> commitParameterChangeSet(
    List<StagedParameterChange> changes,
  ) async {
    final response = client.commitParameterChangeSet(
      changes
          .map(
            (change) => <String, Object?>{
              'name': change.parameter.name,
              'value': change.stagedValue,
              'type': change.parameter.mavlinkType,
            },
          )
          .toList(growable: false),
    );
    if (response['success'] == false && response['failed'] is! List<Object?>) {
      throw StateError(response['error']?.toString() ?? '参数变更集写入失败');
    }
    return _decoder.decodeParameterChangeSetCommit(response);
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
