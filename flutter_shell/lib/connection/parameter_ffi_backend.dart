import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

/// 参数 FFI 适配层。
///
/// 集中管理参数请求、快照、描述信息和写入 ABI；参数缓存、元数据批处理、
/// 写入队列与界面通知仍由 ConnectionController 负责。
final class ParameterFfiBackend {
  ParameterFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _requestAll = _lookup1('mg_params_request_all');
    _getAll = _lookup1('mg_params_get_all');
    _getProgress = _lookup1('mg_params_get_progress');
    _getByName = _lookup2('mg_params_get_by_name');
    _readDescriptor = _lookup2('mg_config_read_param');
    _write = _lookupWrite('mg_config_write_param');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestAll;
  late final Pointer<Utf8> Function(Pointer<Void>) _getAll;
  late final Pointer<Utf8> Function(Pointer<Void>) _getProgress;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _getByName;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _readDescriptor;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double)
  _write;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _lookup2(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
      >(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double) _lookupWrite(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Float)
        >
      >(symbol)
      .asFunction<
        Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double)
      >();

  Map<String, Object?> requestAll() => _read(_requestAll(_core));

  Object? all() => _readRaw(_getAll(_core));

  Map<String, Object?> progress() => _read(_getProgress(_core));

  Map<String, Object?> byName(String name) {
    final nativeName = name.toNativeUtf8();
    try {
      return _read(_getByName(_core, nativeName));
    } finally {
      malloc.free(nativeName);
    }
  }

  Map<String, Object?> descriptor(String name) {
    final nativeName = name.toNativeUtf8();
    try {
      return _read(_readDescriptor(_core, nativeName));
    } finally {
      malloc.free(nativeName);
    }
  }

  Map<String, Object?> write(String name, double value) {
    final nativeName = name.toNativeUtf8();
    try {
      return _read(_write(_core, nativeName, value));
    } finally {
      malloc.free(nativeName);
    }
  }

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    final raw = _readRaw(pointer);
    if (raw is! Map) throw const FormatException('参数响应格式无效');
    return Map<String, Object?>.from(raw);
  }

  Object? _readRaw(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      return jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
  }
}
