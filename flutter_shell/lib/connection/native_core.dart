import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

/// mgcore 共享库的加载、符号绑定与生命周期。
///
/// 全应用只有这里持有「原生库 + 核心句柄」。业务层（控制器、页面）调用这里的
/// 类型化方法，不再出现 `Pointer` / `DynamicLibrary` / `calloc`；
/// 各 domain backend 仍按 `architecture.md` 的 A5 约定自行绑定自己那一组符号，
/// 通过 [library] / [handle] 取得入口。
///
/// 返回值统一是「已解析的 JSON」或 `void`：C ABI 返回的 `char*` 由本类负责用
/// `mg_free_string` 释放，调用方不需要关心所有权（对应 `ffi-contract.md` 的 F3）。
class NativeCore {
  NativeCore._(this._library) {
    _create = _library
        .lookup<NativeFunction<Pointer<Void> Function()>>('mg_create')
        .asFunction();
    _destroy = _library
        .lookup<NativeFunction<Void Function(Pointer<Void>)>>('mg_destroy')
        .asFunction();
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>(
          'mg_free_string',
        )
        .asFunction();
    _listPorts = _bind1('mg_connection_list_ports');
    _getSessionState = _bindInt1('mg_connection_get_session_state');
    _clearSigningKey = _bindInt1('mg_vehicle_clear_signing_key');
    _clearSigningCandidates = _bindInt1('mg_vehicle_clear_signing_candidates');
    _getSigningStatus = _bindInt1('mg_vehicle_get_signing_status');
    _getWaveformInstances = _bind1('mg_waveform_get_instances');
    _requestFirmwareVersion = _bind1('mg_vehicle_request_firmware_version');
    _getSerialUartNames = _bind1('mg_serial_get_uart_names');
    _getFlightModes = _bind1('mg_flight_get_modes');
    _getMessages = _bindUint64('mg_telemetry_get_messages');
    _setSigningCandidates = _bindText1('mg_vehicle_set_signing_candidates');
    _cancelOperationForVehicle = _bindText1('mg_operations_cancel_for_vehicle');
    _setStreamInterval = _bindInt2('mg_stream_set_interval');
    _setSigningKey = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>, Int32, Uint64),
          Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>, int, int)
        >('mg_vehicle_set_signing_key');
    _sendSetupSigning = _library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>, Uint64),
          Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>, int)
        >('mg_vehicle_send_setup_signing');
    _readWaveform = _library
        .lookupFunction<
          Int32 Function(Pointer<Void>, Pointer<Float>, Int32, Int32, Int32, Int32),
          int Function(Pointer<Void>, Pointer<Float>, int, int, int, int)
        >('mg_waveform_poll');
    _handle = _create();
  }

  /// 打开当前平台对应的 mgcore 共享库并绑定符号。
  ///
  /// 找不到库或符号缺失时抛 [StateError]，调用方负责转成用户可见的错误。
  factory NativeCore.open() => NativeCore._(_openPlatformLibrary());

  final DynamicLibrary _library;
  late final Pointer<Void> _handle;

  late final Pointer<Void> Function() _create;
  late final void Function(Pointer<Void>) _destroy;
  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _listPorts;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getSessionState;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _clearSigningKey;
  late final Pointer<Utf8> Function(Pointer<Void>, int)
  _clearSigningCandidates;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getSigningStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _getWaveformInstances;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestFirmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _getSerialUartNames;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFlightModes;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getMessages;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
  _setSigningCandidates;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
  _cancelOperationForVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _setStreamInterval;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>, int, int)
  _setSigningKey;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>, int)
  _sendSetupSigning;
  late final int Function(Pointer<Void>, Pointer<Float>, int, int, int, int)
  _readWaveform;

  Pointer<Float>? _waveformBuffer;
  int _waveformCapacity = 0;

  /// 原生核心句柄，供各 domain backend 构造使用。
  Pointer<Void> get handle => _handle;

  /// 原生库句柄，供各 domain backend 绑定自己的符号使用。
  DynamicLibrary get library => _library;

  // ================================================================
  // 连接
  // ================================================================

  /// 枚举本机串口。
  Object? listPorts() => _readJson(_listPorts(_handle));

  /// 读取某条会话的连接状态（心跳、阶段、焦点车辆等）。
  Object? sessionState(int sessionId) =>
      _readJson(_getSessionState(_handle, sessionId));

  // ================================================================
  // MAVLink 签名
  // ================================================================

  Object? setSigningKey(
    int vehicleId,
    String keyHex,
    bool signOutgoing,
    int timestampTicks,
  ) {
    final key = keyHex.toNativeUtf8();
    try {
      return _readJson(
        _setSigningKey(_handle, vehicleId, key, signOutgoing ? 1 : 0, timestampTicks),
      );
    } finally {
      malloc.free(key);
    }
  }

  Object? setSigningCandidates(int vehicleId, String candidatesJson) {
    final text = candidatesJson.toNativeUtf8();
    try {
      return _readJson(_setSigningCandidates(_handle, vehicleId, text));
    } finally {
      malloc.free(text);
    }
  }

  Object? clearSigningKey(int vehicleId) =>
      _readJson(_clearSigningKey(_handle, vehicleId));

  Object? clearSigningCandidates(int vehicleId) =>
      _readJson(_clearSigningCandidates(_handle, vehicleId));

  Object? signingStatus(int vehicleId) =>
      _readJson(_getSigningStatus(_handle, vehicleId));

  Object? sendSetupSigning(int vehicleId, String keyHex, int timestampTicks) {
    final key = keyHex.toNativeUtf8();
    try {
      return _readJson(
        _sendSetupSigning(_handle, vehicleId, key, timestampTicks),
      );
    } finally {
      malloc.free(key);
    }
  }

  // ================================================================
  // 波形与数据流
  // ================================================================

  /// 读取波形窗口，返回实际点数。
  ///
  /// 数据留在本类持有的原生缓冲区里，用 [waveformValueAt] / [waveformView] 读取，
  /// 调用方不需要接触原生内存。
  int readWaveform({
    required int maxPoints,
    required int floatsPerPoint,
    required int windowMs,
    required int instance,
  }) {
    _ensureWaveformCapacity(maxPoints * floatsPerPoint);
    return _readWaveform(_handle, _waveformBuffer!, maxPoints, maxPoints, windowMs, instance);
  }

  /// 最近一次 [readWaveform] 结果里第 [floatIndex] 个 float。
  double waveformValueAt(int floatIndex) =>
      _waveformBuffer![floatIndex].toDouble();

  /// 最近一次 [readWaveform] 结果前 [floatCount] 个 float 的副本。
  Float32List waveformView(int floatCount) {
    final view = _waveformBuffer!.asTypedList(floatCount);
    return Float32List.fromList(view);
  }

  Object? waveformInstances() => _readJson(_getWaveformInstances(_handle));

  /// 设置某条 MAVLink 消息的下发频率；失败只是画质问题，不向上报错。
  void setStreamInterval(int messageId, int intervalUs) {
    _free(_setStreamInterval(_handle, messageId, intervalUs));
  }

  // ================================================================
  // 车辆与操作
  // ================================================================

  Object? requestFirmwareVersion() =>
      _readJson(_requestFirmwareVersion(_handle));

  Object? flightModes() => _readJson(_getFlightModes(_handle));

  Object? messages(int sinceSeq) => _readJson(_getMessages(_handle, sinceSeq));

  Object? serialUartNames() => _readJson(_getSerialUartNames(_handle));

  /// 取消某架飞机上的一类长操作（best effort）。
  void cancelOperationForVehicle(int vehicleId, String operation) {
    final name = operation.toNativeUtf8();
    try {
      _free(_cancelOperationForVehicle(_handle, vehicleId, name));
    } finally {
      malloc.free(name);
    }
  }

  void dispose() {
    if (_waveformBuffer != null) {
      calloc.free(_waveformBuffer!);
      _waveformBuffer = null;
      _waveformCapacity = 0;
    }
    _destroy(_handle);
  }

  // ================================================================
  // 内部
  // ================================================================

  Pointer<Utf8> Function(Pointer<Void>) _bind1(String symbol) => _library
      .lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>),
        Pointer<Utf8> Function(Pointer<Void>)
      >(symbol);

  Pointer<Utf8> Function(Pointer<Void>, int) _bindInt1(String symbol) => _library
      .lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>, Int32),
        Pointer<Utf8> Function(Pointer<Void>, int)
      >(symbol);

  Pointer<Utf8> Function(Pointer<Void>, int) _bindUint64(String symbol) =>
      _library
          .lookupFunction<
            Pointer<Utf8> Function(Pointer<Void>, Uint64),
            Pointer<Utf8> Function(Pointer<Void>, int)
          >(symbol);

  Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>) _bindText1(
    String symbol,
  ) => _library
      .lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>),
        Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
      >(symbol);

  Pointer<Utf8> Function(Pointer<Void>, int, int) _bindInt2(String symbol) =>
      _library
          .lookupFunction<
            Pointer<Utf8> Function(Pointer<Void>, Int32, Int32),
            Pointer<Utf8> Function(Pointer<Void>, int, int)
          >(symbol);

  void _ensureWaveformCapacity(int floats) {
    if (_waveformCapacity >= floats) return;
    if (_waveformBuffer != null) calloc.free(_waveformBuffer!);
    _waveformBuffer = calloc<Float>(floats);
    _waveformCapacity = floats;
  }

  void _free(Pointer<Utf8> pointer) {
    if (pointer != nullptr) _freeString(pointer);
  }

  Object? _readJson(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      return jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
  }

  /// 按平台查找 mgcore 共享库。
  ///
  /// 桌面平台的构建脚本会把共享库放到可执行文件旁边；macOS 用 app bundle 的
  /// Frameworks 目录；Android 由系统 linker 按 JNI 库名查找。
  /// `MGCS_FLEET_LIBRARY` 可显式指定路径（测试用）。
  static DynamicLibrary _openPlatformLibrary() {
    final testLibrary = Platform.environment['MGCS_FLEET_LIBRARY'];
    if (testLibrary != null && testLibrary.isNotEmpty) {
      return DynamicLibrary.open(testLibrary);
    }
    if (Platform.isAndroid) {
      return DynamicLibrary.open('libmgcore.so');
    }

    final executable = File(Platform.resolvedExecutable);
    final candidates = <String>[];
    if (Platform.isMacOS) {
      candidates.add(
        '${executable.parent.parent.path}/Frameworks/libmgcore.dylib',
      );
    } else if (Platform.isWindows) {
      candidates.add('${executable.parent.path}/mgcore.dll');
    } else if (Platform.isLinux) {
      candidates.add('${executable.parent.path}/libmgcore.so');
      candidates.add('${executable.parent.path}/lib/libmgcore.so');
    }

    for (final path in candidates) {
      final file = File(path);
      if (file.existsSync()) return DynamicLibrary.open(file.path);
    }

    // Linux distributions may resolve a library installed in the process
    // search path even when it is not beside the executable.
    if (Platform.isLinux) return DynamicLibrary.open('libmgcore.so');

    final expected = candidates.isEmpty
        ? '当前平台的 mgcore 共享库'
        : candidates.join(' 或 ');
    throw StateError('找不到 $expected');
  }
}
