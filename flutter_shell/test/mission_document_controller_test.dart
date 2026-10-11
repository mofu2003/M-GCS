import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

import 'support/mission_backend_fake.dart';

void main() {
  test(
    'mission document controller edits and validates through backend',
    () async {
      final backend = FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

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

  test('mission document models keep opaque QGC fields on round-trip', () {
    final document = MissionDocument.fromJson({
      'schema_version': 1,
      'metadata': {'name': 'QGC import'},
      'defaults': {'frame': 3, 'altitude_m': 50, 'speed_ms': 12},
      'complex_items': [
        {
          'id': 'plan-complex-item-1',
          'item_type': 'Survey',
          'json': '{"type":"ComplexItem","complexItemType":"Survey"}',
          'entity_insert_index': 1,
        },
      ],
      'extensions': {
        'qgc_plan.root_fields': {'customTopLevel': true},
        'qgc_plan.mission_fields': {'hoverSpeed': 4.5},
      },
      'entities': [
        {
          'id': 'plan-item-1',
          'command': 16,
          'altitudeMode': 1,
          'extensions': {
            'altitudeMode': 1,
            'AMSLAltAboveTerrain': 120,
          },
        },
      ],
    });

    expect(document.complexItems, hasLength(1));
    expect(document.complexItems.single.itemType, 'Survey');
    expect(document.complexItems.single.entityInsertIndex, 1);
    expect(document.extensions['qgc_plan.mission_fields'], {
      'hoverSpeed': 4.5,
    });
    expect(document.entities.single.extensions['AMSLAltAboveTerrain'], 120);

    final restored = MissionDocument.fromJson(document.toJson());
    expect(restored.complexItems.single.toJson(), document.complexItems.single.toJson());
    expect(restored.extensions['qgc_plan.root_fields'], {
      'customTopLevel': true,
    });
    expect(restored.entities.single.extensions, {
      'altitudeMode': 1,
      'AMSLAltAboveTerrain': 120,
    });
  });

  test('transfer progress exposes retry and failure state', () {
    final progress = MissionTransferProgress.fromJson({
      'state': 'downloading',
      'progress_pct': 40,
      'received': 4,
      'total': 10,
      'retry_count': 2,
      'error': 'mission transfer timeout',
    });

    expect(progress.state, MissionTransferState.downloading);
    expect(progress.retryCount, 2);
    expect(progress.error, 'mission transfer timeout');
  });

  test('mission document preserves fence and rally layers', () {
    final document = MissionDocument.fromJson({
      'schema_version': 1,
      'metadata': {'name': 'Fence mission'},
      'rally_points': [
        {'lat': 39.9, 'lon': 116.4, 'alt_m': 30.0},
      ],
      'geo_fence': {
        'polygons': [
          {
            'inclusion': false,
            'vertices': [
              [39.9, 116.4],
              [39.91, 116.41],
              [39.92, 116.4],
            ],
          },
        ],
        'circles': [
          {'inclusion': true, 'lat': 39.91, 'lon': 116.4, 'radius_m': 120.0},
        ],
        'breach_return': {'lat': 39.89, 'lon': 116.39, 'alt_m': 25.0},
      },
    });

    expect(document.rallyPoints, hasLength(1));
    expect(document.rallyPoints.single.altitudeMeters, 30.0);
    expect(document.geoFence.polygons.single.vertices, hasLength(3));
    expect(document.geoFence.polygons.single.inclusion, isFalse);
    expect(document.geoFence.circles.single.radiusMeters, 120.0);
    expect(document.geoFence.breachReturn?.latitude, 39.89);
    expect(document.geoFence.breachReturnAltitudeMeters, 25.0);

    final restored = MissionDocument.fromJson(document.toJson());
    expect(restored.rallyPoints, hasLength(1));
    expect(restored.geoFence.polygons, hasLength(1));
    expect(restored.geoFence.circles, hasLength(1));
    expect(restored.geoFence.breachReturn, isNotNull);
  });

  test('mission entities identify VTOL takeoff and landing commands', () {
    const takeoff = MissionEntity(command: kMavCmdNavVtolTakeoff);
    const landing = MissionEntity(command: kMavCmdNavVtolLand);
    const waypoint = MissionEntity(command: 16);

    expect(takeoff.isVtolTakeoff, isTrue);
    expect(takeoff.isVtolLand, isFalse);
    expect(landing.isVtolLand, isTrue);
    expect(waypoint.isVtolTakeoff, isFalse);
  });

  test(
    'upload performs readback verification and clears dirty state',
    () async {
      final backend = FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

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
    'changing vehicle session blocks upload until task reconciliation',
    () async {
      final backend = FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );

      controller.bindSession(2);
      expect(controller.requiresVehicleReconciliation, isTrue);
      await expectLater(controller.upload(), throwsA(isA<StateError>()));

      await controller.downloadFromVehicle();
      expect(controller.requiresVehicleReconciliation, isFalse);
      expect(controller.dirty, isFalse);
    },
  );

  test(
    'download from vehicle replaces the draft and clears dirty state',
    () async {
      final backend = FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

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
      final backend = FakeMissionBackend()..transferCompletionPolls = 100;
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

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
    final backend = FakeMissionBackend()
      ..verificationResult = const MissionVerificationResult(
        differenceCount: 1,
        expectedCount: 1,
        vehicleCount: 1,
      );
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);

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
      final backend = FakeMissionBackend()
        ..verificationResult = const MissionVerificationResult(
          differenceCount: 1,
          expectedCount: 1,
          vehicleCount: 1,
        );
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

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
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);

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

  test('mission execution progress polls vehicle feedback', () async {
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);

    await controller.initialize();
    await controller.refreshExecutionProgress();
    expect(controller.executionProgress?.active, isTrue);
    expect(controller.executionProgress?.total, 3);
    expect(controller.executionProgress?.finished, isFalse);

    backend.executionProgress = const MissionExecutionProgress(
      valid: true,
      currentIndex: 2,
      total: 3,
      reachedCount: 3,
      lastReachedSeq: 3,
      finished: true,
    );
    await controller.refreshExecutionProgress();
    expect(controller.executionProgress?.finished, isTrue);
    expect(controller.executionProgress?.reachedCount, 3);
  });

  test(
    'clearing the vehicle mission leaves the local document alone',
    () async {
      final backend = FakeMissionBackend();
      final controller = MissionDocumentController(backend: backend)
        ..bindSession(1);

      await controller.initialize();
      await controller.addEntity(
        const MissionEntity(
          id: 'entity-1',
          latitude: 31.2,
          longitude: 121.4,
          altitudeMeters: 50,
        ),
      );

      await controller.clearVehicleMission();

      expect(backend.vehicleMissionClears, 1);
      expect(controller.status, contains('已清除飞控任务'));
      expect(controller.document?.entities, hasLength(1));
    },
  );
  test('command catalog filters by connected vehicle', () {
    const catalog = [
      MissionCommandDescriptor(
        command: 16,
        name: 'NAV_WAYPOINT',
        label: '航点',
        entityType: 'waypoint',
        category: 'navigation',
        description: '',
        requiresPosition: true,
        requiresAltitude: true,
        parameters: [],
        vehicles: ['copter', 'plane', 'vtol', 'rover'],
      ),
      MissionCommandDescriptor(
        command: 21,
        name: 'NAV_LAND',
        label: '降落',
        entityType: 'land',
        category: 'navigation',
        description: '',
        requiresPosition: true,
        requiresAltitude: true,
        parameters: [],
        vehicles: ['copter', 'plane', 'vtol'],
      ),
      MissionCommandDescriptor(
        command: 84,
        name: 'NAV_VTOL_TAKEOFF',
        label: '垂直起飞',
        entityType: 'vtol_takeoff',
        category: 'navigation',
        description: '',
        requiresPosition: true,
        requiresAltitude: true,
        parameters: [],
        vehicles: ['vtol'],
      ),
      MissionCommandDescriptor(
        command: 99,
        name: 'UNANNOTATED',
        label: '未标注机型',
        entityType: 'command',
        category: 'command',
        description: '',
        requiresPosition: false,
        requiresAltitude: false,
        parameters: [],
        vehicles: [],
      ),
    ];

    expect(normalizeMissionVehicleClass('FixedWing'), 'plane');
    expect(normalizeMissionVehicleClass('Rover'), 'rover');
    expect(normalizeMissionVehicleClass('unknown'), isNull);
    expect(normalizeMissionVehicleClass(null), isNull);

    bool has(List<MissionCommandDescriptor> list, int command) =>
        list.any((descriptor) => descriptor.command == command);

    final rover = filterCommandCatalogForVehicle(catalog, 'Rover');
    expect(has(rover, 16), isTrue);
    expect(has(rover, 21), isFalse);
    expect(has(rover, 84), isFalse);
    expect(has(rover, 99), isTrue, reason: '未标注机型的命令始终保留');

    final vtol = filterCommandCatalogForVehicle(catalog, 'vtol');
    expect(has(vtol, 84), isTrue);

    final kept = filterCommandCatalogForVehicle(
      catalog,
      'rover',
      keepCommands: const [21],
    );
    expect(has(kept, 21), isTrue);

    expect(filterCommandCatalogForVehicle(catalog, null).length, catalog.length);
  });

  test('mission document controller forwards interchange export format', () async {
    final backend = FakeMissionBackend();
    final controller = MissionDocumentController(backend: backend)
      ..bindSession(1);
    await controller.initialize();
    await controller.addEntity(
      const MissionEntity(
        type: 'waypoint',
        latitude: 31.2,
        longitude: 121.4,
        altitudeMeters: 50,
      ),
    );

    final dirtyBefore = controller.dirty;
    final revisionBefore = controller.document?.revision;
    final exported = await controller.exportInterchange('plan');
    expect(exported.format, 'plan');
    expect(exported.itemCount, 1);
    expect(exported.text, backend.interchangeText);
    expect(backend.lastInterchangeFormat, 'plan');

    // 导出不得改动文档、dirty 或 revision。
    expect(controller.document?.entities, hasLength(1));
    expect(controller.document?.revision, revisionBefore);
    expect(controller.dirty, dirtyBefore);
  });

}
