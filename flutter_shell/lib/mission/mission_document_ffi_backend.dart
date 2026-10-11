import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi_response.dart';
import 'mission_document_backend.dart';
import 'mission_document_models.dart';

/// Flutter 到 Mission FFI 的适配层。
///
/// 任务文档的 JSON/传输 ABI 绑定集中在这里，ConnectionController 只负责连接、
/// 遥测和生命周期。业务层继续依赖 MissionDocumentBackend，不感知 FFI 细节。
final class MissionDocumentFfiBackend implements MissionDocumentBackend {
  MissionDocumentFfiBackend(
    this._library,
    this._corePointer, {
    required this.isConnected,
    this.beforeAccess,
  }) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _missionDocCreate = _lookup2('mg_mission_doc_create');
    _missionDocGet = _lookup1('mg_mission_doc_get');
    _missionDocApply = _lookup2('mg_mission_doc_apply');
    _missionDocUndo = _lookup1('mg_mission_doc_undo');
    _missionDocRedo = _lookup1('mg_mission_doc_redo');
    _missionDocValidate = _lookup1('mg_mission_doc_validate');
    _missionDocCompile = _lookup1('mg_mission_doc_compile');
    _missionDocEstimate = _lookup1('mg_mission_doc_estimate');
    _missionDocUpload = _lookupUpload('mg_mission_doc_upload');
    _missionDocVerify = _lookupVerify('mg_mission_doc_verify');
    _missionCommandCatalog = _lookup1('mg_mission_get_command_catalog');
    _missionDocExport = _lookup1('mg_mission_doc_export');
    _missionDocExportFormat = _lookup2('mg_mission_doc_export_format');
    _missionDocImport = _lookup2('mg_mission_doc_import');
    _missionDocSave = _lookup2('mg_mission_doc_save');
    _missionDocLoad = _lookup2('mg_mission_doc_load');
    _missionDocImportFromVehicle = _lookup1(
      'mg_mission_doc_import_from_vehicle',
    );
    _missionDocCompileFence = _lookup1('mg_mission_doc_compile_fence');
    _missionDocUploadFence = _lookupUpload('mg_mission_doc_upload_fence');
    _missionDocClearFence = _lookup1('mg_mission_doc_clear_fence');
    _missionDocImportFenceFromVehicle = _lookup1(
      'mg_mission_doc_import_fence_from_vehicle',
    );
    _missionDocVerifyFence = _lookupVerify('mg_mission_doc_verify_fence');
    _fenceDownload = _lookup1('mg_fence_download');
    _missionDocCompileRally = _lookup1('mg_mission_doc_compile_rally');
    _missionDocUploadRally = _lookupUpload('mg_mission_doc_upload_rally');
    _missionDocClearRally = _lookup1('mg_mission_doc_clear_rally');
    _missionDocImportRallyFromVehicle = _lookup1(
      'mg_mission_doc_import_rally_from_vehicle',
    );
    _missionDocVerifyRally = _lookupVerify('mg_mission_doc_verify_rally');
    _rallyDownload = _lookup1('mg_rally_download');
    _missionDownload = _lookup1('mg_mission_download');
    _missionClear = _lookup1('mg_mission_clear');
    _missionExecutionProgress = _lookup1('mg_mission_get_execution_progress');
    _operationsGetSnapshot = _lookup1('mg_operations_get_snapshot');
    _operationsCancel = _lookup2('mg_operations_cancel');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _corePointer;
  final void Function()? beforeAccess;

  // Check before evaluating the native call, including resumed async work.
  Pointer<Void> get _core {
    beforeAccess?.call();
    return _corePointer;
  }

