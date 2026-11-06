import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/map/amap_map_view.dart';
import 'package:mgcs_frontend/mission/mission_document_controller.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';
import 'package:mgcs_frontend/mission/mission_planning_panel.dart';
import 'package:mgcs_frontend/widgets/mg_floating_panel.dart';

import 'support/mission_backend_fake.dart';

void main() {
  for (final mapHeight in [900.0, 500.0, 392.0, 302.0]) {
    testWidgets(
      'planning slot aligns below status bar in map space $mapHeight',
      (tester) async {
        await tester.binding.setSurfaceSize(const Size(1400, 1000));
        addTearDown(() => tester.binding.setSurfaceSize(null));
        final connection = ConnectionController();
        final mission = MissionDocumentController(
          backend: FakeMissionBackend(),
        );
        await mission.initialize();
        await mission.create();
        for (var i = 0; i < 12; i++) {
          await mission.addEntity(
            MissionEntity(
              latitude: 31.2 + i * 0.001,
              longitude: 121.4,
              altitudeMeters: 50,
            ),
          );
        }
        await tester.pumpWidget(
          MacosApp(
            home: Scaffold(
              body: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const SizedBox(
                    key: ValueKey('status-bar'),
                    width: 1400,
                    height: 48,
                  ),
                  SizedBox(
                    width: 1400,
                    height: mapHeight,
                    child: Stack(
                      children: [
                        MapPlanningPanelSlot(
                          child: MissionPlanningFloatingPanel(
                            controller: connection,
                            missionController: mission,
                            onClose: () {},
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
        );
        await tester.pump();
        final panel = find.descendant(
          of: find.byType(MissionPlanningFloatingPanel),
          matching: find.byType(MgFloatingPanel),
        );
        final rect = tester.getRect(panel);
        final statusBar = tester.getRect(
          find.byKey(const ValueKey('status-bar')),
        );
        expect(rect.top - statusBar.bottom, 12);
        expect(rect.right, 1382);
        final bottomInset = mapHeight < 450 ? 90.0 : 226.0;
        expect(rect.height, mapHeight - 12 - bottomInset);
        // Retain the existing 18 + 196 + 12 clearance above the flight HUD.
        expect(rect.bottom, statusBar.bottom + mapHeight - bottomInset);
        final list = find.byType(ReorderableListView);
        final scrollable = tester.state<ScrollableState>(
          find.descendant(of: list, matching: find.byType(Scrollable)).first,
        );
        expect(scrollable.position.maxScrollExtent, greaterThan(0));
        await tester.ensureVisible(list);
        await tester.pump();
        await tester.drag(list, const Offset(0, -180));
        await tester.pump();
        expect(scrollable.position.pixels, greaterThan(0));
        await tester.tap(find.byTooltip('折叠航线规划'));
        await tester.pump();
        expect(panel, findsNothing);
        await tester.tap(find.byTooltip('展开航线规划'));
        await tester.pump();
        expect(tester.getRect(panel), rect);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox.shrink());
        connection.dispose();
        mission.dispose();
      },
    );
  }
}
