import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/fleet/fleet_backend.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';
import 'package:mgcs_frontend/fleet/squad_controller.dart';
import 'package:mgcs_frontend/fleet/squad_models.dart';
import 'package:mgcs_frontend/fleet/squad_store.dart';
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
      flightMode: id == 1 ? 'Guided' : 'FBWA',
      headingDegrees: 90,
      altitudeMeters: 42.5,
      relativeAltitudeMeters: 12.5,
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

class _MemorySquadStore implements SquadStore {
  List<Squad> squads = const [];

  @override
  Future<List<Squad>> load() async => squads;

  @override
  Future<void> save(List<Squad> next) async => squads = List.of(next);
}

void main() {
  testWidgets('batch panel shows progress and stops pending dispatch', (
    tester,
  ) async {
    final ack = Completer<FleetBatchCommandResult>();
    final fleet = FleetController(
      _MemoryFleetBackend(),
      executeBatchCommand: (v, a) => ack.future,
    );
    addTearDown(fleet.dispose);
    fleet.openLink(FleetConnectionSpec('udp://localhost:12345'));
    fleet.openLink(FleetConnectionSpec('udp://localhost:12346'));
    fleet.selectAllVehicles();
    final pending = fleet.runBatch(
      fleet.captureBatchTarget(FleetBatchAction.arm),
    );
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 360,
              height: 500,
              child: MgFleetBatchPanel(
                fleet: fleet,
                onSelectVehicle: (_) async {},
                onRequestAction: (_) {},
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pump();
    expect(find.text('批量解锁 · 已处理 0/2'), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('fleet-batch-stop')));
    await tester.pump();
    expect(find.text('已请求停止后续派发'), findsOneWidget);
    ack.complete(
      const FleetBatchCommandResult.success(vehicleId: 1, systemId: 1),
    );
    await pending;
    await tester.pump();
    expect(find.text('批量解锁 · 已处理 2/2'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  for (final height in [200.0, 300.0, 500.0]) {
    testWidgets('fleet actions remain reachable at height $height', (
      tester,
    ) async {
      final backend = _MemoryFleetBackend();
      final fleet = FleetController(backend);
      fleet.openLink(FleetConnectionSpec('udp://localhost:12345'));
      fleet.openLink(FleetConnectionSpec('udp://localhost:12346'));
      addTearDown(fleet.dispose);
      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Align(
                alignment: Alignment.topLeft,
                child: SizedBox(
                  width: 360,
                  height: height,
                  child: MgFleetBatchPanel(
                    fleet: fleet,
                    onSelectVehicle: (_) async {},
                    onRequestAction: (_) {},
                    onClose: () {},
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      final action = find.byKey(const ValueKey('fleet-batch-mission-start'));
      await tester.ensureVisible(action);
      await tester.pumpAndSettle();
      expect(tester.getRect(action).bottom, lessThanOrEqualTo(height));
      expect(tester.takeException(), isNull);
    });
  }

  testWidgets('duplicate sysids show endpoints and refuse batch confirmation', (
    tester,
  ) async {
    final backend = _MemoryFleetBackend();
    final fleet = FleetController(backend);
    addTearDown(fleet.dispose);
    fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14550'));
    fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14551'));
    backend.vehicles[2] = const FleetVehicleSummary(
      id: 2,
      systemId: 1,
      endpoint: 'udp://127.0.0.1:14551',
      connected: true,
    );
    fleet.refresh();
    fleet.selectAllVehicles();
    var requests = 0;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 360,
              height: 500,
              child: MgFleetBatchPanel(
                fleet: fleet,
                onSelectVehicle: (_) async {},
                onRequestAction: (_) => requests++,
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(find.text('飞机 1'), findsNWidgets(2));
    expect(find.text('127.0.0.1:14550'), findsOneWidget);
    expect(find.text('127.0.0.1:14551'), findsOneWidget);
    expect(find.textContaining('SYSID 重复'), findsOneWidget);
    final arm = find.byKey(const ValueKey('fleet-batch-arm'));
    await tester.ensureVisible(arm);
    await tester.tap(arm);
    await tester.pumpAndSettle();
    expect(requests, 0);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('fleet panel selects a squad by its executable members', (
    tester,
  ) async {
    final backend = _MemoryFleetBackend();
    final fleet = FleetController(backend);
    addTearDown(fleet.dispose);
    final first = fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14550'));
    final second = fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14551'));
    backend.vehicles[first] = FleetVehicleSummary(
      id: first,
      systemId: 1,
      endpoint: 'udp://127.0.0.1:14550',
      connected: true,
      hardwareUid: 'uid2:aa',
    );
    backend.vehicles[second] = FleetVehicleSummary(
      id: second,
      systemId: 2,
      endpoint: 'udp://127.0.0.1:14551',
      connected: true,
      hardwareUid: 'uid2:bb',
    );
    fleet.refresh();
    final store = _MemorySquadStore()
      ..squads = const [
        Squad(
          id: 's1',
          name: '一队',
          members: [
            SquadMember(identityKey: 'uid2:aa', label: '飞机 1'),
            SquadMember(identityKey: 'uid2:bb', label: '飞机 2'),
            SquadMember(identityKey: 'uid2:missing', label: '飞机 3'),
          ],
        ),
      ];
    final squads = SquadController(fleet, store: store);
    addTearDown(squads.dispose);
    await squads.load();

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 360,
              height: 600,
              child: MgFleetBatchPanel(
                fleet: fleet,
                squads: squads,
                onSelectVehicle: (_) async {},
                onRequestAction: (_) {},
              ),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('手动勾选'), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('fleet-squad-selector')));
    await tester.pumpAndSettle();
    await tester.tap(find.text('一队 · 2/3 在线'));
    await tester.pumpAndSettle();

    expect(fleet.selectedVehicleIds, {first, second});
    expect(find.text('已按「一队」选中 2 架 · 1 架离线或冲突'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

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
      final modeRequests = <int>[];
      FleetBatchTarget? requested;

      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: Align(
                alignment: Alignment.topRight,
                child: SizedBox(
                  width: 360,
                  height: 640,
                  child: MgFleetBatchPanel(
                    fleet: fleet,
                    onSelectVehicle: (vehicleId) async {
                      activeSelections.add(vehicleId);
                      fleet.selectVehicle(vehicleId);
                    },
                    onRequestAction: (target) => requested = target,
                    onOpenFlightMode: modeRequests.add,
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('多机控制'), findsOneWidget);
      expect(find.text('2 架飞机'), findsNothing);
      expect(find.text('选择飞机后可批量下发动作'), findsNothing);
      expect(find.text('选择'), findsNothing);
      expect(find.text('当前'), findsNothing);
      expect(find.text('航向'), findsNWidgets(2));
      expect(find.text('相对高度'), findsNWidgets(2));
      expect(find.text('高度'), findsNothing);
      expect(find.text('地速'), findsNWidgets(2));
      expect(find.text('90°'), findsNWidgets(2));
      expect(find.text('12.5 m'), findsNWidgets(2));
      expect(find.text('42.5 m'), findsNothing);
      expect(find.text('3.2 m/s'), findsNWidgets(2));

      final modeButton = find.byKey(ValueKey('fleet-mode-$first'));
      final vehicleName = tester.getRect(find.text('飞机 $first'));
      final modeRect = tester.getRect(modeButton);
      expect(modeRect.left, greaterThan(vehicleName.right));
      expect(modeRect.center.dy, closeTo(vehicleName.center.dy, 1));
      expect(modeRect.width, 100);
      expect(
        tester.getSize(find.byKey(ValueKey('fleet-mode-$second'))).width,
        modeRect.width,
      );
      expect(find.text('模式：Guided'), findsNothing);

      final firstSelection = find.byKey(ValueKey('fleet-select-$first'));
      expect(tester.getSize(firstSelection).width, greaterThanOrEqualTo(40));
      await tester.tap(firstSelection);
      await tester.pumpAndSettle();
      expect(fleet.selectedVehicleIds, {first});
      expect(find.text('已选'), findsNothing);
      expect(activeSelections, isEmpty);

      await tester.tap(find.byKey(ValueKey('fleet-select-$second')));
      await tester.pumpAndSettle();
      expect(fleet.selectedVehicleIds, {first, second});
      expect(find.text('2 架飞机 · 已选 2'), findsNothing);
      expect(find.text('已选择 2 架飞机'), findsNothing);

      await tester.tap(find.byKey(ValueKey('fleet-mode-$second')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(ValueKey('fleet-mode-$first')));
      await tester.pumpAndSettle();
      expect(modeRequests, [second, first]);
      expect(activeSelections, isEmpty);
      expect(fleet.selectedVehicleIds, {first, second});

      await tester.tap(find.text('飞机 $second'));
      await tester.pumpAndSettle();
      expect(activeSelections, [second]);
      expect(fleet.snapshot.activeVehicleId, second);
      expect(find.text('当前'), findsNothing);

      await tester.tap(find.text('返航'));
      await tester.pumpAndSettle();
      expect(requested, isNotNull);
      expect(requested!.action, FleetBatchAction.returnToLaunch);
      expect(requested!.vehicleIds, [first, second]);
      // 固定悬浮面板在动作下发后保持显示，不再自动关闭。
      expect(find.text('多机控制'), findsOneWidget);

      // 遥测过期后保留最后的模式展示，但不允许通过入口下发切换。
      backend.vehicles[second] = FleetVehicleSummary(
        id: second,
        systemId: second,
        endpoint: 'udp://127.0.0.1:14551',
        connected: true,
        heartbeatFresh: false,
        flightMode: 'LoiterAltQLand',
      );
      fleet.refresh();
      await tester.pumpAndSettle();
      expect(
        tester.getSize(find.byKey(ValueKey('fleet-mode-$second'))).width,
        modeRect.width,
      );
      expect(
        tester
            .widget<PushButton>(find.byKey(ValueKey('fleet-mode-$second')))
            .onPressed,
        isNull,
      );
      expect(modeRequests, [second, first]);
      expect(tester.takeException(), isNull);
    },
  );
}
