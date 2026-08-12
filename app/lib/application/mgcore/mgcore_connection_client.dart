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

  Map<String, Object?> connectSerial(String port, int baudRate) {
    return _decodeObject(api.connectSerial(_core, port, baudRate));
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

  Map<String, Object?> missionPlan() => _decodeObject(api.missionPlan(_core));

  Map<String, Object?> missionProgress() =>
      _decodeObject(api.missionProgress(_core));

  Map<String, Object?> firmwareType() {
    return _decodeObject(api.firmwareType(_core));
  }

  Map<String, Object?> firmwareVersion() {
    return _decodeObject(api.firmwareVersion(_core));
  }

  Map<String, Object?> systemDiagnostics() {
    return _decodeObject(api.systemDiagnostics(_core));
  }

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
