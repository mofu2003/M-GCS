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
}

final class _FakeMissionBackend implements MissionDocumentBackend {
  MissionDocument _document = const MissionDocument(
    metadata: MissionDocumentMetadata(id: 'fake-mission'),
  );

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
  Future<JsonMap> uploadMissionDocument(int expectedRevision) async => {
    'success': true,
    'revision': expectedRevision,
  };

  @override
  Future<MissionValidationReport> validateMissionDocument() async =>
      const MissionValidationReport(valid: true);

  @override
  Future<JsonMap> verifyMissionDocument({double toleranceMeters = 0.1}) async =>
      const {'success': true, 'matches': true};

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
