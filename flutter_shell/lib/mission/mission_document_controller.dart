import 'dart:async';
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
  MissionTransferProgress? transfer;
  MissionVerificationResult? verification;
  MissionVerificationResult? fenceVerification;
  MissionVerificationResult? rallyVerification;
  List<MissionCommandDescriptor> commandCatalog = const [];
  String? status;
  String? error;
  bool busy = false;
  bool dirty = false;
  bool initialized = false;
  int? selectedEntityIndex;

  bool get transferActive => transfer?.active ?? false;

  Future<void> initialize() async {
    if (initialized || busy) return;
    await _run('正在初始化任务文档…', () async {
      commandCatalog = await backend.getMissionCommandCatalog();
      document = await backend.getMissionDocument();
      dirty = false;
      initialized = true;
      await _refreshDerived();
      await _refreshTransfer();
    });
  }

  Future<void> create([JsonMap config = const {}]) =>
      _run('正在创建任务文档…', () async {
        document = await backend.createMissionDocument(config);
        validation = null;
        compileResult = null;
        estimate = null;
        verification = null;
        selectedEntityIndex = null;
        dirty = true;
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

  Future<void> reorderEntity(int oldIndex, int newIndex) async {
    final entities = document?.entities;
    if (entities == null || oldIndex < 0 || oldIndex >= entities.length) return;
    if (newIndex < 0 || newIndex >= entities.length || newIndex == oldIndex) {
      return;
    }
    final entityId = entities[oldIndex].id;
    await moveEntity(entityId, newIndex);
    selectEntityById(entityId);
  }

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

  /// 上传后必须回读核对；一次上传返回只代表传输已开始。
  Future<MissionVerificationResult?> upload({
    double toleranceMeters = 0.1,
  }) async {
    final current = document;
    if (current == null) {
      throw StateError('任务文档尚未初始化');
    }
    final currentRevision = current.revision;
    verification = null;
    await _run('正在上传任务…', rethrowErrors: true, () async {
      await backend.uploadMissionDocument(currentRevision);
      await _waitForTransfer('上传任务');

      status = '上传完成，正在从飞控回读核对…';
      notifyListeners();
      await backend.startMissionDownload();
      await _waitForTransfer('回读任务');

      verification = await backend.verifyMissionDocument(
        toleranceMeters: toleranceMeters,
      );
      final result = verification!;
      if (result.matches) {
        dirty = false;
        status = result.normalizationCount > 0
            ? '上传并回读核对通过，自动归一化 ${result.normalizationCount} 处飞控表示'
            : '上传并回读核对通过';
      } else {
        dirty = true;
        status = '上传完成，但回读发现 ${result.differenceCount} 处差异';
      }
    });
    return verification;
  }

  Future<void> downloadFromVehicle() =>
      _run('正在从飞控读取任务…', rethrowErrors: true, () async {
        verification = null;
        await backend.startMissionDownload();
        await _waitForTransfer('读取飞控任务');
        document = await backend.importMissionDocumentFromVehicle();
        validation = null;
        compileResult = null;
        estimate = null;
        verification = null;
        selectedEntityIndex = null;
        dirty = false;
        await _refreshDerived();
        status = '已从飞控读取 ${document?.entities.length ?? 0} 个任务项';
      });

  /// 采用最近一次完整回读的飞控任务，覆盖本地文档。
  /// 上传回读发现差异后复用 transport cache，不重新发起下载。
  Future<void> adoptDownloadedVehicleMission() =>
      _run('正在采用飞控回读任务…', rethrowErrors: true, () async {
        document = await backend.importMissionDocumentFromVehicle();
        validation = null;
        compileResult = null;
        estimate = null;
        verification = null;
        selectedEntityIndex = null;
        dirty = false;
        await _refreshDerived();
        status = '已采用飞控任务 ${document?.entities.length ?? 0} 个任务项';
      });

  Future<void> cancelTransfer() async {
    if (!transferActive) return;
    try {
      await backend.cancelMissionTransfer();
      await _refreshTransfer();
      if (transfer?.cancelled == true) {
        error = null;
        status = '已取消任务传输';
      }
    } on Object catch (exception) {
      error = _friendlyError(exception);
      status = error;
    }
    notifyListeners();
  }

  Future<MissionVerificationResult> verify({
    double toleranceMeters = 0.1,
  }) async {
    MissionVerificationResult? result;
    await _run('正在核对飞控任务…', rethrowErrors: true, () async {
      result = await backend.verifyMissionDocument(
        toleranceMeters: toleranceMeters,
      );
      verification = result;
    });
    return result!;
  }

  /// 下发围栏：上传 → 等待 → 回读 → 核对。围栏是独立 mission type，不随任务上传。
  Future<MissionVerificationResult?> uploadFence({
    double toleranceMeters = 0.1,
  }) async {
    final current = document;
    if (current == null) throw StateError('任务文档尚未初始化');
    final currentRevision = current.revision;
    fenceVerification = null;
    await _run('正在下发围栏…', rethrowErrors: true, () async {
      await backend.uploadFence(currentRevision);
      await _waitForOperation(backend.getFenceTransferProgress, '下发围栏');
      status = '围栏已下发，正在从飞控回读核对…';
      notifyListeners();
      await backend.startFenceDownload();
      await _waitForOperation(backend.getFenceTransferProgress, '回读围栏');
      fenceVerification = await backend.verifyFence(
        toleranceMeters: toleranceMeters,
      );
      final result = fenceVerification!;
      status = result.matches
          ? '围栏已下发并回读核对通过'
          : '围栏已下发，但回读发现 ${result.differenceCount} 处差异';
    });
    return fenceVerification;
  }

  /// 从飞控读取围栏并替换文档的围栏区段。
  Future<void> readFenceFromVehicle() =>
      _run('正在从飞控读取围栏…', rethrowErrors: true, () async {
        fenceVerification = null;
        await backend.startFenceDownload();
        await _waitForOperation(backend.getFenceTransferProgress, '读取围栏');
        document = await backend.importFenceFromVehicle();
        await _refreshDerived();
        status = '已从飞控读取围栏并更新文档';
      });

  Future<MissionVerificationResult> verifyFence({
    double toleranceMeters = 0.1,
  }) async {
    MissionVerificationResult? result;
    await _run('正在核对飞控围栏…', rethrowErrors: true, () async {
      await backend.startFenceDownload();
      await _waitForOperation(backend.getFenceTransferProgress, '回读围栏');
      result = await backend.verifyFence(toleranceMeters: toleranceMeters);
      fenceVerification = result;
    });
    return result!;
  }

  Future<void> clearFence() =>
      _run('正在清除飞控围栏…', rethrowErrors: true, () async {
        fenceVerification = null;
        await backend.clearFence();
        status = '已请求清除飞控围栏';
      });

  /// 下发 Rally 点：上传 → 等待 → 回读 → 核对。
  Future<MissionVerificationResult?> uploadRally({
    double toleranceMeters = 0.1,
  }) async {
    final current = document;
    if (current == null) throw StateError('任务文档尚未初始化');
    final currentRevision = current.revision;
    rallyVerification = null;
    await _run('正在下发 Rally 点…', rethrowErrors: true, () async {
      await backend.uploadRally(currentRevision);
      await _waitForOperation(backend.getRallyTransferProgress, '下发 Rally 点');
      status = 'Rally 点已下发，正在从飞控回读核对…';
      notifyListeners();
      await backend.startRallyDownload();
      await _waitForOperation(backend.getRallyTransferProgress, '回读 Rally 点');
      rallyVerification = await backend.verifyRally(
        toleranceMeters: toleranceMeters,
      );
      final result = rallyVerification!;
      status = result.matches
          ? 'Rally 点已下发并回读核对通过'
          : 'Rally 点已下发，但回读发现 ${result.differenceCount} 处差异';
    });
    return rallyVerification;
  }

  Future<void> readRallyFromVehicle() =>
      _run('正在从飞控读取 Rally 点…', rethrowErrors: true, () async {
        rallyVerification = null;
        await backend.startRallyDownload();
        await _waitForOperation(backend.getRallyTransferProgress, '读取 Rally 点');
        document = await backend.importRallyFromVehicle();
        await _refreshDerived();
        status = '已从飞控读取 Rally 点并更新文档';
      });

  Future<MissionVerificationResult> verifyRally({
    double toleranceMeters = 0.1,
  }) async {
    MissionVerificationResult? result;
    await _run('正在核对飞控 Rally 点…', rethrowErrors: true, () async {
      await backend.startRallyDownload();
      await _waitForOperation(backend.getRallyTransferProgress, '回读 Rally 点');
      result = await backend.verifyRally(toleranceMeters: toleranceMeters);
      rallyVerification = result;
    });
    return result!;
  }

  Future<void> clearRally() =>
      _run('正在清除飞控 Rally 点…', rethrowErrors: true, () async {
        rallyVerification = null;
        await backend.clearRally();
        status = '已请求清除飞控 Rally 点';
      });

  Future<void> importJson(String json) => _run('正在导入任务…', () async {
    document = await backend.importMissionDocument(json);
    validation = null;
    compileResult = null;
    estimate = null;
    verification = null;
    selectedEntityIndex = null;
    dirty = true;
  });

  Future<String> exportJson() => backend.exportMissionDocument();

  Future<void> save(String filePath) =>
      _run('正在保存任务…', () => backend.saveMissionDocument(filePath));

  Future<void> load(String filePath) => _run('正在加载任务…', () async {
    document = await backend.loadMissionDocument(filePath);
    validation = null;
    compileResult = null;
    estimate = null;
    verification = null;
    selectedEntityIndex = null;
    dirty = true;
  });

  void selectEntity(int? index) {
    if (selectedEntityIndex == index) return;
    selectedEntityIndex = index;
    notifyListeners();
  }

  void selectEntityById(String entityId) {
    final entities = document?.entities ?? const <MissionEntity>[];
    final index = entities.indexWhere((entity) => entity.id == entityId);
    if (index >= 0) selectEntity(index);
  }

  Future<void> _waitForTransfer(String action) async {
    final deadline = DateTime.now().add(const Duration(seconds: 30));
    while (DateTime.now().isBefore(deadline)) {
      await _refreshTransfer();
      final current = transfer;
      if (current == null || current.active) {
        await Future<void>.delayed(const Duration(milliseconds: 100));
        continue;
      }
      if (current.cancelled) throw const _MissionTransferCancelled();
      if (current.error.isNotEmpty || current.result != 0) {
        throw StateError(
          '$action失败：${current.error.isEmpty ? 'result=${current.result}' : current.error}',
        );
      }
      return;
    }
    throw StateError('$action超时');
  }

  Future<void> _refreshTransfer() async {
    transfer = await backend.getMissionTransferProgress();
    notifyListeners();
  }

  /// 与 [_waitForTransfer] 相同的等待语义，但进度来源是围栏/Rally 这类独立
  /// mission type。operations 快照里它们分别是 `fence` / `rally` 区段。
  Future<void> _waitForOperation(
    Future<MissionTransferProgress> Function() poll,
    String action,
  ) async {
    final deadline = DateTime.now().add(const Duration(seconds: 30));
    while (DateTime.now().isBefore(deadline)) {
      final current = await poll();
      if (current.active) {
        await Future<void>.delayed(const Duration(milliseconds: 100));
        continue;
      }
      if (current.cancelled) throw const _MissionTransferCancelled();
      if (current.error.isNotEmpty || current.result != 0) {
        throw StateError(
          '$action失败：${current.error.isEmpty ? 'result=${current.result}' : current.error}',
        );
      }
      return;
    }
    throw StateError('$action超时');
  }

  Future<void> _refreshDerived() async {
    validation = await backend.validateMissionDocument();
    estimate = await backend.estimateMissionDocument();
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
    compileResult = null;
    verification = null;
    dirty = true;
    await _refreshDerived();
    status = switch (operation) {
      'undo' => '已撤销',
      'redo' => '已重做',
      _ => '任务已更新，revision ${result.revision}',
    };
  });

  Future<void> _run(
    String initialStatus,
    Future<void> Function() action, {
    bool rethrowErrors = false,
  }) async {
    if (busy) return;
    busy = true;
    status = initialStatus;
    error = null;
    notifyListeners();
    try {
      await action();
    } on _MissionTransferCancelled {
      error = null;
      status = '任务传输已取消';
    } on Object catch (exception) {
      error = _friendlyError(exception);
      status = error;
      if (rethrowErrors) rethrow;
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

final class _MissionTransferCancelled implements Exception {
  const _MissionTransferCancelled();
}
