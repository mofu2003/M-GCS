import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection_panel.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

void main() {
  testWidgets('fleet link list selects and closes exact endpoints', (
    tester,
  ) async {
    final selected = <int>[];
    final disconnected = <FleetLinkSummary>[];
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Center(
              child: SizedBox(
                width: 400,
                child: MgFleetLinkList(
                  links: const [
                    FleetLinkSummary(
                      id: 11,
                      routeId: 11,
                      vehicleId: 11,
                      systemId: 2,
                      endpoint: 'udp://127.0.0.1:14551',
                      connected: true,
                      primary: true,
                    ),
                    FleetLinkSummary(
                      id: 12,
                      routeId: 12,
                      vehicleId: 12,
                      systemId: 1,
                      endpoint: 'serial:///dev/cu.test?baud=115200',
                      connected: true,
                      primary: true,
                    ),
                    FleetLinkSummary(
                      id: 13,
                      routeId: 13,
                      systemId: 0,
                      endpoint: 'udp://127.0.0.1:14552',
                      connected: false,
                      state: 'waiting_heartbeat',
                    ),
                  ],
                  activeVehicleId: 11,
                  activeLinkId: 11,
                  busy: false,
                  onSelect: selected.add,
                  onDisconnect: disconnected.add,
                ),
              ),
            ),
          ),
        ),
      ),
    );

    expect(find.text('当前链路'), findsOneWidget);
    expect(find.text('3 条'), findsOneWidget);
    expect(find.text('udp://127.0.0.1:14551'), findsOneWidget);
    expect(find.text('serial:///dev/cu.test?baud=115200'), findsOneWidget);
    expect(find.text('已连接'), findsNWidgets(2));
    expect(find.text('等待心跳'), findsOneWidget);
    expect(tester.takeException(), isNull);

    await tester.tap(find.byKey(const ValueKey('fleet-link-12:12')));
    expect(selected, [12]);

    await tester.tap(find.byKey(const ValueKey('fleet-link-disconnect-12:12')));
    expect(disconnected.map((link) => link.id), [12]);
    expect(tester.takeException(), isNull);
  });

  testWidgets('fleet link list disables destructive actions while busy', (
    tester,
  ) async {
    final disconnected = <FleetLinkSummary>[];
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: MgFleetLinkList(
              links: const [
                FleetLinkSummary(
                  id: 21,
                  routeId: 21,
                  vehicleId: 21,
                  systemId: 1,
                  endpoint: 'udp://127.0.0.1:14550',
                  connected: true,
                  primary: true,
                ),
              ],
              activeVehicleId: 21,
              activeLinkId: 21,
              busy: true,
              onSelect: (_) {},
              onDisconnect: disconnected.add,
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.byKey(const ValueKey('fleet-link-disconnect-21:21')));
    expect(disconnected, isEmpty);
    expect(tester.takeException(), isNull);
  });
}
