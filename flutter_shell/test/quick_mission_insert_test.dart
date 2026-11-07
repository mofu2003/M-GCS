import 'dart:io';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'support/mission_backend_fake.dart';

const generated = [
  MissionEntity(id: 'g1', latitude: 32.5, longitude: 114.7),
  MissionEntity(id: 'g2', latitude: 32.501, longitude: 114.701),
];
const initial = MissionDocument(
  home: MissionHome(set: true, latitude: 32.5, longitude: 114.7),
  entities: [
    MissionEntity(id: 'wp', latitude: 32.5, longitude: 114.7),
    MissionEntity(id: 'rtl', command: 20),
  ],
  defaults: MissionDefaults(altitudeMeters: 75),
  extensions: {'retained': 'value'},
  complexItems: [
    MissionComplexItem(
      id: 'complex',
      itemType: 'Survey',
      json: '{"opaque":true}',
      entityInsertIndex: 1,
    ),
  ],
);

void main() {
  test(
    'batch inserts before RTL and retains independent mission data',
    () async {
      final mission = MissionDocumentController(backend: FakeMissionBackend())
        ..bindSession(4);
      addTearDown(mission.dispose);
      await mission.initialize();
      await mission.replaceDocument(initial);
      final revision = mission.document!.revision;
      await mission.insertGeneratedWaypoints(
        generated,
        expectedRevision: revision,
        expectedSessionId: 4,
      );
      expect(mission.document!.entities.map((e) => e.id), [
        'wp',
        'g1',
        'g2',
        'rtl',
      ]);
      expect(mission.document!.home.toJson(), initial.home.toJson());
      expect(mission.document!.defaults.altitudeMeters, 75);
      expect(mission.document!.extensions, initial.extensions);
      expect(mission.document!.complexItems.single.entityInsertIndex, 3);
      expect(
        mission.document!.complexItems.single.json,
        initial.complexItems.single.json,
      );
    },
  );
  test('stale revision or different session cannot write the draft', () async {
    final mission = MissionDocumentController(backend: FakeMissionBackend())
      ..bindSession(4);
    addTearDown(mission.dispose);
    await mission.initialize();
    await mission.replaceDocument(initial);
    final revision = mission.document!.revision;
    await expectLater(
      mission.insertGeneratedWaypoints(
        generated,
        expectedRevision: revision - 1,
        expectedSessionId: 4,
      ),
      throwsStateError,
    );
    await expectLater(
      mission.insertGeneratedWaypoints(
        generated,
        expectedRevision: revision,
        expectedSessionId: 8,
      ),
      throwsStateError,
    );
    expect(mission.document!.entities.map((e) => e.id), ['wp', 'rtl']);
  });
  test(
    'native document undo restores the whole generated batch once',
    () async {
      final controller = ConnectionController();
      addTearDown(controller.dispose);
      final mission = controller.missionPlanning;
      await mission.initialize();
      await mission.replaceDocument(initial.copyWith(complexItems: const []));
      final before = mission.document!;
      await mission.insertGeneratedWaypoints(
        generated,
        expectedRevision: before.revision,
        expectedSessionId: null,
      );
      expect(mission.document!.entities.length, 4);
      await mission.undo();
      expect(mission.document!.entities.map((e) => e.id), ['wp', 'rtl']);
      expect(mission.document!.home.toJson(), before.home.toJson());
      expect(mission.document!.extensions, before.extensions);
    },
    skip: Platform.environment['MGCS_FLEET_LIBRARY'] == null
        ? 'Set MGCS_FLEET_LIBRARY for native undo check'
        : false,
  );
}
