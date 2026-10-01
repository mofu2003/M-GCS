import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_backend.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

void main() {
  test(
    'mission document controller edits and validates through backend',
    () async {
      final backend = _FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend);

      await controller.initialize();
      expect(controller.initialized, isTrue);
      expect(controller.commandCatalog.single.name, 'NAV_WAYPOINT');
      expect(controller.document?.entities, isEmpty);

      await controller.addEntity(
        const MissionEntity(
          type: 'waypoint',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );
      expect(controller.document?.entities, hasLength(1));
      expect(controller.document?.revision, 2);

      await controller.validate();
      expect(controller.validation?.valid, isTrue);

      await controller.compile();
      expect(controller.compileResult?.valid, isTrue);
      expect(controller.compileResult?.sourceEntityIds, ['entity-1']);

      await controller.refreshEstimate();
      expect(controller.estimate?.enabledEntityCount, 1);

      await controller.undo();
      expect(controller.document?.entities, isEmpty);
      expect(controller.document?.revision, 3);

      await controller.redo();
      expect(controller.document?.entities, hasLength(1));
    },
  );

  test('mission document models round-trip core JSON fields', () {
    const entity = MissionEntity(
      id: 'takeoff-1',
      type: 'takeoff',
      command: 22,
      altitudeMeters: 30,
      params: [15, 0, 0, 0],
    );
    final restored = MissionEntity.fromJson(entity.toJson());
    expect(restored.id, 'takeoff-1');
    expect(restored.command, 22);
    expect(restored.altitudeMeters, 30);
    expect(restored.params, [15, 0, 0, 0]);
  });

  test(
    'upload performs readback verification and clears dirty state',
    () async {
      final backend = _FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );
      expect(controller.dirty, isTrue);

      final verification = await controller.upload();

      expect(verification?.matches, isTrue);
      expect(controller.dirty, isFalse);
      expect(backend.downloadStarts, 1);
      expect(controller.status, '上传并回读核对通过');
    },
  );

  test(
    'download from vehicle replaces the draft and clears dirty state',
    () async {
      final backend = _FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );
      expect(controller.dirty, isTrue);

      await controller.downloadFromVehicle();

      expect(controller.dirty, isFalse);
      expect(backend.downloadStarts, 1);
      expect(controller.status, startsWith('已从飞控读取'));
    },
  );

  test(
    'cancel aborts an active upload without clearing local changes',
    () async {
      final backend = _FakeMissionBackend()..transferCompletionPolls = 100;
      final controller = MissionDocumentController(backend: backend);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );

      final upload = controller.upload();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(controller.transferActive, isTrue);

      await controller.cancelTransfer();
      await upload;

      expect(backend.cancelCalled, isTrue);
      expect(controller.transferActive, isFalse);
      expect(controller.dirty, isTrue);
      expect(controller.status, contains('取消'));
    },
  );

  test('readback mismatch keeps the local mission dirty', () async {
    final backend = _FakeMissionBackend()
      ..verificationResult = const MissionVerificationResult(
        differenceCount: 1,
        expectedCount: 1,
        vehicleCount: 1,
      );
    final controller = MissionDocumentController(backend: backend);

    await controller.initialize();
    await controller.addEntity(
      const MissionEntity(
        id: 'entity-1',
        latitude: 31.2,
        longitude: 121.4,
        altitudeMeters: 50,
      ),
    );

    final verification = await controller.upload();

    expect(verification?.matches, isFalse);
    expect(controller.dirty, isTrue);
    expect(controller.status, contains('1 处差异'));
  });

  test(
    'adopting the downloaded vehicle mission clears the local mismatch',
    () async {
      final backend = _FakeMissionBackend()
        ..verificationResult = const MissionVerificationResult(
          differenceCount: 1,
          expectedCount: 1,
          vehicleCount: 1,
        );
      final controller = MissionDocumentController(backend: backend);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );
      await controller.upload();
      expect(controller.dirty, isTrue);

      await controller.adoptDownloadedVehicleMission();

      expect(controller.dirty, isFalse);
      expect(controller.verification, isNull);
      expect(controller.status, contains('已采用飞控任务'));
    },
  );

  test('fence and rally flows upload, read back and verify', () async {
    final backend = _FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend);

    await controller.initialize();
    await controller.addEntity(
      const MissionEntity(
        id: 'entity-1',
        latitude: 31.2,
        longitude: 121.4,
        altitudeMeters: 50,
      ),
    );

    final fence = await controller.uploadFence();
    expect(fence?.matches, isTrue);
    expect(controller.status, contains('围栏已下发并回读核对通过'));

    await controller.readFenceFromVehicle();
    expect(backend.fenceDownloadStarts, 2);
    expect(controller.status, contains('已从飞控读取围栏'));

    await controller.clearFence();
    expect(backend.fenceClears, 1);

    final rally = await controller.uploadRally();
    expect(rally?.matches, isTrue);
    expect(controller.status, contains('Rally 点已下发并回读核对通过'));

    await controller.readRallyFromVehicle();
    expect(backend.rallyDownloadStarts, 2);

    await controller.clearRally();
    expect(backend.rallyClears, 1);
  });
}

