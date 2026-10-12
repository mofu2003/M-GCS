import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../connection/native_core.dart';
import 'log_backend.dart';

/// `mg_onboard_log_*` 的 FFI 实现。
final class LogFfiBackend implements LogBackend {
  LogFfiBackend(
    this._library,
    this._core, {
    required bool Function() isConnected,
  }) {
    _isConnected = isConnected;
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _requestList = _lookup1('mg_onboard_log_request_list');
    _getEntries = _lookup1('mg_onboard_log_get_entries');
    _cancel = _lookup1('mg_onboard_log_download_cancel');
    _erase = _lookup1('mg_onboard_log_erase');
    _status = _lookup1('mg_onboard_log_download_status');
    _download = _library
        .lookup<
          NativeFunction<
            Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>)
          >
        >('mg_onboard_log_download')
        .asFunction();
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;
  late final bool Function() _isConnected;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestList;
  late final Pointer<Utf8> Function(Pointer<Void>) _getEntries;
  late final Pointer<Utf8> Function(Pointer<Void>) _cancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _erase;
  late final Pointer<Utf8> Function(Pointer<Void>) _status;
  late final Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
  _download;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  @override
  bool get isConnected => _isConnected();

  @override
  Map<String, Object?> requestList() => _read(_requestList(_core));

  @override
  Map<String, Object?> entries() => _read(_getEntries(_core));

  @override
  Map<String, Object?> download(int logId, String filepath) {
    final nativePath = filepath.toNativeUtf8();
    try {
      return _read(_download(_core, logId, nativePath));
    } finally {
      malloc.free(nativePath);
    }
  }

  @override
  Map<String, Object?> cancel() => _read(_cancel(_core));

  @override
  Map<String, Object?> status() => _read(_status(_core));

  @override
  Map<String, Object?> eraseAll() => _read(_erase(_core));

  Map<String, Object?> _read(Pointer<Utf8> pointer) => readJsonMap(
    pointer,
    freeString: _freeString,
    invalidResponseMessage: '日志响应格式无效',
  );
}
