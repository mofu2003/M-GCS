import 'package:mgcs_frontend/mission/mission_document_backend.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

/// `MissionDocumentBackend` 的内存替身：不接触 `dart:ffi`，按脚本驱动
/// 任务文档的增删改、传输与校验状态机，供任务控制器测试使用。
final class FakeMissionBackend implements MissionDocumentBackend {
  MissionDocument _document = const MissionDocument(
    metadata: MissionDocumentMetadata(id: 'fake-mission'),
  );
  MissionTransferProgress _transfer = const MissionTransferProgress();
  int _transferPolls = 0;
  int transferPollCalls = 0;
  int transferCompletionPolls = 2;
  int downloadStarts = 0;
  MissionDocument? downloadedDocument;
  String? downloadError;
  bool cancelCalled = false;
  int fenceDownloadStarts = 0;
  int fenceClears = 0;
  int rallyDownloadStarts = 0;
  int rallyClears = 0;
  MissionVerificationResult fenceVerificationResult =
      const MissionVerificationResult(matches: true);
  MissionVerificationResult rallyVerificationResult =
      const MissionVerificationResult(matches: true);
  final MissionTransferProgress _fenceTransfer =
      const MissionTransferProgress();
  final MissionTransferProgress _rallyTransfer =
      const MissionTransferProgress();
  MissionVerificationResult verificationResult =
      const MissionVerificationResult(
        matches: true,
        expectedCount: 1,
        vehicleCount: 1,
      );

  void _startTransfer(MissionTransferState state) {
    _transferPolls = 0;
    _transfer = MissionTransferProgress(
      state: state,
      total: 1,
      cancellable: true,
    );
  }

  @override
  Future<MissionCompileResult> compileMissionDocument() async =>
      MissionCompileResult(
        valid: true,
        documentRevision: _document.revision,
        plan: {'valid': true, 'count': _document.entities.length},
        sourceEntityIds: [for (final entity in _document.entities) entity.id],
        report: const MissionValidationReport(valid: true),
      );

  @override
  Future<MissionDocument> createMissionDocument([
    JsonMap config = const {},
  ]) async {
    _document = MissionDocument(
      metadata: MissionDocumentMetadata.fromJson(
        config['metadata'] as JsonMap? ?? const {},
      ),
    );
    return _document;
  }

  @override
  Future<MissionEstimate> estimateMissionDocument() async => MissionEstimate(
    enabledEntityCount: _document.entities.length,
    waypointCount: _document.entities.length,
    totalDistanceMeters: 100,
  );

  String interchangeText = 'QGC WPL 110\n0\t1\t0\t16\t0\t0\t0\t0\t0\t0\t0\t1\n';
  String? lastInterchangeText;
  int interchangeImportCalls = 0;
  String? interchangeImportError;
  MissionDocument? interchangeImportDocument;
  List<String> interchangeImportWarnings = const [];
  List<String> interchangeWarnings = const [];
  String? lastInterchangeFormat;

  @override
  Future<MissionInterchangeImport> importMissionInterchange(
    String format,
    String text,
  ) async {
    lastInterchangeFormat = format;
    lastInterchangeText = text;
    interchangeImportCalls++;
    if (interchangeImportError != null) {
      throw StateError(interchangeImportError!);
    }
    _document = interchangeImportDocument ?? const MissionDocument();
    return MissionInterchangeImport(
      document: _document,
      warnings: interchangeImportWarnings,
    );
  }

  @override
  Future<MissionInterchangeExport> exportMissionInterchange(
    String format,
  ) async {
    lastInterchangeFormat = format;
    return MissionInterchangeExport(
      format: format == 'plan' ? 'plan' : 'waypoints',
      itemCount: _document.entities.length,
      text: interchangeText,
      warnings: interchangeWarnings,
    );
  }

  @override
  Future<List<MissionCommandDescriptor>> getMissionCommandCatalog() async =>
      const [
        MissionCommandDescriptor(
          command: 16,
          name: 'NAV_WAYPOINT',
          label: '航点',
          entityType: 'waypoint',
          category: 'navigation',
          description: 'Fly to a position',
          requiresPosition: true,
          requiresAltitude: true,
          parameters: [],
          vehicles: ['copter', 'plane', 'vtol', 'rover'],
        ),
      ];

  @override
  Future<MissionDocument> getMissionDocument() async => _document;

  @override
  Future<MissionDocument> importMissionDocumentFromVehicle() async =>
      _document = downloadedDocument ?? _document;

  @override
  Future<MissionDocument> loadMissionDocument(String filePath) async =>
      _document;

  @override
  Future<MissionDocumentOperationResult> redoMissionDocument() async {
    _document = MissionDocument(
      revision: _document.revision + 1,
      entities: const [
        MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      ],
    );
    return MissionDocumentOperationResult(
      document: _document,
      revision: _document.revision,
    );
  }

  @override
  Future<void> saveMissionDocument(String filePath) async {}

  @override
  Future<MissionDocumentOperationResult> undoMissionDocument() async {
    _document = MissionDocument(revision: _document.revision + 1);
    return MissionDocumentOperationResult(
      document: _document,
      revision: _document.revision,
    );
  }

