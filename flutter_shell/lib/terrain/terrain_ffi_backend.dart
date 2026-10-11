import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'terrain_backend.dart';
import 'terrain_models.dart';

/// `mg_terrain_*` 的 FFI 实现。
///
/// 全部是同步调用：查询只读 native 缓存，写入只更新 native 缓存。
/// 网络拉取在 [ElevationProvider]，不在这一层。
final class TerrainFfiBackend implements TerrainBackend {
  TerrainFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _queryPoint = _library
        .lookup<
          NativeFunction<
            Pointer<Utf8> Function(Pointer<Void>, Double, Double)
          >
        >('mg_terrain_query')
        .asFunction();
    _queryLine = _library
        .lookup<
          NativeFunction<
            Pointer<Utf8> Function(
              Pointer<Void>,
              Double,
              Double,
              Double,
              Double,
              Int32,
            )
          >
        >('mg_terrain_query_line')
        .asFunction();
    _cacheUpdate = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
        >('mg_terrain_cache_update')
        .asFunction();
    _cacheClear = _library
        .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
          'mg_terrain_cache_clear',
        )
        .asFunction();
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double)
  _queryPoint;
  late final Pointer<Utf8> Function(
    Pointer<Void>,
    double,
    double,
    double,
    double,
    int,
  )
  _queryLine;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _cacheUpdate;
  late final Pointer<Utf8> Function(Pointer<Void>) _cacheClear;

  @override
  Map<String, Object?> queryPoint(double lat, double lon) =>
      _read(_queryPoint(_core, lat, lon));

  @override
  Map<String, Object?> queryLine(
    double lat1,
    double lon1,
    double lat2,
    double lon2,
    int samples,
  ) => _read(_queryLine(_core, lat1, lon1, lat2, lon2, samples));

  @override
  Map<String, Object?> cacheUpdate(List<TerrainSample> samples) {
    final payload = jsonEncode(<String, Object?>{
      'points': samples.map((s) => s.toJson()).toList(),
    });
    final nativeJson = payload.toNativeUtf8();
    try {
      return _read(_cacheUpdate(_core, nativeJson));
    } finally {
      malloc.free(nativeJson);
    }
  }

  @override
  Map<String, Object?> cacheClear() => _read(_cacheClear(_core));

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    final Object? raw;
    try {
      raw = jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
    if (raw is! Map) throw const FormatException('地形响应格式无效');
    return Map<String, Object?>.from(raw);
  }
}