final class _FakeMissionBackend implements MissionDocumentBackend {
  MissionDocument _document = const MissionDocument(
    metadata: MissionDocumentMetadata(id: 'fake-mission'),
  );
  MissionTransferProgress _transfer = const MissionTransferProgress();
  int _transferPolls = 0;
  int transferCompletionPolls = 2;
  int downloadStarts = 0;
  bool cancelCalled = false;
  int fenceDownloadStarts = 0;
  int fenceClears = 0;
  int rallyDownloadStarts = 0;
  int rallyClears = 0;
  MissionVerificationResult fenceVerificationResult =
      const MissionVerificationResult(matches: true);
  MissionVerificationResult rallyVerificationResult =
      const MissionVerificationResult(matches: true);
  final MissionTransferProgress _fenceTransfer = const MissionTransferProgress();
  final MissionTransferProgress _rallyTransfer = const MissionTransferProgress();
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

  @override
  Future<String> exportMissionDocument() async => '{"schema_version":1}';

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
        ),
      ];

  @override
  Future<MissionDocument> getMissionDocument() async => _document;

  @override
  Future<MissionDocument> importMissionDocument(String json) async {
    _document = const MissionDocument();
    return _document;
  }

  @override
  Future<MissionDocument> importMissionDocumentFromVehicle() async => _document;

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
    double toleranceMeters = 0.1,
  }) async => verificationResult;

  @override
  Future<void> startMissionDownload() async {
    downloadStarts++;
    _startTransfer(MissionTransferState.downloading);
  }

  @override
  Future<JsonMap> compileFencePlan() async => const {'success': true, 'item_count': 0};

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
    double toleranceMeters = 0.1,
  }) async => fenceVerificationResult;

  @override
  Future<void> clearFence() async {
    fenceClears++;
  }

  @override
  Future<JsonMap> compileRallyPlan() async => const {'success': true, 'item_count': 0};

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
    double toleranceMeters = 0.1,
  }) async => rallyVerificationResult;

  @override
  Future<void> clearRally() async {
    rallyClears++;
  }

  @override
  Future<MissionTransferProgress> getMissionTransferProgress() async {
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
    if (command['operation'] == 'add') {
      final entity = MissionEntity.fromJson(command['entity'] as JsonMap);
      _document = MissionDocument(
        revision: _document.revision + 1,
        entities: [
          ..._document.entities,
          entity.copyWith(id: 'entity-${_document.entities.length + 1}'),
        ],
      );
    }
    return MissionDocumentOperationResult(
      document: _document,
      revision: _document.revision,
    );
  }
}