  @override
  Future<JsonMap> uploadMissionDocument(int expectedRevision) async {
    if (expectedRevision != _document.revision) {
      throw StateError('mission document revision mismatch');
    }
    _startTransfer(MissionTransferState.uploading);
    return {
      'success': true,
      'revision': expectedRevision,
      'state': 'uploading',
    };
  }

  @override
  Future<MissionValidationReport> validateMissionDocument() async =>
      const MissionValidationReport(valid: true);

  @override
  Future<MissionVerificationResult> verifyMissionDocument({
    double toleranceMeters = 1.0,
  }) async => verificationResult;

  @override
  Future<void> startMissionDownload() async {
    downloadStarts++;
    if (downloadError != null) throw StateError(downloadError!);
    _startTransfer(MissionTransferState.downloading);
  }

  @override
  Future<JsonMap> uploadFence(int expectedRevision) async => {
    'success': true,
    'revision': expectedRevision,
    'state': 'uploading',
  };

  @override
  Future<void> startFenceDownload() async {
    fenceDownloadStarts++;
  }

  @override
  Future<MissionTransferProgress> getFenceTransferProgress() async =>
      _fenceTransfer;

  @override
  Future<MissionDocument> importFenceFromVehicle() async => _document;

  @override
  Future<MissionVerificationResult> verifyFence({
    double toleranceMeters = 1.0,
  }) async => fenceVerificationResult;

  @override
  Future<void> clearFence() async {
    fenceClears++;
  }

  @override
  Future<JsonMap> uploadRally(int expectedRevision) async => {
    'success': true,
    'revision': expectedRevision,
    'state': 'uploading',
  };

  @override
  Future<void> startRallyDownload() async {
    rallyDownloadStarts++;
  }

  @override
  Future<MissionTransferProgress> getRallyTransferProgress() async =>
      _rallyTransfer;

  @override
  Future<MissionDocument> importRallyFromVehicle() async => _document;

  @override
  Future<MissionVerificationResult> verifyRally({
    double toleranceMeters = 1.0,
  }) async => rallyVerificationResult;

  @override
  Future<void> clearRally() async {
    rallyClears++;
  }

  @override
  Future<MissionExecutionProgress> getMissionExecutionProgress() async =>
      executionProgress;

  @override
  Future<void> clearVehicleMission() async {
    vehicleMissionClears++;
  }

  int vehicleMissionClears = 0;

  MissionExecutionProgress executionProgress = const MissionExecutionProgress(
    valid: true,
    total: 3,
  );

  @override
  Future<MissionTransferProgress> getMissionTransferProgress() async {
    transferPollCalls++;
    if (_transfer.active) {
      _transferPolls++;
      if (_transferPolls >= transferCompletionPolls) {
        _transfer = MissionTransferProgress(
          progressPercent: 100,
          total: _transfer.total,
          received: _transfer.total,
          sent: _transfer.total,
        );
      }
    }
    return _transfer;
  }

  @override
  Future<void> cancelMissionTransfer() async {
    cancelCalled = true;
    _transfer = const MissionTransferProgress(
      result: 15,
      cancelled: true,
      error: 'mission transfer cancelled',
    );
  }

  @override
  Future<MissionDocumentOperationResult> applyMissionDocumentCommand(
    JsonMap command,
  ) async {
    switch (command['operation']) {
      case 'add':
        final entity = MissionEntity.fromJson(command['entity'] as JsonMap);
        _document = _document.copyWith(
          revision: _document.revision + 1,
          entities: [
            ..._document.entities,
            entity.copyWith(id: 'entity-${_document.entities.length + 1}'),
          ],
        );
      case 'update':
        final entity = MissionEntity.fromJson(command['entity'] as JsonMap);
        _document = _document.copyWith(
          revision: _document.revision + 1,
          entities: [
            for (final existing in _document.entities)
              if (existing.id == entity.id) entity else existing,
          ],
        );
      case 'move':
        final id = command['entity_id'];
        final toIndex = command['to_index'] as int? ?? 0;
        final entities = [..._document.entities];
        final fromIndex = entities.indexWhere((entity) => entity.id == id);
        if (fromIndex >= 0) {
          final entity = entities.removeAt(fromIndex);
          entities.insert(toIndex.clamp(0, entities.length), entity);
        }
        _document = _document.copyWith(
          revision: _document.revision + 1,
          entities: [
            for (var index = 0; index < entities.length; index++)
              entities[index].copyWith(order: index),
          ],
        );
      case 'replace':
        // native 的 replace 是「整份文档原子替换」；这里保留实体并推进 revision，
        // 让「追加航点后整体替换」这类调用方拿到新的文档版本。
        _document = MissionDocument.fromJson(
          command['document'] as JsonMap,
        ).copyWith(revision: _document.revision + 1);
      case 'clear':
        _document = _document.copyWith(
          revision: _document.revision + 1,
          entities: const [],
        );
      case 'remove':
        final id = command['entity_id'];
        _document = _document.copyWith(
          revision: _document.revision + 1,
          entities: [
            for (final e in _document.entities)
              if (e.id != id) e,
          ],
        );
    }
    return MissionDocumentOperationResult(
      document: _document,
      revision: _document.revision,
    );
  }
}
