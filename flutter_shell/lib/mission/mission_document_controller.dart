import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'mission_document_backend.dart';
import 'mission_document_models.dart';

final class MissionDocumentController extends ChangeNotifier {
  MissionDocumentController({required this.backend});

  final MissionDocumentBackend backend;

  MissionDocument? document;
  MissionValidationReport? validation;
  MissionCompileResult? compileResult;
  MissionEstimate? estimate;
  List<MissionCommandDescriptor> commandCatalog = const [];
  String? status;
  String? error;
  bool busy = false;
  bool initialized = false;
  int? selectedEntityIndex;

  Future<void> initialize() async {
    if (initialized || busy) return;
    await _run('正在初始化任务文档…', () async {
      commandCatalog = await backend.getMissionCommandCatalog();
      document = await backend.getMissionDocument();
      initialized = true;
    });
  }

  Future<void> create([JsonMap config = const {}]) =>
      _run('正在创建任务文档…', () async {
        document = await backend.createMissionDocument(config);
        validation = null;
        compileResult = null;
        estimate = null;
        selectedEntityIndex = null;
        initialized = true;
      });

  Future<void> refresh() => _run(
    '正在刷新任务文档…',
    () async => document = await backend.getMissionDocument(),
  );

  Future<void> addEntity(MissionEntity entity) => _apply({
    'operation': 'add',
    if (document != null) 'expected_revision': document!.revision,
    'entity': entity.toJson(),
  });

  Future<void> updateEntity(MissionEntity entity) => _apply({
    'operation': 'update',
    if (document != null) 'expected_revision': document!.revision,
    'entity': entity.toJson(),
  });

  Future<void> removeEntity(String entityId) => _apply({
    'operation': 'remove',
    if (document != null) 'expected_revision': document!.revision,
    'entity_id': entityId,
  });

  Future<void> moveEntity(String entityId, int toIndex) => _apply({
    'operation': 'move',
    if (document != null) 'expected_revision': document!.revision,
    'entity_id': entityId,
    'to_index': toIndex,
  });

  Future<void> clear() => _apply({
    'operation': 'clear',
    if (document != null) 'expected_revision': document!.revision,
  });

  Future<void> replaceDocument(MissionDocument next) => _apply({
    'operation': 'replace',
    if (document != null) 'expected_revision': document!.revision,
    'document': next.toJson(),
  });

  Future<void> undo() =>
      _operation('undo', '正在撤销…', backend.undoMissionDocument);

  Future<void> redo() =>
      _operation('redo', '正在重做…', backend.redoMissionDocument);

  Future<void> validate() => _run('正在校验任务…', () async {
    validation = await backend.validateMissionDocument();
  });

  Future<void> compile() => _run('正在编译任务…', () async {
    compileResult = await backend.compileMissionDocument();
    validation = compileResult?.report;
  });

  Future<void> refreshEstimate() => _run('正在估算任务…', () async {
    estimate = await backend.estimateMissionDocument();
  });

  Future<JsonMap> upload() async {
    final current = document;
    if (current == null) {
      throw StateError('任务文档尚未初始化');
    }
    JsonMap response = const {};
    await _run('正在写入飞控…', () async {
      response = await backend.uploadMissionDocument(current.revision);
    });
    return response;
  }

  Future<JsonMap> verify({double toleranceMeters = 0.1}) async {
    JsonMap response = const {};
    await _run('正在核对飞控任务…', () async {
      response = await backend.verifyMissionDocument(
        toleranceMeters: toleranceMeters,
      );
    });
    return response;
  }

  Future<void> importJson(String json) => _run('正在导入任务…', () async {
    document = await backend.importMissionDocument(json);
    validation = null;
    compileResult = null;
    estimate = null;
    selectedEntityIndex = null;
  });

  Future<String> exportJson() => backend.exportMissionDocument();

  Future<void> save(String filePath) =>
      _run('正在保存任务…', () => backend.saveMissionDocument(filePath));

  Future<void> load(String filePath) => _run('正在加载任务…', () async {
    document = await backend.loadMissionDocument(filePath);
    validation = null;
    compileResult = null;
    estimate = null;
    selectedEntityIndex = null;
  });

  void selectEntity(int? index) {
    if (selectedEntityIndex == index) return;
    selectedEntityIndex = index;
    notifyListeners();
  }

  Future<void> _apply(JsonMap command) => _operation(
    'apply',
    '正在更新任务…',
    () => backend.applyMissionDocumentCommand(command),
  );

  Future<void> _operation(
    String operation,
    String initialStatus,
    Future<MissionDocumentOperationResult> Function() action,
  ) => _run(initialStatus, () async {
    final result = await action();
    document = result.document;
    validation = null;
    compileResult = null;
    estimate = null;
    status = switch (operation) {
      'undo' => '已撤销',
      'redo' => '已重做',
      _ => '任务已更新，revision ${result.revision}',
    };
  });

  Future<void> _run(
    String initialStatus,
    Future<void> Function() action,
  ) async {
    if (busy) return;
    busy = true;
    status = initialStatus;
    error = null;
    notifyListeners();
    try {
      await action();
    } on Object catch (exception) {
      error = _friendlyError(exception);
      status = error;
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  static String _friendlyError(Object error) {
    if (error is StateError) {
      return error.message.toString();
    }
    if (error is FormatException) {
      return '后端返回格式错误：${error.message}';
    }
    return error.toString();
  }

  String exportDebugJson() => const JsonEncoder.withIndent(
    '  ',
  ).convert(document?.toJson() ?? const <String, Object?>{});
}
