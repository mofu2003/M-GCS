import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/fleet/fleet_backend.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';
import 'package:mgcs_frontend/widgets/mg_fleet_batch_panel.dart';

class _MemoryFleetBackend implements FleetBackend {
  final vehicles = <int, FleetVehicleSummary>{};
  final links = <int, FleetLinkSummary>{};
  int? active;
  int nextId = 1;

  @override
  int open(FleetConnectionSpec spec) {
    final id = nextId++;
    links[id] = FleetLinkSummary(
      id: id,
      routeId: id,
      vehicleId: id,
      systemId: id,
      endpoint: spec.uri,
      connected: true,
      primary: true,
    );
    vehicles[id] = FleetVehicleSummary(
      id: id,
      systemId: id,
      endpoint: spec.uri,
      connected: true,
      flightMode: 'Guided',
      headingDegrees: 90,
      altitudeMeters: 12.5,
      groundSpeedMps: 3.2,
      linkCount: 1,
      healthyLinkCount: 1,
    );
    return id;
  }

  @override
  void close(int linkId) {
    final vehicleId = links.remove(linkId)?.vehicleId ?? linkId;
    vehicles.remove(vehicleId);
    if (active == vehicleId) active = vehicles.keys.firstOrNull;
  }

  @override
  void closeRoute(int routeId) {}

  @override
  void select(int vehicleId) => active = vehicleId;

  @override
  FleetSnapshot snapshot() => FleetSnapshot(
    vehicles: vehicles.values,
    links: links.values,
    activeVehicleId: active,
  );
}

void main() {
  testWidgets(
    'fleet panel separates selection, active focus and batch request',
    (tester) async {
      tester.view.physicalSize = const Size(900, 700);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      final backend = _MemoryFleetBackend();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final first = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14550'),
      );
      final second = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14551'),
      );
      final activeSelections = <int>[];
      FleetBatchTarget? requested;

      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Align(
                alignment: Alignment.topRight,
                child: SizedBox(
                  width: 400,
                  height: 640,
                  child: MgFleetBatchPanel(
                    fleet: fleet,
                    onSelectVehicle: (vehicleId) async {
                      activeSelections.add(vehicleId);
                      fleet.selectVehicle(vehicleId);
                    },
                    onRequestAction: (target) => requested = target,
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('多机控制'), findsOneWidget);
      expect(find.byType(MacosCheckbox), findsNWidgets(2));
      expect(find.text('航向'), findsNWidgets(2));
      expect(find.text('高度'), findsNWidgets(2));
      expect(find.text('地速'), findsNWidgets(2));
      expect(find.text('90°'), findsNWidgets(2));
      expect(find.text('12.5 m'), findsNWidgets(2));
      expect(find.text('3.2 m/s'), findsNWidgets(2));

      await tester.tap(find.byType(MacosCheckbox).first);
      await tester.pumpAndSettle();
      expect(fleet.selectedVehicleIds, {first});

      await tester.tap(find.byType(MacosCheckbox).last);
      await tester.pumpAndSettle();
      expect(fleet.selectedVehicleIds, {first, second});

      await tester.tap(find.byKey(ValueKey('fleet-batch-vehicle-$second')));
      await tester.pumpAndSettle();
      expect(activeSelections, [second]);
      expect(fleet.snapshot.activeVehicleId, second);

      await tester.tap(find.text('返航'));
      await tester.pumpAndSettle();
      expect(requested, isNotNull);
      expect(requested!.action, FleetBatchAction.returnToLaunch);
      expect(requested!.vehicleIds, [first, second]);
      // 固定悬浮面板在动作下发后保持显示，不再自动关闭。
      expect(find.text('多机控制'), findsOneWidget);
      expect(tester.takeException(), isNull);
    },
  );
}
