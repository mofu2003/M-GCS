import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/mission/mission_planning_panel.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/adaptive_popup_button.dart';

import 'support/mission_backend_fake.dart';

Widget _macosApp(Widget child) => MacosApp(
  debugShowCheckedModeBanner: false,
  theme: MacosThemeData.light().copyWith(
    typography: MgTypography.macos(Brightness.light),
  ),
  home: child,
);

Future<MissionDocumentController> _missionWithWaypoints(int count) async {
  final mission = MissionDocumentController(backend: FakeMissionBackend());
  await mission.initialize();
  await mission.create();
  for (var index = 0; index < count; index++) {
    await mission.addEntity(
      MissionEntity(
        latitude: 31.2 + index * 0.001,
        longitude: 121.4 + index * 0.001,
        altitudeMeters: 50,
      ),
    );
  }
  return mission;
}

const _catalog = [
  MissionCommandDescriptor(
    command: 16,
    name: 'NAV_WAYPOINT',
    label: '航点',
    entityType: 'waypoint',
    category: 'navigation',
    description: '飞向航点',
    requiresPosition: true,
    requiresAltitude: true,
    parameters: [],
    vehicles: ['copter', 'plane', 'vtol'],
  ),
  MissionCommandDescriptor(
    command: kMavCmdNavTakeoff,
    name: 'NAV_TAKEOFF',
    label: '起飞',
    entityType: 'takeoff',
    category: 'navigation',
    description: '起飞至设定高度',
    requiresPosition: false,
    requiresAltitude: true,
    parameters: [],
    vehicles: ['copter', 'plane', 'vtol'],
  ),
  MissionCommandDescriptor(
    command: kMavCmdNavVtolTakeoff,
    name: 'NAV_VTOL_TAKEOFF',
    label: '垂直起飞',
    entityType: 'vtol_takeoff',
    category: 'navigation',
    description: '垂直起飞至设定高度',
    requiresPosition: true,
    requiresAltitude: true,
    parameters: [],
    vehicles: ['vtol'],
  ),
];

void main() {
  testWidgets('dragging a mission row reorders it without a build failure', (
    tester,
  ) async {
    debugDefaultTargetPlatformOverride = TargetPlatform.macOS;
    final mission = await _missionWithWaypoints(3);
    mission.commandCatalog = _catalog;
    final connection = ConnectionController();
    await tester.pumpWidget(
      _macosApp(
        Scaffold(
          body: SizedBox(
            width: kMissionPanelWidth,
            height: 800,
            child: MissionPlanningFloatingPanel(
              controller: connection,
              missionController: mission,
              onClose: () {},
            ),
          ),
        ),
      ),
    );
    await tester.pump(const Duration(milliseconds: 100));

    final before = [for (final entity in mission.document!.entities) entity.id];
    expect(
      find.descendant(
        of: find.byType(ReorderableListView),
        matching: find.byType(MacosScrollbar),
      ),
      findsNothing,
    );
    final handles = find.byType(ReorderableDragStartListener);
    expect(handles, findsNWidgets(3));
    // Mission rows enter the drag Overlay. Keep LayoutBuilder-based widgets out
    // of that reparented subtree; otherwise Flutter can report
    // `_RenderLayoutBuilder was mutated` during layout.
    expect(find.byType(AdaptivePopupButton<int>), findsNothing);

    await tester.drag(
      handles.first,
      const Offset(0, 240),
      touchSlopY: 0,
      warnIfMissed: false,
    );
    await tester.pump(const Duration(milliseconds: 200));

    expect(tester.takeException(), isNull);
    final after = [for (final entity in mission.document!.entities) entity.id];
    expect(after, isNot(before));

    await tester.pumpWidget(const SizedBox.shrink());
    connection.dispose();
    mission.dispose();
    debugDefaultTargetPlatformOverride = null;
  });

  testWidgets('a new takeoff keeps the clicked position as its map reference', (
    tester,
  ) async {
    final mission = await _missionWithWaypoints(0);
    mission.commandCatalog = const [
      MissionCommandDescriptor(
        command: kMavCmdNavTakeoff,
        name: 'NAV_TAKEOFF',
        label: '起飞',
        entityType: 'takeoff',
        category: 'navigation',
        description: '起飞至设定高度',
        requiresPosition: false,
        requiresAltitude: true,
        parameters: [],
        vehicles: ['copter', 'plane', 'vtol'],
      ),
    ];
    final connection = ConnectionController();
    MissionEntity? saved;

    await tester.pumpWidget(
      _macosApp(
        Builder(
          builder: (context) => TextButton(
            onPressed: () => showMissionEntityDialog(
              context,
              controller: connection,
              missionController: mission,
              catalog: mission.commandCatalog,
              initialPosition: const LatLng(31.2345678, 121.456789),
              onSave: (entity) async => saved = entity,
            ),
            child: const Text('添加起飞'),
          ),
        ),
      ),
    );

    await tester.tap(find.text('添加起飞'));
    await tester.pumpAndSettle();
    expect(find.byType(MacosSheet), findsOneWidget);
    await tester.tap(find.text('保存'));
    await tester.pumpAndSettle();

    expect(saved?.command, kMavCmdNavTakeoff);
    expect(saved?.latitude, closeTo(31.2345678, 1e-9));
    expect(saved?.longitude, closeTo(121.456789, 1e-9));

    connection.dispose();
    mission.dispose();
  });
}
