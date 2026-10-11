import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'tlog_backend.dart';

/// `mg_tlog_*`（录制与本地回放）的 FFI 实现。
final class TlogFfiBackend implements TlogBackend {
  TlogFfiBackend(
    this._library,
    this._core, {
    required bool Function() isConnected,
  }) {
    _isConnected = isConnected;
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _status = _lookup1('mg_tlog_status');
    _stop = _lookup1('mg_tlog_stop');
    _replayClose = _lookup1('mg_tlog_replay_close');
    _start = _lookupPath('mg_tlog_start');
    _replayOpen = _lookupPath('mg_tlog_replay_open');
    _replayStep = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>
        >('mg_tlog_replay_step')
        .asFunction();
    _replaySeek = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Double)>
        >('mg_tlog_replay_seek')
        .asFunction();
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;
  late final bool Function() _isConnected;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _status;
  late final Pointer<Utf8> Function(Pointer<Void>) _stop;
  late final Pointer<Utf8> Function(Pointer<Void>) _replayClose;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _start;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _replayOpen;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _replayStep;
  late final Pointer<Utf8> Function(Pointer<Void>, double) _replaySeek;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _lookupPath(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
        >
      >(symbol)
      .asFunction();

  @override
  bool get isConnected => _isConnected();

  @override
  Map<String, Object?> recordingStatus() => _read(_status(_core));

  @override
  Map<String, Object?> startRecording(String filepath) =>
      _withPath(_start, filepath);

  @override
  Map<String, Object?> stopRecording() => _read(_stop(_core));

  @override
  Map<String, Object?> replayOpen(String filepath) =>
      _withPath(_replayOpen, filepath);

  @override
  Map<String, Object?> replayStep(int maxMessages) =>
      _read(_replayStep(_core, maxMessages));

  @override
  Map<String, Object?> replaySeek(double percent) =>
      _read(_replaySeek(_core, percent));

  @override
  Map<String, Object?> replayClose() => _read(_replayClose(_core));

  Map<String, Object?> _withPath(
    Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) fn,
    String filepath,
  ) {
    final nativePath = filepath.toNativeUtf8();
    try {
      return _read(fn(_core, nativePath));
    } finally {
      malloc.free(nativePath);
    }
  }

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    final Object? raw;
    try {
      raw = jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
    if (raw is! Map) throw const FormatException('飞行记录响应格式无效');
    return Map<String, Object?>.from(raw);
  }
}