  final bool Function() isConnected;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocCreate;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocGet;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocApply;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocUndo;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocRedo;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocValidate;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocCompile;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocEstimate;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _missionDocUpload;
  late final Pointer<Utf8> Function(Pointer<Void>, double) _missionDocVerify;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionCommandCatalog;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocExport;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocExportFormat;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocImport;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocSave;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _missionDocLoad;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocImportFromVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocCompileFence;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _missionDocUploadFence;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocClearFence;
  late final Pointer<Utf8> Function(Pointer<Void>)
  _missionDocImportFenceFromVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>, double)
  _missionDocVerifyFence;
  late final Pointer<Utf8> Function(Pointer<Void>) _fenceDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocCompileRally;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _missionDocUploadRally;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDocClearRally;
  late final Pointer<Utf8> Function(Pointer<Void>)
  _missionDocImportRallyFromVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>, double)
  _missionDocVerifyRally;
  late final Pointer<Utf8> Function(Pointer<Void>) _rallyDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionDownload;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionClear;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionExecutionProgress;
  late final Pointer<Utf8> Function(Pointer<Void>) _operationsGetSnapshot;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _operationsCancel;

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

  Pointer<Utf8> Function(Pointer<Void>, int) _lookupUpload(
    String symbol,
  ) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Uint64)>>(
        symbol,
      )
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, double) _lookupVerify(
    String symbol,
  ) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Double)>>(
        symbol,
      )
      .asFunction();

  @override
  Future<MissionDocument> createMissionDocument([
    JsonMap config = const {},
  ]) async {
    final nativeConfig = jsonEncode(config).toNativeUtf8();
    try {
      return _documentFromEnvelope(
        _readJson(_missionDocCreate(_core, nativeConfig)),
      );
    } finally {
      malloc.free(nativeConfig);
    }
  }

  @override
  Future<MissionDocument> getMissionDocument() async =>
      _documentFromEnvelope(_readJson(_missionDocGet(_core)));

  @override
  Future<MissionDocumentOperationResult> applyMissionDocumentCommand(
    JsonMap command,
  ) async {
    final nativeCommand = jsonEncode(command).toNativeUtf8();
    try {
      return _operationResult(
        _readJson(_missionDocApply(_core, nativeCommand)),
      );
    } finally {
      malloc.free(nativeCommand);
    }
  }

  @override
  Future<MissionDocumentOperationResult> undoMissionDocument() async =>
      _operationResult(_readJson(_missionDocUndo(_core)));

  @override
  Future<MissionDocumentOperationResult> redoMissionDocument() async =>
      _operationResult(_readJson(_missionDocRedo(_core)));

  @override
  Future<MissionValidationReport> validateMissionDocument() async =>
      MissionValidationReport.fromJson(
        _readJson(_missionDocValidate(_core)) as JsonMap,
      );

  @override
  Future<MissionCompileResult> compileMissionDocument() async =>
      MissionCompileResult.fromJson(
        _readJson(_missionDocCompile(_core)) as JsonMap,
      );

  @override
  Future<MissionEstimate> estimateMissionDocument() async =>
      MissionEstimate.fromJson(
        _readJson(_missionDocEstimate(_core)) as JsonMap,
      );

  @override
  Future<List<MissionCommandDescriptor>> getMissionCommandCatalog() async {
    final response = _readJson(_missionCommandCatalog(_core)) as JsonMap;
    _requireSuccess(response);
    return (response['commands'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map(MissionCommandDescriptor.fromJson)
        .toList(growable: false);
  }

  @override
  Future<JsonMap> uploadMissionDocument(int expectedRevision) async {
    final response =
        _readJson(_missionDocUpload(_core, expectedRevision)) as JsonMap;
    _requireSuccess(response);
    return response;
  }

  @override
  Future<void> startMissionDownload() async {
    if (!isConnected()) throw StateError('未连接飞控，无法读取飞控任务');
    _requireSuccess(_readJson(_missionDownload(_core)) as JsonMap);
  }

  @override
  Future<MissionTransferProgress> getMissionTransferProgress() async {
    final response = _readJson(_operationsGetSnapshot(_core)) as JsonMap;
    _requireSuccess(response);
    final mission = response['mission'];
    if (mission is! Map) throw const FormatException('任务 operation 响应格式无效');
    return MissionTransferProgress.fromJson(Map<String, Object?>.from(mission));
  }

  @override
  Future<void> cancelMissionTransfer() async {
    final nativeId = 'mission'.toNativeUtf8();
    try {
      _requireSuccess(_readJson(_operationsCancel(_core, nativeId)) as JsonMap);
    } finally {
      malloc.free(nativeId);
    }
  }

  @override
  Future<MissionExecutionProgress> getMissionExecutionProgress() async {
    final response = _readJson(_missionExecutionProgress(_core)) as JsonMap;
    _requireSuccess(response);
    return MissionExecutionProgress.fromJson(response);
  }

  @override
  Future<void> clearVehicleMission() async {
    if (!isConnected()) throw StateError('未连接飞控，无法清除飞控任务');
    _requireSuccess(_readJson(_missionClear(_core)) as JsonMap);
  }

  @override
  Future<MissionDocument> importMissionDocumentFromVehicle() async =>
      _documentFromEnvelope(_readJson(_missionDocImportFromVehicle(_core)));

  @override
  Future<MissionVerificationResult> verifyMissionDocument({
    double toleranceMeters = 1.0,
  }) async {
    final response =
        _readJson(_missionDocVerify(_core, toleranceMeters)) as JsonMap;
    _requireSuccess(response);
    return MissionVerificationResult.fromJson(response);
  }

  @override
  Future<JsonMap> compileFencePlan() async {
    final response = _readJson(_missionDocCompileFence(_core)) as JsonMap;
    _requireSuccess(response);
    return response;
  }

  @override
  Future<JsonMap> uploadFence(int expectedRevision) async {
    final response =
        _readJson(_missionDocUploadFence(_core, expectedRevision)) as JsonMap;
    _requireSuccess(response);
    return response;
  }

  @override
  Future<void> startFenceDownload() async {
    if (!isConnected()) throw StateError('未连接飞控，无法读取围栏');
    _requireSuccess(_readJson(_fenceDownload(_core)) as JsonMap);
  }

  @override
  Future<MissionDocument> importFenceFromVehicle() async =>
      _documentFromEnvelope(
        _readJson(_missionDocImportFenceFromVehicle(_core)),
      );

  @override
  Future<MissionVerificationResult> verifyFence({
    double toleranceMeters = 1.0,
  }) async {
    final response =
        _readJson(_missionDocVerifyFence(_core, toleranceMeters)) as JsonMap;
    _requireSuccess(response);
    return MissionVerificationResult.fromJson(response);
  }

  @override
  Future<void> clearFence() async =>
      _requireSuccess(_readJson(_missionDocClearFence(_core)) as JsonMap);

  @override
  Future<MissionTransferProgress> getFenceTransferProgress() =>
      _transferProgressFor('fence');

  @override
  Future<MissionTransferProgress> getRallyTransferProgress() =>
      _transferProgressFor('rally');

  Future<MissionTransferProgress> _transferProgressFor(String key) async {
    final response = _readJson(_operationsGetSnapshot(_core)) as JsonMap;
    _requireSuccess(response);
    final section = response[key];
    if (section is! Map) throw FormatException('$key operation 响应格式无效');
    return MissionTransferProgress.fromJson(Map<String, Object?>.from(section));
  }

  @override
  Future<JsonMap> compileRallyPlan() async {
    final response = _readJson(_missionDocCompileRally(_core)) as JsonMap;
    _requireSuccess(response);
    return response;
  }

  @override
  Future<JsonMap> uploadRally(int expectedRevision) async {
    final response =
        _readJson(_missionDocUploadRally(_core, expectedRevision)) as JsonMap;
    _requireSuccess(response);
    return response;
  }

  @override
  Future<void> startRallyDownload() async {
    if (!isConnected()) throw StateError('未连接飞控，无法读取 Rally 点');
    _requireSuccess(_readJson(_rallyDownload(_core)) as JsonMap);
  }

  @override
  Future<MissionDocument> importRallyFromVehicle() async =>
      _documentFromEnvelope(
        _readJson(_missionDocImportRallyFromVehicle(_core)),
      );

  @override
  Future<MissionVerificationResult> verifyRally({
    double toleranceMeters = 1.0,
  }) async {
    final response =
        _readJson(_missionDocVerifyRally(_core, toleranceMeters)) as JsonMap;
    _requireSuccess(response);
    return MissionVerificationResult.fromJson(response);
  }

  @override
  Future<void> clearRally() async =>
      _requireSuccess(_readJson(_missionDocClearRally(_core)) as JsonMap);

  @override
  Future<MissionDocument> importMissionDocument(String json) async {
    final nativeJson = json.toNativeUtf8();
    try {
      return _documentFromEnvelope(
        _readJson(_missionDocImport(_core, nativeJson)),
      );
    } finally {
      malloc.free(nativeJson);
    }
  }

  @override
  Future<String> exportMissionDocument() async =>
      _readString(_missionDocExport(_core));

  @override
  Future<MissionInterchangeExport> exportMissionInterchange(
    String format,
  ) async {
    final nativeFormat = format.toNativeUtf8();
    try {
      final json = _readJson(_missionDocExportFormat(_core, nativeFormat));
      if (json is! Map) throw const FormatException('导出响应格式无效');
      _requireSuccess(Map<String, Object?>.from(json));
      return MissionInterchangeExport.fromJson(Map<String, Object?>.from(json));
    } finally {
      malloc.free(nativeFormat);
    }
  }

  @override
  Future<void> saveMissionDocument(String filePath) async {
    final nativePath = filePath.toNativeUtf8();
    try {
      _requireSuccess(_readJson(_missionDocSave(_core, nativePath)) as JsonMap);
    } finally {
      malloc.free(nativePath);
    }
  }

  @override
  Future<MissionDocument> loadMissionDocument(String filePath) async {
    final nativePath = filePath.toNativeUtf8();
    try {
      return _documentFromEnvelope(
        _readJson(_missionDocLoad(_core, nativePath)),
      );
    } finally {
      malloc.free(nativePath);
    }
  }

  Object? _readJson(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      return jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
  }

  String _readString(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      return pointer.toDartString();
    } finally {
      _freeString(pointer);
    }
  }

  MissionDocument _documentFromEnvelope(Object? raw) {
    if (raw is! JsonMap) throw const FormatException('任务文档响应格式无效');
    _requireSuccess(raw);
    return MissionDocument.fromJson(raw['document'] as JsonMap);
  }

  MissionDocumentOperationResult _operationResult(Object? raw) {
    if (raw is! JsonMap) throw const FormatException('任务编辑响应格式无效');
    _requireSuccess(raw);
    return MissionDocumentOperationResult(
      document: MissionDocument.fromJson(raw['document'] as JsonMap),
      revision: raw['revision'] as int? ?? 0,
    );
  }

  void _requireSuccess(JsonMap response) {
    if (ffiSucceeded(response)) return;
    final message = ffiErrorText(response, '任务后端操作失败');
    final revision = response['revision'];
    throw StateError(
      revision == null ? message : '$message（当前 revision $revision）',
    );
  }
}
