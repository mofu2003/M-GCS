import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'native_core.dart';

/// 校准 FFI 适配层。
///
/// 只负责校准命令和状态 JSON 的 ABI 绑定；校准状态机、轮询和 UI 状态仍由
/// ConnectionController 管理。
final class CalibrationFfiBackend {
  CalibrationFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _start = _lookupStart('mg_calibration_start');
    _cancel = _lookup1('mg_calibration_cancel');
    _getStatus = _lookup1('mg_calibration_get_status');
    _accelConfirm = _lookupConfirm('mg_calibration_accel_confirm');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _start;
  late final Pointer<Utf8> Function(Pointer<Void>) _cancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _getStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _accelConfirm;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, int, int) _lookupStart(String symbol) =>
      _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Int32)>
          >(symbol)
          .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, int) _lookupConfirm(String symbol) =>
      _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            symbol,
          )
          .asFunction();

  Map<String, Object?> start(int type, int accelMode) =>
      _read(_start(_core, type, accelMode));

  Map<String, Object?> cancel() => _read(_cancel(_core));

  Map<String, Object?> status() => _read(_getStatus(_core));

  Map<String, Object?> confirmAccelerometerPosition(int position) =>
      _read(_accelConfirm(_core, position));

  Map<String, Object?> _read(Pointer<Utf8> pointer) => readJsonMap(
    pointer,
    freeString: _freeString,
    invalidResponseMessage: '校准响应格式无效',
  );
}
