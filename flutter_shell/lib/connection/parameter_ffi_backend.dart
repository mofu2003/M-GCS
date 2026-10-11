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
    _getWriteStatus = _lookup1('mg_params_get_write_status');
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
  late final Pointer<Utf8> Function(Pointer<Void>) _getWriteStatus;
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

  /// 参数写入事务记录（id / name / requested_value / actual_value / type /
  /// state / error / requested_at_ms / completed_at_ms）。
  ///
  /// [state] 取 `pending`、`confirmed`、`failed`；`failed` 记录带飞控拒绝
  /// （PARAM_ERROR）或超时的原因。写入口（[write]）只表示“已交给发送层”，
  /// 调用方必须用本列表或回读参数值确认写入结果。
  List<Map<String, Object?>> writes() {
    final data = writeStatus()['data'];
    if (data is! Map) return const [];
    final list = data['writes'];
    if (list is! List) return const [];
    return list
        .whereType<Map<Object?, Object?>>()
        .map(Map<String, Object?>.from)
        .toList(growable: false);
  }

  Map<String, Object?> writeStatus() => _read(_getWriteStatus(_core));

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
