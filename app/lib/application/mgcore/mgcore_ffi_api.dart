import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

typedef MgCoreEventNative =
    Void Function(Int32 type, Pointer<Utf8> json, Pointer<Void> userData);

final class MgCoreFfiApi {
  MgCoreFfiApi._(this._library) {
    _create = _library
        .lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>(
          'mg_create',
        );
    _destroy = _library
        .lookupFunction<
          Void Function(Pointer<Void>),
          void Function(Pointer<Void>)
        >('mg_destroy');
    _freeString = _library
        .lookupFunction<
          Void Function(Pointer<Utf8>),
          void Function(Pointer<Utf8>)
        >('mg_free_string');
    _listPorts = _jsonFunction('mg_connection_list_ports');
    _connectSerial = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, int)
        >('mg_connection_connect');
    _disconnect = _jsonFunction('mg_connection_disconnect');
    _connectionDiagnostics = _jsonFunction('mg_connection_get_diagnostics');
    _dashboardSnapshot = _jsonFunction('mg_telemetry_get_dashboard_snapshot');
    _requestAllParameters = _jsonFunction('mg_params_request_all');
    _parameterProgress = _jsonFunction('mg_params_get_progress');
    _allParameters = _jsonFunction('mg_params_get_all');
    _prepareParameterChangeSet = _stringArgumentFunction(
      'mg_params_prepare_change_set',
    );
    _commitParameterChangeSet = _stringArgumentFunction(
      'mg_params_commit_change_set',
    );
    _parameterFileExport = _stringArgumentFunction('mg_param_file_export');
    _parameterFileImport = _stringArgumentFunction('mg_param_file_import');
    _setParameter = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Float, Int32),
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double, int)
        >('mg_params_set');
    _readParameterDescriptor = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>),
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
        >('mg_config_read_param');
    _parameterDescriptorGroups = _jsonFunction('mg_config_get_groups');
    _parameterDescriptorGroup = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>),
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
        >('mg_config_get_group_descriptor');
    _remoteControlChannels = _jsonFunction('mg_rc_get_channels');
    _servoOutputs = _jsonFunction('mg_servo_get_outputs');
    _calibrationStart = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int, int)
        >('mg_calibration_start');
    _calibrationCancel = _jsonFunction('mg_calibration_cancel');
    _calibrationStatus = _jsonFunction('mg_calibration_get_status');
    _calibrationHistory = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_calibration_get_history');
    _calibrationRetryInfo = _jsonFunction('mg_calibration_get_retry_info');
    _calibrationAccelConfirm = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_calibration_accel_confirm');
    _calibrationStartAirspeed = _jsonFunction('mg_calibration_start_airspeed');
    _highresImu = _jsonFunction('mg_vehicle_get_highres_imu');
    _serialUartNames = _jsonFunction('mg_serial_get_uart_names');
    _missionDownload = _jsonFunction('mg_mission_download');
    _missionUpload = _stringArgumentFunction('mg_mission_upload');
    _missionPlan = _jsonFunction('mg_mission_get_plan');
    _missionProgress = _jsonFunction('mg_mission_get_progress');
    _firmwareType = _jsonFunction('mg_firmware_get_type');
    _firmwareVersion = _jsonFunction('mg_vehicle_get_firmware_version');
    _systemDiagnostics = _jsonFunction('mg_system_get_diagnostics');
    _setEventCallback = _library
        .lookupFunction<
          Void Function(
            Pointer<Void>,
            Pointer<NativeFunction<MgCoreEventNative>>,
            Pointer<Void>,
          ),
          void Function(
            Pointer<Void>,
            Pointer<NativeFunction<MgCoreEventNative>>,
            Pointer<Void>,
          )
        >('mg_set_event_callback');
  }

  factory MgCoreFfiApi.open() {
    return MgCoreFfiApi._(DynamicLibrary.open(_resolveLibraryPath()));
  }

  final DynamicLibrary _library;

  late final Pointer<Void> Function() _create;
  late final void Function(Pointer<Void>) _destroy;
  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _listPorts;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, int)
  _connectSerial;
  late final Pointer<Utf8> Function(Pointer<Void>) _disconnect;
  late final Pointer<Utf8> Function(Pointer<Void>) _connectionDiagnostics;
  late final Pointer<Utf8> Function(Pointer<Void>) _dashboardSnapshot;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestAllParameters;
  late final Pointer<Utf8> Function(Pointer<Void>) _parameterProgress;
  late final Pointer<Utf8> Function(Pointer<Void>) _allParameters;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _prepareParameterChangeSet;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _commitParameterChangeSet;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _parameterFileExport;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _parameterFileImport;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double, int)
  _setParameter;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _readParameterDescriptor;
  late final Pointer<Utf8> Function(Pointer<Void>) _parameterDescriptorGroups;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _parameterDescriptorGroup;
  late final Pointer<Utf8> Function(Pointer<Void>) _remoteControlChannels;
  late final Pointer<Utf8> Function(Pointer<Void>) _servoOutputs;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _calibrationStart;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationCancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _calibrationHistory;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationRetryInfo;
  late final Pointer<Utf8> Function(Pointer<Void>, int)
  _calibrationAccelConfirm;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationStartAirspeed;
  late final Pointer<Utf8> Function(Pointer<Void>) _highresImu;
  late final Pointer<Utf8> Function(Pointer<Void>) _serialUartNames;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDownload;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionUpload;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionPlan;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionProgress;
  late final Pointer<Utf8> Function(Pointer<Void>) _firmwareType;
  late final Pointer<Utf8> Function(Pointer<Void>) _firmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _systemDiagnostics;
  late final void Function(
    Pointer<Void>,
    Pointer<NativeFunction<MgCoreEventNative>>,
    Pointer<Void>,
  )
  _setEventCallback;

  Pointer<Void> create() => _create();

  void destroy(Pointer<Void> core) {
    if (core != nullptr) {
      _destroy(core);
    }
  }

  String listPorts(Pointer<Void> core) => _readString(_listPorts(core));

  String connectSerial(Pointer<Void> core, String port, int baudRate) {
    final nativePort = port.toNativeUtf8();
    try {
      return _readString(_connectSerial(core, nativePort, baudRate));
    } finally {
      malloc.free(nativePort);
    }
  }

  String disconnect(Pointer<Void> core) => _readString(_disconnect(core));

  String connectionDiagnostics(Pointer<Void> core) {
    return _readString(_connectionDiagnostics(core));
  }

  String dashboardSnapshot(Pointer<Void> core) {
    return _readString(_dashboardSnapshot(core));
  }

  String requestAllParameters(Pointer<Void> core) {
    return _readString(_requestAllParameters(core));
  }

  String parameterProgress(Pointer<Void> core) {
    return _readString(_parameterProgress(core));
  }

  String allParameters(Pointer<Void> core) => _readString(_allParameters(core));

  String prepareParameterChangeSet(Pointer<Void> core, String json) {
    return _callWithString(_prepareParameterChangeSet, core, json);
  }

  String commitParameterChangeSet(Pointer<Void> core, String json) {
    return _callWithString(_commitParameterChangeSet, core, json);
  }

  String exportParameterFile(Pointer<Void> core, String json) {
    return _callWithString(_parameterFileExport, core, json);
  }

  String importParameterFile(Pointer<Void> core, String content) {
    return _callWithString(_parameterFileImport, core, content);
  }

  String setParameter(Pointer<Void> core, String name, double value, int type) {
    final nativeName = name.toNativeUtf8();
    try {
      return _readString(_setParameter(core, nativeName, value, type));
    } finally {
      malloc.free(nativeName);
    }
  }

  String readParameterDescriptor(Pointer<Void> core, String name) {
    final nativeName = name.toNativeUtf8();
    try {
      return _readString(_readParameterDescriptor(core, nativeName));
    } finally {
      malloc.free(nativeName);
    }
  }

  String parameterDescriptorGroups(Pointer<Void> core) {
    return _readString(_parameterDescriptorGroups(core));
  }

  String parameterDescriptorGroup(Pointer<Void> core, String name) {
    final nativeName = name.toNativeUtf8();
    try {
      return _readString(_parameterDescriptorGroup(core, nativeName));
    } finally {
      malloc.free(nativeName);
    }
  }

  String remoteControlChannels(Pointer<Void> core) {
    return _readString(_remoteControlChannels(core));
  }

  String servoOutputs(Pointer<Void> core) {
    return _readString(_servoOutputs(core));
  }

  String calibrationStart(Pointer<Void> core, int type, int mode) {
    return _readString(_calibrationStart(core, type, mode));
  }

  String calibrationCancel(Pointer<Void> core) {
    return _readString(_calibrationCancel(core));
  }

  String calibrationStatus(Pointer<Void> core) {
    return _readString(_calibrationStatus(core));
  }

  String calibrationHistory(Pointer<Void> core, int type) {
    return _readString(_calibrationHistory(core, type));
  }

  String calibrationRetryInfo(Pointer<Void> core) {
    return _readString(_calibrationRetryInfo(core));
  }

  String calibrationAccelConfirm(Pointer<Void> core, int position) {
    return _readString(_calibrationAccelConfirm(core, position));
  }

  String calibrationStartAirspeed(Pointer<Void> core) {
    return _readString(_calibrationStartAirspeed(core));
  }

  String highresImu(Pointer<Void> core) => _readString(_highresImu(core));

  String serialUartNames(Pointer<Void> core) {
    return _readString(_serialUartNames(core));
  }

  String missionDownload(Pointer<Void> core) =>
      _readString(_missionDownload(core));

  String missionUpload(Pointer<Void> core, String itemsJson) {
    return _callWithString(_missionUpload, core, itemsJson);
  }

  String missionPlan(Pointer<Void> core) => _readString(_missionPlan(core));

  String missionProgress(Pointer<Void> core) =>
      _readString(_missionProgress(core));

  String firmwareType(Pointer<Void> core) {
    return _readString(_firmwareType(core));
  }

  String firmwareVersion(Pointer<Void> core) {
    return _readString(_firmwareVersion(core));
  }

  String systemDiagnostics(Pointer<Void> core) {
    return _readString(_systemDiagnostics(core));
  }

  void setEventCallback(
    Pointer<Void> core,
    Pointer<NativeFunction<MgCoreEventNative>> callback,
  ) {
    _setEventCallback(core, callback, nullptr);
  }

  void freeString(Pointer<Utf8> pointer) {
    if (pointer != nullptr) {
      _freeString(pointer);
    }
  }

  Pointer<Utf8> Function(Pointer<Void>) _jsonFunction(String symbol) {
    return _library.lookupFunction<
      Pointer<Utf8> Function(Pointer<Void>),
      Pointer<Utf8> Function(Pointer<Void>)
    >(symbol);
  }

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _stringArgumentFunction(
    String symbol,
  ) {
    return _library.lookupFunction<
      Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>),
      Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
    >(symbol);
  }

  String _callWithString(
    Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) function,
    Pointer<Void> core,
    String value,
  ) {
    final nativeValue = value.toNativeUtf8();
    try {
      return _readString(function(core, nativeValue));
    } finally {
      malloc.free(nativeValue);
    }
  }

  String _readString(Pointer<Utf8> pointer) {
    if (pointer == nullptr) {
      return '';
    }
    try {
      return pointer.toDartString();
    } finally {
      freeString(pointer);
    }
  }
}

