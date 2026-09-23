import 'dart:async';
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'mgcore_ffi_api.dart';

final class MgCoreConnectionClient {
  MgCoreConnectionClient({required this.api}) {
    _core = api.create();
    _eventCallback = NativeCallable<MgCoreEventNative>.listener(_onNativeEvent);
    api.setEventCallback(_core, _eventCallback.nativeFunction);
  }

  final MgCoreFfiApi api;
  late final Pointer<Void> _core;
  late final NativeCallable<MgCoreEventNative> _eventCallback;
  bool _disposed = false;
  final StreamController<MgCoreEvent> _events = StreamController.broadcast();

  Stream<MgCoreEvent> get events => _events.stream;

  Map<String, Object?> listPorts() => _decodeObject(api.listPorts(_core));
  List<Object?> vehicleList() {
    final value = jsonDecode(api.vehicleList(_core));
    return value is List<Object?> ? value : const [];
  }

  Map<String, Object?> vehicleSelect(int vehicleId) =>
      _decodeObject(api.vehicleSelect(_core, vehicleId));
  Map<String, Object?> vehicleClose(int vehicleId) =>
      _decodeObject(api.vehicleClose(_core, vehicleId));
  int vehicleGetActive() => api.vehicleGetActive(_core);

  Map<String, Object?> connectSerial(String port, int baudRate) {
    return _decodeObject(api.connectSerial(_core, port, baudRate));
  }

  Map<String, Object?> connectTcp(String endpoint) {
    return _decodeObject(api.connectTcp(_core, endpoint));
  }

  Map<String, Object?> connectUdp(String endpoint) {
    return _decodeObject(api.connectUdp(_core, endpoint));
  }

  Map<String, Object?> disconnect() => _decodeObject(api.disconnect(_core));

  Map<String, Object?> connectionDiagnostics() {
    return _decodeObject(api.connectionDiagnostics(_core));
  }

  Map<String, Object?> dashboardSnapshot() {
    return _decodeObject(api.dashboardSnapshot(_core));
  }

  Map<String, Object?> requestAllParameters() {
    return _decodeObject(api.requestAllParameters(_core));
  }

  Map<String, Object?> parameterProgress() {
    return _decodeObject(api.parameterProgress(_core));
  }

  Map<String, Object?> allParameters() {
    return _decodeObject(api.allParameters(_core));
  }

  Map<String, Object?> prepareParameterChangeSet(
    List<Map<String, Object?>> changes,
  ) => _decodeObject(api.prepareParameterChangeSet(_core, jsonEncode(changes)));

  Map<String, Object?> commitParameterChangeSet(
    List<Map<String, Object?>> changes,
  ) => _decodeObject(api.commitParameterChangeSet(_core, jsonEncode(changes)));

  Map<String, Object?> importParameterFile(String content) {
    return _decodeObject(api.importParameterFile(_core, content));
  }

  String exportParameterFile(List<Map<String, Object?>> parameters) {
    return api.exportParameterFile(_core, jsonEncode(parameters));
  }

  Map<String, Object?> setParameter(String name, double value, int type) {
    return _decodeObject(api.setParameter(_core, name, value, type));
  }

  Map<String, Object?> parameterDescriptor(String name) {
    return _decodeObject(api.readParameterDescriptor(_core, name));
  }

  Map<String, Object?> parameterDescriptorGroups() {
    return _decodeObject(api.parameterDescriptorGroups(_core));
  }

  Map<String, Object?> parameterDescriptorGroup(String name) {
    return _decodeObject(api.parameterDescriptorGroup(_core, name));
  }

  Map<String, Object?> remoteControlChannels() {
    return _decodeObject(api.remoteControlChannels(_core));
  }

  Map<String, Object?> servoOutputs() {
    return _decodeObject(api.servoOutputs(_core));
  }

  Map<String, Object?> startCalibration(int type, int mode) {
    return _decodeObject(api.calibrationStart(_core, type, mode));
  }

  Map<String, Object?> cancelCalibration() {
    return _decodeObject(api.calibrationCancel(_core));
  }

