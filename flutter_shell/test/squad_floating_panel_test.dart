import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/squad_floating_panel.dart';

import 'fleet_controller_test.dart' show MemoryFleetBackend;

Widget _host({required FleetController fleet, VoidCallback? onClose}) =>
    MacosTheme(
      data: MacosThemeData.light(),
      child: MaterialApp(
        home: Scaffold(
          body: SizedBox(
            width: kSquadPanelWidth,
            height: 700,
            child: SquadFloatingPanel(
              squads: null,
              fleet: fleet,
              onRequestAction: (_) {},
              onClose: onClose ?? () {},
            ),
          ),
        ),
      ),
    );

void main() {
  testWidgets('squad floating panel closes on the map', (tester) async {
    final fleet = FleetController(MemoryFleetBackend());
    addTearDown(fleet.dispose);
    var closed = false;

    await tester.pumpWidget(_host(fleet: fleet, onClose: () => closed = true));
    await tester.pumpAndSettle();

    expect(find.text('小队'), findsOneWidget);
    expect(find.text('多机后端尚未就绪，暂时无法管理小队。'), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('squad-panel-close')));
    await tester.pumpAndSettle();

    expect(closed, isTrue);
    expect(tester.takeException(), isNull);
  });
}
