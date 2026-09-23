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
    _vehicleList = _jsonFunction('mg_vehicle_list');
    _vehicleSelect = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_vehicle_select');
    _vehicleClose = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_vehicle_close');
    _vehicleGetActive = _library
        .lookupFunction<
          Int32 Function(Pointer<Void>),
          int Function(Pointer<Void>)
        >('mg_vehicle_get_active');
    _connectSerial = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, int)
        >('mg_connection_connect');
    _connectTcp = _stringArgumentFunction('mg_connection_connect_tcp');
    _connectUdp = _stringArgumentFunction('mg_connection_connect_udp');
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
    _missionClear = _jsonFunction('mg_mission_clear');
    _missionSetCurrent = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_mission_set_current');
    _missionStart = _jsonFunction('mg_mission_start');
    _missionPlan = _jsonFunction('mg_mission_get_plan');
    _missionProgress = _jsonFunction('mg_mission_get_progress');
    _surveyGenerate = _stringArgumentFunction('mg_survey_generate');
    _surveyGenerateCorridor = _stringArgumentFunction(
      'mg_survey_generate_corridor',
    );
    _geofenceGetConfig = _jsonFunction('mg_geofence_get_config');
    _geofenceSetConfig = _stringArgumentFunction('mg_geofence_set_config');
    _geofenceAddPoint = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Double, Double, Float),
          Pointer<Utf8> Function(Pointer<Void>, double, double, double)
        >('mg_geofence_add_point');
    _geofenceRemovePoint = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_geofence_remove_point');
    _geofenceGetPoints = _jsonFunction('mg_geofence_get_points');
    _geofenceValidate = _jsonFunction('mg_geofence_validate');
    _geofenceExportJson = _jsonFunction('mg_geofence_export_json');
    _geofenceImportJson = _stringArgumentFunction('mg_geofence_import_json');
    _geofenceExportKml = _stringArgumentFunction('mg_geofence_export_kml');
    _fenceDownload = _jsonFunction('mg_fence_download');
    _fenceGetPlan = _jsonFunction('mg_fence_get_plan');
    _fenceClear = _jsonFunction('mg_fence_clear');
    _fenceEnable = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_fence_enable');
    _rallyDownload = _jsonFunction('mg_rally_download');
    _rallyGetPlan = _jsonFunction('mg_rally_get_plan');
    _rallyClear = _jsonFunction('mg_rally_clear');
    _logRequestList = _jsonFunction('mg_log_request_list');
    _logGetEntries = _jsonFunction('mg_log_get_entries');
    _logDownload = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>),
          Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
        >('mg_log_download');
    _logDownloadCancel = _jsonFunction('mg_log_download_cancel');
    _logDownloadStatus = _jsonFunction('mg_log_download_status');
    _fsListDirectory = _stringArgumentFunction('mg_fs_list_directory');
    _fsDownloadFile = _twoStringArgumentFunction('mg_fs_download_file');
    _fsUploadFile = _twoStringArgumentFunction('mg_fs_upload_file');
    _fsDownloadCancel = _jsonFunction('mg_fs_download_cancel');
    _firmwareType = _jsonFunction('mg_firmware_get_type');
    _firmwareVersion = _jsonFunction('mg_vehicle_get_firmware_version');
    _systemDiagnostics = _jsonFunction('mg_system_get_diagnostics');
    _commandArm = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_command_arm');
    _commandDisarm = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int)
        >('mg_command_disarm');
    _commandSetMode = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32, Int32),
          Pointer<Utf8> Function(Pointer<Void>, int, int)
        >('mg_command_set_mode');
    _commandSetModeByName = _stringArgumentFunction('mg_cmd_set_mode');
    _commandRtl = _jsonFunction('mg_command_rtl');
    _commandTakeoff = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Float),
          Pointer<Utf8> Function(Pointer<Void>, double)
        >('mg_command_takeoff');
    _commandLand = _jsonFunction('mg_command_land');
    _commandEmergencyStop = _jsonFunction('mg_command_emergency_stop');
    _commandGuidedPoint = _library
        .lookupFunction<
          Pointer<Utf8> Function(
            Pointer<Void>,
            Double,
            Double,
            Float,
            Float,
            Float,
          ),
          Pointer<Utf8> Function(
            Pointer<Void>,
            double,
            double,
            double,
            double,
            double,
          )
        >('mg_command_guided_point');
    _flightAvailableModes = _jsonFunction('mg_flight_get_available_modes');
    _flightModeStatus = _jsonFunction('mg_flight_get_mode_status');
    _preflightCheck = _jsonFunction('mg_preflight_check');
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
  late final Pointer<Utf8> Function(Pointer<Void>) _vehicleList;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _vehicleSelect;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _vehicleClose;
  late final int Function(Pointer<Void>) _vehicleGetActive;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, int)
  _connectSerial;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _connectTcp;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _connectUdp;
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
  late final Pointer<Utf8> Function(Pointer<Void>) _missionClear;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _missionSetCurrent;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionStart;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionPlan;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionProgress;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _surveyGenerate;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _surveyGenerateCorridor;
  late final Pointer<Utf8> Function(Pointer<Void>) _geofenceGetConfig;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _geofenceSetConfig;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double, double)
  _geofenceAddPoint;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _geofenceRemovePoint;
  late final Pointer<Utf8> Function(Pointer<Void>) _geofenceGetPoints;
  late final Pointer<Utf8> Function(Pointer<Void>) _geofenceValidate;
  late final Pointer<Utf8> Function(Pointer<Void>) _geofenceExportJson;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _geofenceImportJson;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _geofenceExportKml;
  late final Pointer<Utf8> Function(Pointer<Void>) _fenceDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _fenceGetPlan;
  late final Pointer<Utf8> Function(Pointer<Void>) _fenceClear;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _fenceEnable;
  late final Pointer<Utf8> Function(Pointer<Void>) _rallyDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _rallyGetPlan;
  late final Pointer<Utf8> Function(Pointer<Void>) _rallyClear;
  late final Pointer<Utf8> Function(Pointer<Void>) _logRequestList;
  late final Pointer<Utf8> Function(Pointer<Void>) _logGetEntries;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
  _logDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _logDownloadCancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _logDownloadStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _fsListDirectory;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  _fsDownloadFile;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  _fsUploadFile;
  late final Pointer<Utf8> Function(Pointer<Void>) _fsDownloadCancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _firmwareType;
  late final Pointer<Utf8> Function(Pointer<Void>) _firmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _systemDiagnostics;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _commandArm;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _commandDisarm;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _commandSetMode;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _commandSetModeByName;
  late final Pointer<Utf8> Function(Pointer<Void>) _commandRtl;
  late final Pointer<Utf8> Function(Pointer<Void>, double) _commandTakeoff;
  late final Pointer<Utf8> Function(Pointer<Void>) _commandLand;
  late final Pointer<Utf8> Function(Pointer<Void>) _commandEmergencyStop;
  late final Pointer<Utf8> Function(
    Pointer<Void>,
    double,
    double,
    double,
    double,
    double,
  )
  _commandGuidedPoint;
  late final Pointer<Utf8> Function(Pointer<Void>) _flightAvailableModes;
  late final Pointer<Utf8> Function(Pointer<Void>) _flightModeStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _preflightCheck;
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
  String vehicleList(Pointer<Void> core) => _readString(_vehicleList(core));
  String vehicleSelect(Pointer<Void> core, int vehicleId) =>
      _readString(_vehicleSelect(core, vehicleId));
  String vehicleClose(Pointer<Void> core, int vehicleId) =>
      _readString(_vehicleClose(core, vehicleId));
  int vehicleGetActive(Pointer<Void> core) => _vehicleGetActive(core);

  String connectSerial(Pointer<Void> core, String port, int baudRate) {
    final nativePort = port.toNativeUtf8();
    try {
      return _readString(_connectSerial(core, nativePort, baudRate));
    } finally {
      malloc.free(nativePort);
    }
  }

  String connectTcp(Pointer<Void> core, String endpoint) {
    return _callWithString(_connectTcp, core, endpoint);
  }

  String connectUdp(Pointer<Void> core, String endpoint) {
    return _callWithString(_connectUdp, core, endpoint);
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

  String missionClear(Pointer<Void> core) => _readString(_missionClear(core));

  String missionSetCurrent(Pointer<Void> core, int sequence) =>
      _readString(_missionSetCurrent(core, sequence));

  String missionStart(Pointer<Void> core) => _readString(_missionStart(core));

  String missionPlan(Pointer<Void> core) => _readString(_missionPlan(core));

  String missionProgress(Pointer<Void> core) =>
      _readString(_missionProgress(core));

  String surveyGenerate(Pointer<Void> core, String configJson) =>
      _callWithString(_surveyGenerate, core, configJson);

  String surveyGenerateCorridor(Pointer<Void> core, String configJson) =>
      _callWithString(_surveyGenerateCorridor, core, configJson);

  String geofenceGetConfig(Pointer<Void> core) =>
      _readString(_geofenceGetConfig(core));
  String geofenceSetConfig(Pointer<Void> core, String json) =>
      _callWithString(_geofenceSetConfig, core, json);
  String geofenceAddPoint(
    Pointer<Void> core,
    double lat,
    double lon,
    double alt,
  ) => _readString(_geofenceAddPoint(core, lat, lon, alt));
  String geofenceRemovePoint(Pointer<Void> core, int index) =>
      _readString(_geofenceRemovePoint(core, index));
  String geofenceGetPoints(Pointer<Void> core) =>
      _readString(_geofenceGetPoints(core));
  String geofenceValidate(Pointer<Void> core) =>
      _readString(_geofenceValidate(core));
  String geofenceExportJson(Pointer<Void> core) =>
      _readString(_geofenceExportJson(core));
  String geofenceImportJson(Pointer<Void> core, String json) =>
      _callWithString(_geofenceImportJson, core, json);
  String geofenceExportKml(Pointer<Void> core, String name) =>
      _callWithString(_geofenceExportKml, core, name);
  String fenceDownload(Pointer<Void> core) => _readString(_fenceDownload(core));
  String fenceGetPlan(Pointer<Void> core) => _readString(_fenceGetPlan(core));
  String fenceClear(Pointer<Void> core) => _readString(_fenceClear(core));
  String fenceEnable(Pointer<Void> core, int enabled) =>
      _readString(_fenceEnable(core, enabled));
  String rallyDownload(Pointer<Void> core) => _readString(_rallyDownload(core));
  String rallyGetPlan(Pointer<Void> core) => _readString(_rallyGetPlan(core));
  String rallyClear(Pointer<Void> core) => _readString(_rallyClear(core));

  String logRequestList(Pointer<Void> core) =>
      _readString(_logRequestList(core));

  String logGetEntries(Pointer<Void> core) => _readString(_logGetEntries(core));

  String logDownload(Pointer<Void> core, int logId, String filepath) {
    final nativePath = filepath.toNativeUtf8();
    try {
      return _readString(_logDownload(core, logId, nativePath));
    } finally {
      malloc.free(nativePath);
    }
  }

  String logDownloadCancel(Pointer<Void> core) =>
      _readString(_logDownloadCancel(core));

  String logDownloadStatus(Pointer<Void> core) =>
      _readString(_logDownloadStatus(core));

  String fsListDirectory(Pointer<Void> core, String path) =>
      _callWithString(_fsListDirectory, core, path);

  String fsDownloadFile(
    Pointer<Void> core,
    String remotePath,
    String localPath,
  ) {
    final nativeRemotePath = remotePath.toNativeUtf8();
    final nativeLocalPath = localPath.toNativeUtf8();
    try {
      return _readString(
        _fsDownloadFile(core, nativeRemotePath, nativeLocalPath),
      );
    } finally {
      malloc.free(nativeRemotePath);
      malloc.free(nativeLocalPath);
    }
  }

  String fsDownloadCancel(Pointer<Void> core) =>
      _readString(_fsDownloadCancel(core));

  String fsUploadFile(Pointer<Void> core, String localPath, String remotePath) {
    final nativeLocalPath = localPath.toNativeUtf8();
    final nativeRemotePath = remotePath.toNativeUtf8();
    try {
      return _readString(
        _fsUploadFile(core, nativeLocalPath, nativeRemotePath),
      );
    } finally {
      malloc.free(nativeLocalPath);
      malloc.free(nativeRemotePath);
    }
  }

  String firmwareType(Pointer<Void> core) {
    return _readString(_firmwareType(core));
  }

  String firmwareVersion(Pointer<Void> core) {
    return _readString(_firmwareVersion(core));
  }

  String systemDiagnostics(Pointer<Void> core) {
    return _readString(_systemDiagnostics(core));
  }

  String commandArm(Pointer<Void> core, bool force) =>
      _readString(_commandArm(core, force ? 1 : 0));
  String commandDisarm(Pointer<Void> core, bool force) =>
      _readString(_commandDisarm(core, force ? 1 : 0));
  String commandSetMode(Pointer<Void> core, int baseMode, int customMode) =>
      _readString(_commandSetMode(core, baseMode, customMode));
  String commandSetModeByName(Pointer<Void> core, String mode) =>
      _callWithString(_commandSetModeByName, core, mode);
  String commandRtl(Pointer<Void> core) => _readString(_commandRtl(core));
  String commandTakeoff(Pointer<Void> core, double altitude) =>
      _readString(_commandTakeoff(core, altitude));
  String commandLand(Pointer<Void> core) => _readString(_commandLand(core));
  String commandEmergencyStop(Pointer<Void> core) =>
      _readString(_commandEmergencyStop(core));
  String commandGuidedPoint(
    Pointer<Void> core,
    double lat,
    double lon,
    double alt,
    double yaw,
    double speed,
  ) => _readString(_commandGuidedPoint(core, lat, lon, alt, yaw, speed));
  String flightAvailableModes(Pointer<Void> core) =>
      _readString(_flightAvailableModes(core));
  String flightModeStatus(Pointer<Void> core) =>
      _readString(_flightModeStatus(core));
  String preflightCheck(Pointer<Void> core) =>
      _readString(_preflightCheck(core));

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

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  _twoStringArgumentFunction(String symbol) {
    return _library.lookupFunction<
      Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>),
      Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
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
    if (Platform.isLinux) '$executableDirectory/mgcore.so',
    if (Platform.isLinux) '$executableDirectory/libmgcore.so',
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
    if (Platform.isLinux) 'mgcore.so',
    if (Platform.isLinux) 'libmgcore.so',
    if (Platform.isMacOS) '../build/mgcore.dylib',
    if (Platform.isMacOS) '../../build/mgcore.dylib',
    if (Platform.isWindows) r'..\build\mgcore.dll',
    if (Platform.isWindows) r'..\..\build\mgcore.dll',
    if (Platform.isLinux) '../build/mgcore.so',
    if (Platform.isLinux) '../build/libmgcore.so',
    if (Platform.isLinux) '../../build/mgcore.so',
    if (Platform.isLinux) '../../build/libmgcore.so',
  ];

  for (final candidate in candidates) {
    if (File(candidate).existsSync()) {
      return candidate;
    }
  }

  return candidates.first;
}