  Map<String, Object?> calibrationStatus() {
    return _decodeObject(api.calibrationStatus(_core));
  }

  Map<String, Object?> calibrationHistory(int type) {
    return _decodeObject(api.calibrationHistory(_core, type));
  }

  Map<String, Object?> calibrationRetryInfo() {
    return _decodeObject(api.calibrationRetryInfo(_core));
  }

  Map<String, Object?> confirmAccelerometerOrientation(int position) {
    return _decodeObject(api.calibrationAccelConfirm(_core, position));
  }

  Map<String, Object?> startAirspeedCalibration() {
    return _decodeObject(api.calibrationStartAirspeed(_core));
  }

  Map<String, Object?> highresImu() => _decodeObject(api.highresImu(_core));

  Map<String, Object?> serialUartNames() {
    return _decodeObject(api.serialUartNames(_core));
  }

  Map<String, Object?> missionDownload() =>
      _decodeObject(api.missionDownload(_core));

  Map<String, Object?> missionUpload(List<Map<String, Object?>> items) {
    return _decodeObject(api.missionUpload(_core, jsonEncode(items)));
  }

  Map<String, Object?> missionClear() => _decodeObject(api.missionClear(_core));

  Map<String, Object?> missionSetCurrent(int sequence) =>
      _decodeObject(api.missionSetCurrent(_core, sequence));

  Map<String, Object?> missionStart() => _decodeObject(api.missionStart(_core));

  Map<String, Object?> missionPlan() => _decodeObject(api.missionPlan(_core));

  Map<String, Object?> missionProgress() =>
      _decodeObject(api.missionProgress(_core));

  Map<String, Object?> surveyGenerate(Map<String, Object?> config) =>
      _decodeObject(api.surveyGenerate(_core, jsonEncode(config)));

  Map<String, Object?> surveyGenerateCorridor(Map<String, Object?> config) =>
      _decodeObject(api.surveyGenerateCorridor(_core, jsonEncode(config)));

  Map<String, Object?> geofenceGetConfig() =>
      _decodeObject(api.geofenceGetConfig(_core));
  Map<String, Object?> geofenceSetConfig(Map<String, Object?> config) =>
      _decodeObject(api.geofenceSetConfig(_core, jsonEncode(config)));
  Map<String, Object?> geofenceAddPoint(double lat, double lon, double alt) =>
      _decodeObject(api.geofenceAddPoint(_core, lat, lon, alt));
  Map<String, Object?> geofenceRemovePoint(int index) =>
      _decodeObject(api.geofenceRemovePoint(_core, index));
  List<Object?> geofenceGetPoints() {
    final value = jsonDecode(api.geofenceGetPoints(_core));
    return value is List<Object?> ? value : const [];
  }

  Map<String, Object?> geofenceValidate() =>
      _decodeObject(api.geofenceValidate(_core));
  String geofenceExportJson() => api.geofenceExportJson(_core);
  Map<String, Object?> geofenceImportJson(String json) =>
      _decodeObject(api.geofenceImportJson(_core, json));
  String geofenceExportKml(String name) => api.geofenceExportKml(_core, name);
  Map<String, Object?> fenceDownload() =>
      _decodeObject(api.fenceDownload(_core));
  Map<String, Object?> fenceGetPlan() => _decodeObject(api.fenceGetPlan(_core));
  Map<String, Object?> fenceClear() => _decodeObject(api.fenceClear(_core));
  Map<String, Object?> fenceEnable(bool enabled) =>
      _decodeObject(api.fenceEnable(_core, enabled ? 1 : 0));
  Map<String, Object?> rallyDownload() =>
      _decodeObject(api.rallyDownload(_core));
  Map<String, Object?> rallyGetPlan() => _decodeObject(api.rallyGetPlan(_core));
  Map<String, Object?> rallyClear() => _decodeObject(api.rallyClear(_core));

  Map<String, Object?> logRequestList() =>
      _decodeObject(api.logRequestList(_core));

