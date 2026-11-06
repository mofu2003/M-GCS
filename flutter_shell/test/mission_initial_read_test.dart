import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

import 'support/mission_backend_fake.dart';

void main() {
  test(
    'initial connection imports vehicle route once despite bind dirty flag',
    () async {
      final backend = FakeMissionBackend()..transferCompletionPolls = 1;
      backend.downloadedDocument = const MissionDocument(
        entities: [
          MissionEntity(
            id: 'vehicle-waypoint',
            latitude: 32.5,
            longitude: 114.7,
          ),
        ],
      );
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(4);
      addTearDown(controller.dispose);
      expect(controller.dirty, isTrue);
      await controller.downloadOnInitialConnection();
      expect(controller.document!.entities.single.id, 'vehicle-waypoint');
      expect(controller.dirty, isFalse);
      expect(controller.initialized, isTrue);
      await controller.downloadOnInitialConnection();
      expect(backend.downloadStarts, 1);
    },
  );

  test(
    'initial connection imports an empty vehicle mission successfully',
    () async {
      final backend = FakeMissionBackend()..transferCompletionPolls = 1;
      backend.downloadedDocument = const MissionDocument();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);
      addTearDown(controller.dispose);
      await controller.downloadOnInitialConnection();
      expect(controller.document!.entities, isEmpty);
      expect(controller.error, isNull);
      expect(backend.downloadStarts, 1);
    },
  );

  test('initial read preserves an explicitly created empty draft', () async {
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    addTearDown(controller.dispose);
    await controller.create();
    await controller.downloadOnInitialConnection();
    expect(backend.downloadStarts, 0);
    expect(controller.dirty, isTrue);
  });

  test('initial read preserves an existing backend draft', () async {
    final backend = FakeMissionBackend();
    await backend.applyMissionDocumentCommand({
      'operation': 'add',
      'entity': const MissionEntity(id: 'draft').toJson(),
    });
    final original = (await backend.getMissionDocument()).toJson();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    addTearDown(controller.dispose);
    await controller.downloadOnInitialConnection();
    expect(backend.downloadStarts, 0);
    expect(controller.document!.toJson(), original);
  });

  test(
    'busy initial read waits and concurrent polls do not duplicate transfer',
    () async {
      final backend = FakeMissionBackend()..transferCompletionPolls = 2;
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);
      addTearDown(controller.dispose);
      controller.busy = true;
      await controller.downloadOnInitialConnection();
      expect(backend.downloadStarts, 0);
      controller.busy = false;
      await Future.wait([
        controller.downloadOnInitialConnection(),
        controller.downloadOnInitialConnection(),
      ]);
      expect(backend.downloadStarts, 1);
    },
  );

  test(
    'reconnection refreshes a downloaded route but preserves subsequent edits',
    () async {
      final backend = FakeMissionBackend()..transferCompletionPolls = 1;
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);
      addTearDown(controller.dispose);
      await controller.downloadOnInitialConnection();
      controller.resetInitialVehicleRead();
      await controller.downloadOnInitialConnection();
      expect(backend.downloadStarts, 2);
      await controller.create();
      controller.resetInitialVehicleRead();
      await controller.downloadOnInitialConnection();
      expect(backend.downloadStarts, 2);
    },
  );

  test('automatic Home adoption does not suppress the initial read', () async {
    final backend = FakeMissionBackend()..transferCompletionPolls = 1;
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    addTearDown(controller.dispose);
    await controller.initialize();
    await controller.adoptVehicleHomeIfMissing(
      latitude: 32.5,
      longitude: 114.7,
      altitudeMeters: 30,
      markDirty: false,
    );
    await controller.downloadOnInitialConnection();
    expect(backend.downloadStarts, 1);
  });

  test('returning to a session resumes an interrupted initial read', () async {
    final backend = FakeMissionBackend()..transferCompletionPolls = 1;
    backend.downloadError = '任务所属飞机已切换或断开';
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    addTearDown(controller.dispose);
    await controller.downloadOnInitialConnection();
    expect(controller.error, isNotNull);
    backend.downloadError = null;
    controller.resumeInitialVehicleRead();
    await controller.downloadOnInitialConnection();
    expect(backend.downloadStarts, 2);
    expect(controller.error, isNull);
  });

  test(
    'disposing during an automatic read produces no unhandled failure',
    () async {
      final backend = FakeMissionBackend()..transferCompletionPolls = 100;
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);
      final pending = controller.downloadOnInitialConnection();
      while (backend.downloadStarts == 0) {
        await Future<void>.delayed(Duration.zero);
      }
      final pollsAtDispose = backend.transferPollCalls;
      controller.dispose();
      await expectLater(pending, completes);
      expect(backend.transferPollCalls, pollsAtDispose);
      expect(backend.downloadStarts, 1);
    },
  );

  test('initial read refuses a detached session', () async {
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend);
    addTearDown(controller.dispose);
    await controller.downloadOnInitialConnection();
    expect(backend.downloadStarts, 0);
  });
}
