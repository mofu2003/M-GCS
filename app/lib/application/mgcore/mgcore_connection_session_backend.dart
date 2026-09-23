import 'dart:async';
import 'dart:io';

import '../connection_models.dart';
import '../calibration_models.dart';
import '../connection_session_backend.dart';
import '../flight_log_models.dart';
import '../vehicle_file_models.dart';
import '../mission_models.dart';
import '../parameter_models.dart';
import '../flight_control_models.dart';
import '../safety_models.dart';
import '../fleet_models.dart';
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
        MissionControlSessionBackend,
        MissionProgressSessionBackend,
        MissionPlanningSessionBackend,
        GeofenceSessionBackend,
        RallySessionBackend,
        FlightLogSessionBackend,
        VehicleFileSessionBackend,
        FleetSessionBackend,
        CalibrationSessionBackend,
        CalibrationHistorySessionBackend,
        SensorTelemetrySessionBackend,
        FlightControlSessionBackend {
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
    switch (request.transport) {
      case ConnectionTransport.serial:
        final port = request.port;
        if (port == null || port.isEmpty) {
          return const ConnectionOperationResult(
            success: false,
            message: '请选择串口',
          );
        }
        return _decoder.decodeOperation(
          client.connectSerial(port, request.baudRate ?? 115200),
        );
      case ConnectionTransport.tcp:
      case ConnectionTransport.udp:
        final endpoint = request.endpoint?.trim() ?? '';
        if (endpoint.isEmpty) {
          return const ConnectionOperationResult(
            success: false,
            message: '请输入有效的网络地址和端口',
          );
        }
        return _decoder.decodeOperation(
          request.transport == ConnectionTransport.tcp
              ? client.connectTcp(endpoint)
              : client.connectUdp(endpoint),
        );
    }
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
  Future<ConnectionOperationResult> clearMission() async {
    return _decoder.decodeOperation(client.missionClear());
  }

  @override
  Future<ConnectionOperationResult> setCurrentMission(int sequence) async {
    return _decoder.decodeOperation(client.missionSetCurrent(sequence));
  }

  @override
  Future<ConnectionOperationResult> startMission() async {
    return _decoder.decodeOperation(client.missionStart());
  }

  @override
  Future<MissionProgressSnapshot> loadMissionProgress() async {
    return _decoder.decodeMissionProgress(client.missionProgress());
  }

  @override
  Future<MissionGenerationResult> generateSurvey(SurveyConfig config) async {
    return _decoder.decodeMissionGeneration(
      client.surveyGenerate(config.toJson()),
    );
  }

  @override
  Future<MissionGenerationResult> generateCorridor(
    CorridorConfig config,
  ) async {
    return _decoder.decodeMissionGeneration(
      client.surveyGenerateCorridor(config.toJson()),
    );
  }

  @override
  Future<GeofenceConfigSnapshot> loadGeofenceConfig() async =>
      _decoder.decodeGeofenceConfig(client.geofenceGetConfig());

  @override
  Future<List<FencePointSnapshot>> loadGeofencePoints() async =>
      _decoder.decodeGeofencePoints(client.geofenceGetPoints());

  @override
  Future<ConnectionOperationResult> setGeofenceConfig(
    GeofenceConfigSnapshot config,
  ) async => _decoder.decodeOperation(
    client.geofenceSetConfig({
      'enabled': config.enabled,
      'max_altitude': config.maxAltitudeMeters,
      'min_altitude': config.minAltitudeMeters,
      'action': config.action,
      'return_at_min': config.returnAtMinimum,
      'breach_distance': config.breachDistanceMeters,
    }),
  );

  @override
  Future<ConnectionOperationResult> addGeofencePoint(
    FencePointSnapshot point,
  ) async => _decoder.decodeOperation(
    client.geofenceAddPoint(
      point.latitude,
      point.longitude,
      point.altitudeMeters,
    ),
  );

  @override
  Future<ConnectionOperationResult> removeGeofencePoint(int index) async =>
      _decoder.decodeOperation(client.geofenceRemovePoint(index));

  @override
  Future<GeofenceValidationSnapshot> validateGeofence() async =>
      _decoder.decodeGeofenceValidation(client.geofenceValidate());

  @override
  Future<String> exportGeofenceJson() async => client.geofenceExportJson();

  @override
  Future<String> exportGeofenceKml({String name = 'Geofence'}) async =>
      client.geofenceExportKml(name);

  @override
  Future<ConnectionOperationResult> importGeofenceJson(String content) async =>
      _decoder.decodeOperation(client.geofenceImportJson(content));

  @override
  Future<ConnectionOperationResult> downloadGeofence() async =>
      _decoder.decodeOperation(client.fenceDownload());

  @override
  Future<ConnectionOperationResult> clearGeofence() async =>
      _decoder.decodeOperation(client.fenceClear());

  @override
  Future<ConnectionOperationResult> enableGeofence(bool enabled) async =>
      _decoder.decodeOperation(client.fenceEnable(enabled));

  @override
  Future<ConnectionOperationResult> downloadRally() async =>
      _decoder.decodeOperation(client.rallyDownload());

  @override
  Future<MissionPlanSnapshot> loadRallyPlan() async =>
      _decoder.decodeMissionPlan(client.rallyGetPlan());

  @override
  Future<ConnectionOperationResult> clearRally() async =>
      _decoder.decodeOperation(client.rallyClear());

  @override
  Future<List<FlightLogEntry>> loadFlightLogs() async {
    final result = _decoder.decodeOperation(client.logRequestList());
    if (!result.success) throw StateError(result.message);

    final deadline = DateTime.now().add(const Duration(seconds: 6));
    var lastCount = -1;
    var lastChange = DateTime.now();
    List<Object?> entries = const [];
    while (DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 150));
      entries = client.logGetEntries();
      if (entries.length != lastCount) {
        lastCount = entries.length;
        lastChange = DateTime.now();
      }
      // LOG_ENTRY 列表可能合法地为空；空列表也应在稳定窗口后结束，
      // 避免飞控没有日志时无意义地等待完整超时。
      if (lastCount >= 0 &&
          DateTime.now().difference(lastChange) >=
              const Duration(milliseconds: 450)) {
        break;
      }
    }
    final logs = _decoder.decodeFlightLogs(entries).toList(growable: true)
      ..sort((a, b) => b.id.compareTo(a.id));
    return logs;
  }

  @override
  Stream<FlightLogDownloadProgress> downloadFlightLog(
    FlightLogEntry log,
    String destinationPath,
  ) async* {
    final result = _decoder.decodeOperation(
      client.logDownload(log.id, destinationPath),
    );
    if (!result.success) {
      yield FlightLogDownloadProgress(
        logId: log.id,
        receivedBytes: 0,
        totalBytes: log.sizeBytes,
        state: FlightLogDownloadState.failed,
        message: result.message,
      );
      return;
    }

    var lastReceived = -1;
    var lastChange = DateTime.now();
    yield FlightLogDownloadProgress(
      logId: log.id,
      receivedBytes: 0,
      totalBytes: log.sizeBytes,
      state: FlightLogDownloadState.downloading,
      destinationPath: destinationPath,
    );
    while (true) {
      await Future<void>.delayed(const Duration(milliseconds: 120));
      final status = _decoder.decodeFlightLogProgress(
        client.logDownloadStatus(),
      );
      final progress = FlightLogDownloadProgress(
        logId: log.id,
        receivedBytes: status.receivedBytes,
        totalBytes: status.totalBytes > 0 ? status.totalBytes : log.sizeBytes,
        state: status.state,
        destinationPath: status.destinationPath ?? destinationPath,
        message: status.message,
      );
      if (progress.receivedBytes != lastReceived) {
        lastReceived = progress.receivedBytes;
        lastChange = DateTime.now();
      }
      yield progress;
      if (progress.state != FlightLogDownloadState.downloading) return;
      if (DateTime.now().difference(lastChange) >= const Duration(seconds: 8)) {
        yield FlightLogDownloadProgress(
          logId: log.id,
          receivedBytes: progress.receivedBytes,
          totalBytes: progress.totalBytes,
          state: FlightLogDownloadState.failed,
          destinationPath: destinationPath,
          message: '下载停滞，请检查飞控链路后重试',
        );
        return;
      }
    }
  }

  @override
  Future<void> cancelFlightLogDownload() async {
    final result = _decoder.decodeOperation(client.logDownloadCancel());
    if (!result.success) throw StateError(result.message);
  }

  @override
  Future<List<VehicleFileEntry>> listDirectory(String path) async {
    final response = client.listVehicleDirectory(path);
    if (response['success'] != true) {
      throw StateError(response['error']?.toString() ?? '读取目录失败');
    }
    final items = response['items'];
    if (items is! List) return const [];
    return items
        .whereType<Map>()
        .map((item) {
          final name = item['name']?.toString() ?? '';
          final isDirectory = item['directory'] == true;
          final size = item['size_bytes'] is num
              ? (item['size_bytes'] as num).toInt()
              : 0;
          return VehicleFileEntry(
            name: name,
            path: joinVehiclePath(path, name),
            isDirectory: isDirectory,
            sizeBytes: size,
          );
        })
        .where((entry) => entry.name.isNotEmpty)
        .toList(growable: false)
      ..sort((a, b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory ? -1 : 1;
        return a.name.toLowerCase().compareTo(b.name.toLowerCase());
      });
  }

  @override
  Stream<VehicleFileDownloadProgress> downloadFile(
    VehicleFileEntry file,
    String destinationPath,
  ) async* {
    yield VehicleFileDownloadProgress(
      file: file,
      state: VehicleFileDownloadState.downloading,
      receivedBytes: 0,
      totalBytes: file.sizeBytes,
      destinationPath: destinationPath,
    );
    final result = _decoder.decodeOperation(
      client.downloadVehicleFile(file.path, destinationPath),
    );
    if (!result.success) {
      yield VehicleFileDownloadProgress(
        file: file,
        state: VehicleFileDownloadState.failed,
        receivedBytes: 0,
        totalBytes: file.sizeBytes,
        destinationPath: destinationPath,
        message: result.message,
      );
      return;
    }
    yield VehicleFileDownloadProgress(
      file: file,
      state: VehicleFileDownloadState.completed,
      receivedBytes: file.sizeBytes,
      totalBytes: file.sizeBytes,
      destinationPath: destinationPath,
    );
  }

  @override
  Future<void> cancelFileDownload() async {
    final result = _decoder.decodeOperation(client.cancelVehicleFileDownload());
    if (!result.success) throw StateError(result.message);
  }

  @override
  Stream<VehicleFileUploadProgress> uploadFile(
    String localPath,
    String remotePath,
  ) async* {
    final totalBytes = await File(localPath).length();
    yield VehicleFileUploadProgress(
      localPath: localPath,
      remotePath: remotePath,
      state: VehicleFileUploadState.uploading,
      sentBytes: 0,
      totalBytes: totalBytes,
    );
    final result = _decoder.decodeOperation(
      client.uploadVehicleFile(localPath, remotePath),
    );
    if (!result.success) {
      yield VehicleFileUploadProgress(
        localPath: localPath,
        remotePath: remotePath,
        state: VehicleFileUploadState.failed,
        sentBytes: 0,
        totalBytes: totalBytes,
        message: result.message,
      );
      return;
    }
    yield VehicleFileUploadProgress(
      localPath: localPath,
      remotePath: remotePath,
      state: VehicleFileUploadState.completed,
      sentBytes: totalBytes,
      totalBytes: totalBytes,
    );
  }

  @override
  Future<void> cancelFileTransfer() async {
    final result = _decoder.decodeOperation(client.cancelVehicleFileDownload());
    if (!result.success) throw StateError(result.message);
  }

  @override
  Future<List<FleetVehicleSnapshot>> loadVehicles() async {
    return client
        .vehicleList()
        .whereType<Map>()
        .map((item) {
          return FleetVehicleSnapshot(
            vehicleId: (item['vehicle_id'] as num?)?.toInt() ?? -1,
            systemId: (item['system_id'] as num?)?.toInt() ?? 0,
            name: item['name']?.toString() ?? '',
            endpoint: item['endpoint']?.toString() ?? '',
            firmwareType: item['firmware_type']?.toString() ?? '',
            vehicleType: item['vehicle_type']?.toString() ?? '',
            connected: item['connected'] == true,
          );
        })
        .where((vehicle) => vehicle.vehicleId >= 0)
        .toList(growable: false);
  }

  @override
  Future<int?> loadActiveVehicleId() async {
    final id = client.vehicleGetActive();
    return id < 0 ? null : id;
  }

  @override
  Future<ConnectionOperationResult> selectVehicle(int vehicleId) async =>
      _decoder.decodeOperation(client.vehicleSelect(vehicleId));

  @override
  Future<ConnectionOperationResult> closeVehicle(int vehicleId) async =>
      _decoder.decodeOperation(client.vehicleClose(vehicleId));

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
  Future<List<FlightModeOption>> loadFlightModes() async {
    return _decoder.decodeFlightModes(client.flightAvailableModes());
  }

  @override
  Future<FlightModeStatus> loadFlightModeStatus() async {
    return _decoder.decodeFlightModeStatus(client.flightModeStatus());
  }

  @override
  Future<ConnectionOperationResult> arm({bool force = false}) async =>
      _decoder.decodeOperation(client.commandArm(force: force));

  @override
  Future<ConnectionOperationResult> disarm({bool force = false}) async =>
      _decoder.decodeOperation(client.commandDisarm(force: force));

  @override
  Future<ConnectionOperationResult> setFlightMode(String mode) async =>
      _decoder.decodeOperation(client.commandSetMode(mode));

  @override
  Future<ConnectionOperationResult> returnToLaunch() async =>
      _decoder.decodeOperation(client.commandRtl());

  @override
  Future<ConnectionOperationResult> takeoff(double altitudeMeters) async =>
      _decoder.decodeOperation(client.commandTakeoff(altitudeMeters));

  @override
  Future<ConnectionOperationResult> land() async =>
      _decoder.decodeOperation(client.commandLand());

  @override
  Future<ConnectionOperationResult> emergencyStop() async =>
      _decoder.decodeOperation(client.commandEmergencyStop());

  @override
  Future<ConnectionOperationResult> guidedPoint(
    GuidedPointRequest request,
  ) async => _decoder.decodeOperation(
    client.commandGuidedPoint(
      request.latitude,
      request.longitude,
      request.altitudeMeters,
      request.yawDegrees,
      request.speedMetersPerSecond,
    ),
  );

  @override
  Future<PreflightCheckResult> preflightCheck() async =>
      _decoder.decodePreflightCheck(client.preflightCheck());

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