  List<Object?> logGetEntries() {
    final value = jsonDecode(api.logGetEntries(_core));
    return value is List<Object?> ? value : const [];
  }

  Map<String, Object?> logDownload(int logId, String filepath) =>
      _decodeObject(api.logDownload(_core, logId, filepath));

  Map<String, Object?> logDownloadCancel() =>
      _decodeObject(api.logDownloadCancel(_core));

  Map<String, Object?> logDownloadStatus() =>
      _decodeObject(api.logDownloadStatus(_core));

  Map<String, Object?> listVehicleDirectory(String path) =>
      _decodeObject(api.fsListDirectory(_core, path));

  Map<String, Object?> downloadVehicleFile(
    String remotePath,
    String localPath,
  ) => _decodeObject(api.fsDownloadFile(_core, remotePath, localPath));

  Map<String, Object?> cancelVehicleFileDownload() =>
      _decodeObject(api.fsDownloadCancel(_core));

  Map<String, Object?> uploadVehicleFile(String localPath, String remotePath) =>
      _decodeObject(api.fsUploadFile(_core, localPath, remotePath));

  Map<String, Object?> firmwareType() {
    return _decodeObject(api.firmwareType(_core));
  }

  Map<String, Object?> firmwareVersion() {
    return _decodeObject(api.firmwareVersion(_core));
  }

  Map<String, Object?> systemDiagnostics() {
    return _decodeObject(api.systemDiagnostics(_core));
  }

  Map<String, Object?> commandArm({bool force = false}) =>
      _decodeObject(api.commandArm(_core, force));
  Map<String, Object?> commandDisarm({bool force = false}) =>
      _decodeObject(api.commandDisarm(_core, force));
  Map<String, Object?> commandSetMode(String mode) =>
      _decodeObject(api.commandSetModeByName(_core, mode));
  Map<String, Object?> commandRtl() => _decodeObject(api.commandRtl(_core));
  Map<String, Object?> commandTakeoff(double altitude) =>
      _decodeObject(api.commandTakeoff(_core, altitude));
  Map<String, Object?> commandLand() => _decodeObject(api.commandLand(_core));
  Map<String, Object?> commandEmergencyStop() =>
      _decodeObject(api.commandEmergencyStop(_core));
  Map<String, Object?> commandGuidedPoint(
    double latitude,
    double longitude,
    double altitude,
    double yaw,
    double speed,
  ) => _decodeObject(
    api.commandGuidedPoint(_core, latitude, longitude, altitude, yaw, speed),
  );
  List<Object?> flightAvailableModes() {
    final value = jsonDecode(api.flightAvailableModes(_core));
    return value is List<Object?> ? value : const [];
  }

  Map<String, Object?> flightModeStatus() =>
      _decodeObject(api.flightModeStatus(_core));
  Map<String, Object?> preflightCheck() =>
      _decodeObject(api.preflightCheck(_core));

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    api.setEventCallback(
      _core,
      nullptr.cast<NativeFunction<MgCoreEventNative>>(),
    );
    api.destroy(_core);
    _eventCallback.close();
    _events.close();
  }

  void _onNativeEvent(int type, Pointer<Utf8> json, Pointer<Void> _) {
    try {
      final decoded = _decodeObject(json.toDartString());
      if (!_disposed) {
        _events.add(MgCoreEvent(type: type, payload: decoded));
      }
    } catch (_) {
      // Invalid native event payloads must not take down the UI isolate.
    } finally {
      api.freeString(json);
    }
  }
}

final class MgCoreEvent {
  const MgCoreEvent({required this.type, required this.payload});

  final int type;
  final Map<String, Object?> payload;
}

Map<String, Object?> _decodeObject(String jsonText) {
  if (jsonText.isEmpty) {
    return const {};
  }
  final decoded = jsonDecode(jsonText);
  if (decoded is Map<String, Object?>) {
    return decoded;
  }
  if (decoded is List<Object?>) {
    return <String, Object?>{'items': decoded};
  }
  return <String, Object?>{'value': decoded};
}