String _resolveLibraryPath() {
  final envPath = Platform.environment['MGCORE_LIBRARY_PATH'];
  if (envPath != null && envPath.isNotEmpty && File(envPath).existsSync()) {
    return envPath;
  }

  final executableDirectory = File(Platform.resolvedExecutable).parent.path;
  final bundledCandidates = <String>[
    if (Platform.isMacOS) '$executableDirectory/../Frameworks/mgcore.dylib',
    if (Platform.isWindows) '$executableDirectory/mgcore.dll',
  ];
  for (final candidate in bundledCandidates) {
    if (File(candidate).existsSync()) {
      return candidate;
    }
  }

  final candidates = <String>[
    if (Platform.isMacOS) 'mgcore.dylib',
    if (Platform.isMacOS) 'libmgcore.dylib',
    if (Platform.isWindows) 'mgcore.dll',
    if (Platform.isWindows) 'libmgcore.dll',
    if (Platform.isMacOS) '../build/mgcore.dylib',
    if (Platform.isMacOS) '../../build/mgcore.dylib',
    if (Platform.isWindows) r'..\build\mgcore.dll',
    if (Platform.isWindows) r'..\..\build\mgcore.dll',
  ];

  for (final candidate in candidates) {
    if (File(candidate).existsSync()) {
      return candidate;
    }
  }

  return candidates.first;
}
