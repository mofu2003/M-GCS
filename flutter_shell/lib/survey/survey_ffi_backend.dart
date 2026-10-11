import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../mission/mission_document_models.dart';
import 'survey_backend.dart';
import 'survey_models.dart';

/// `mg_survey_*` 的 FFI 实现。
///
/// 两条导出都是同步纯计算：JSON 配置进，JSON 结果出。返回值由
/// `mg_free_string` 释放，调用方不接触指针所有权（ffi-contract F3）。
final class SurveyFfiBackend implements SurveyBackend {
  SurveyFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _generate = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
        >('mg_survey_generate')
        .asFunction();
    _generateCorridor = _library
        .lookup<
          NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
        >('mg_survey_generate_corridor')
        .asFunction();
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _generate;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _generateCorridor;

  @override
  Future<SurveyResult> generateGrid(SurveyGridConfig config) async =>
      SurveyResult.fromJson(_call(_generate, config.toJson()));

  @override
  Future<SurveyResult> generateCorridor(SurveyCorridorConfig config) async =>
      SurveyResult.fromJson(_call(_generateCorridor, config.toJson()));

  JsonMap _call(
    Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) fn,
    JsonMap config,
  ) {
    final nativeJson = jsonEncode(config).toNativeUtf8();
    final Pointer<Utf8> pointer;
    try {
      pointer = fn(_core, nativeJson);
    } finally {
      malloc.free(nativeJson);
    }
    if (pointer == nullptr) throw StateError('后端返回空结果');
    final Object? raw;
    try {
      raw = jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
    if (raw is! Map) throw const FormatException('测绘响应格式无效');
    return Map<String, Object?>.from(raw);
  }
}
