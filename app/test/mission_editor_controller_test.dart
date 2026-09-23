import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/application/mission_models.dart';
import 'package:mgcs/features/mission/mission_editor_controller.dart';

void main() {
  test('editor supports add, reorder, validate and remove', () {
    final controller = MissionEditorController(backend: _Backend());
    controller.addWaypoint();
    controller.addWaypoint();
    expect(controller.items, hasLength(2));

    controller.moveWaypoint(1, -1);
    expect(controller.items.map((item) => item.sequence), [0, 1]);
    controller.removeWaypoint(0);
    expect(controller.items.single.sequence, 0);
    expect(controller.validation.isValid, isTrue);
    controller.dispose();
  });

  test('editor adds a map waypoint with domain coordinates and speed', () {
    final controller = MissionEditorController(backend: _Backend());

    controller.addWaypointAt(
      latitude: 31.230401,
      longitude: 121.473701,
      altitude: 80,
      speedMetersPerSecond: 6,
    );

    expect(controller.items, hasLength(1));
    expect(controller.items.single.latitude, 31.230401);
    expect(controller.items.single.longitude, 121.473701);
    expect(controller.items.single.altitudeMeters, 80);
    expect(controller.items.single.speedMetersPerSecond, 6);
    expect(controller.currentIndex, 0);
  });

  test('control operations are delegated to optional backend', () async {
    final backend = _Backend();
    final controller = MissionEditorController(backend: backend);
    controller.addWaypoint();

    await controller.upload();
    await controller.setCurrent(0);
    await controller.start();
    await controller.clear();

    expect(backend.uploaded, isTrue);
    expect(backend.current, 0);
    expect(backend.started, isTrue);
    expect(controller.items, isEmpty);
    controller.dispose();
  });

  test('mission drafts round-trip through JSON and CSV', () {
    final controller = MissionEditorController(backend: _Backend());
    controller.addWaypointAt(
      latitude: 31.2,
      longitude: 121.4,
      altitude: 70,
      speedMetersPerSecond: 5,
    );
    final json = controller.exportJson();
    final csv = controller.exportCsv();

    final jsonController = MissionEditorController(backend: _Backend());
    jsonController.importJson(json);
    expect(jsonController.items.single.latitude, 31.2);
    expect(jsonController.items.single.speedMetersPerSecond, 5);

    final csvController = MissionEditorController(backend: _Backend());
    csvController.importCsv(csv);
    expect(csvController.items.single.longitude, 121.4);
    expect(csvController.items.single.altitudeMeters, 70);

    controller.dispose();
    jsonController.dispose();
    csvController.dispose();
  });
}

final class _Backend implements MissionControlSessionBackend {
  bool uploaded = false;
  bool started = false;
  int? current;

  @override
  Future<MissionPlanSnapshot> downloadMission() async =>
      const MissionPlanSnapshot();

  @override
  Future<ConnectionOperationResult> uploadMission(
    List<MissionItemSnapshot> items,
  ) async {
    uploaded = true;
    return const ConnectionOperationResult(success: true, message: '已写入');
  }

  @override
  Future<ConnectionOperationResult> clearMission() async =>
      const ConnectionOperationResult(success: true, message: '已清空');

  @override
  Future<ConnectionOperationResult> setCurrentMission(int sequence) async {
    current = sequence;
    return const ConnectionOperationResult(success: true, message: '已设置');
  }

  @override
  Future<ConnectionOperationResult> startMission() async {
    started = true;
    return const ConnectionOperationResult(success: true, message: '已开始');
  }
}
