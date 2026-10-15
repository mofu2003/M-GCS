import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

void main() {
  test('航线规划移除设置页和围栏入口，围栏操作迁移至独立面板', () {
    final planningPanel = File(
      'lib/mission/mission_planning_panel.dart',
    ).readAsStringSync();
    final geofencePanel = File(
      'lib/mission/geofence_floating_panel.dart',
    ).readAsStringSync();
    final settingsMenu = File(
      'lib/widgets/mg_settings_menu.dart',
    ).readAsStringSync();
    final mapView = File('lib/map/amap_map_view.dart').readAsStringSync();
    final app = File('lib/main.dart').readAsStringSync();

    expect(planningPanel, isNot(contains('_buildSettingsTab')));
    expect(planningPanel, isNot(contains('class _FenceRallyCard')));
    expect(geofencePanel, contains('class GeofenceFloatingPanel'));
    expect(geofencePanel, contains('mission.readFenceFromVehicle'));
    expect(geofencePanel, contains('mission.uploadFence'));
    expect(geofencePanel, contains('mission.clearFence'));
    expect(settingsMenu, contains('MgSettingsDestination.geofence'));
    expect(mapView, contains('GeofenceFloatingPanel('));
    expect(app, contains('case MgSettingsDestination.geofence:'));
  });
}
