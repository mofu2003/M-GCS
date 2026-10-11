import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/survey/survey_controller.dart';
import 'package:mgcs_frontend/survey/survey_models.dart';

import 'support/mission_backend_fake.dart';
import 'support/survey_backend_fake.dart';

void main() {
  late FakeSurveyBackend backend;
  late SurveyController controller;

  setUp(() {
    backend = FakeSurveyBackend();
    controller = SurveyController(backend: backend);
  });

  test('生成配置交给 native，结果原样返回', () async {
    final result = await controller.generateGrid(
      const SurveyGridConfig(centerLat: 31, centerLon: 121, widthM: 300),
    );

    expect(result.valid, isTrue);
    expect(result.waypointCount, 2);
    expect(backend.lastGrid?.widthM, 300);
    expect(backend.lastGrid?.crossGrid, isFalse);
  });

  test('valid:false 的结果照实返回，不抛异常也不谎报成功', () async {
    backend.gridResult = const SurveyResult(
      valid: false,
      message: 'invalid JSON',
    );

    final result = await controller.generateGrid(
      const SurveyGridConfig(centerLat: 31, centerLon: 121),
    );

    expect(result.valid, isFalse);
    expect(result.message, 'invalid JSON');
    expect(result.waypoints, isEmpty);
  });

  test('applyToMission 把生成航点追加到任务文档并推进 revision', () async {
    final mission = MissionDocumentController(backend: FakeMissionBackend())
      ..bindSession(1);
    final result = surveyOk(waypoints: 3);

    final added = await controller.applyToMission(
      mission: mission,
      result: result,
    );

    expect(added, 3);
    expect(mission.document?.entities, hasLength(3));
    expect(mission.document?.revision, 2);
    final first = mission.document!.entities.first;
    expect(first.type, 'waypoint');
    expect(first.command, 16);
    expect(first.latitude, closeTo(31.0, 1e-9));
    expect(first.altitudeMeters, 80);
  });

  test('frame 覆盖把并入的航点统一写成 AGL（MAV_FRAME_GLOBAL_TERRAIN_ALT）', () async {
    final mission = MissionDocumentController(backend: FakeMissionBackend())
      ..bindSession(1);

    final added = await controller.applyToMission(
      mission: mission,
      result: surveyOk(waypoints: 2),
      frame: kMissionFrameTerrainAlt,
    );

    expect(added, 2);
    expect(
      mission.document!.entities.map((entity) => entity.frame),
      everyElement(kMissionFrameTerrainAlt),
    );

    // 不传 frame 时沿用 native 返回值（替身固定为 3），不擅自改帧。
    final entities = controller.entitiesFor(surveyOk(waypoints: 1), const []);
    expect(entities.single.frame, kMissionFrameRelativeAlt);
  });

  test('missionFrameLabel / kMissionFrameChoices 覆盖相对高度与地形跟随', () {
    expect(missionFrameLabel(kMissionFrameTerrainAlt), contains('AGL'));
    expect(missionFrameLabel(kMissionFrameRelativeAlt), contains('Home'));
    expect(kMissionFrameChoices.map((choice) => choice.$1), [
      kMissionFrameRelativeAlt,
      kMissionFrameTerrainAlt,
    ]);
  });

  test('命令目录缺少匹配项时回退 waypoint，不让未知命令变成死条目', () async {
    final entities = controller.entitiesFor(surveyOk(waypoints: 2), const []);
    expect(entities.map((entity) => entity.type), everyElement('waypoint'));

    // 目录里有同号命令但类型不同：以目录为准，而不是硬编码 waypoint。
    final typed = controller.entitiesFor(surveyOk(waypoints: 1), const [
      MissionCommandDescriptor(
        command: 16,
        name: 'NAV_WAYPOINT',
        label: '航点',
        entityType: 'survey-waypoint',
        category: 'navigation',
        description: '',
        requiresPosition: true,
        requiresAltitude: true,
        parameters: [],
        vehicles: ['copter'],
      ),
    ]);
    expect(typed.single.type, 'survey-waypoint');
  });

  test('无效结果不能并入任务', () async {
    final mission = MissionDocumentController(backend: FakeMissionBackend())
      ..bindSession(1);

    expect(
      () => controller.applyToMission(
        mission: mission,
        result: const SurveyResult(valid: false, message: '生成失败'),
      ),
      throwsA(isA<StateError>()),
    );
  });

  test('SurveyResult.fromJson 解析 native 契约字段', () {
    final result = SurveyResult.fromJson(<String, Object?>{
      'valid': true,
      'waypoint_count': 1,
      'flight_line_count': 2,
      'total_distance_m': 1234.5,
      'estimated_time_s': 321.0,
      'area_m2': 50000,
      'photo_count': 9,
      'waypoints': [
        <String, Object?>{
          'seq': 0,
          'command': 16,
          'frame': 3,
          'autocontinue': 1,
          'param1': 1.5,
          'param2': 0,
          'param3': 0,
          'param4': 0,
          'lat': 31.25,
          'lon': 121.5,
          'z': 80,
        },
      ],
    });

    expect(result.valid, isTrue);
    expect(result.photoCount, 9);
    expect(result.waypoints.single.latitude, closeTo(31.25, 1e-9));
    expect(result.waypoints.single.altitudeMeters, 80);
    expect(result.waypoints.single.params.first, 1.5);

    final invalid = SurveyResult.fromJson(<String, Object?>{
      'valid': false,
      'message': 'empty config',
    });
    expect(invalid.valid, isFalse);
    expect(invalid.message, 'empty config');
  });

  test('配置 JSON 使用 native 键名', () {
    const grid = SurveyGridConfig(centerLat: 31, centerLon: 121);
    final gridJson = grid.toJson();
    expect(gridJson['center_lat'], 31);
    expect(gridJson['cross_grid'], isFalse);
    expect(gridJson['return_to_start'], isTrue);
    final camera = gridJson['camera']! as Map<String, Object?>;
    expect(camera['focal_length'], 35);
    expect(camera['side_overlap'], 60);

    const corridor = SurveyCorridorConfig(
      startLat: 31,
      startLon: 121,
      endLat: 31.1,
      endLon: 121.1,
    );
    final corridorJson = corridor.toJson();
    expect(corridorJson['start_lat'], 31);
    expect(corridorJson['end_lat'], 31.1);
    expect(corridorJson['side_overlap'], 60);
  });
}
