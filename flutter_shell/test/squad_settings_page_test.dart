import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';
import 'package:mgcs_frontend/fleet/squad_controller.dart';
import 'package:mgcs_frontend/fleet/squad_models.dart';
import 'package:mgcs_frontend/fleet/squad_settings_page.dart';
import 'package:mgcs_frontend/fleet/squad_store.dart';

import 'fleet_controller_test.dart' show MemoryFleetBackend;

class _MemorySquadStore implements SquadStore {
  List<Squad> squads = const [];

  @override
  Future<List<Squad>> load() async => squads;

  @override
  Future<void> save(List<Squad> next) async => squads = List.of(next);
}

Widget _host(
  SquadController squads,
  FleetController fleet, {
  void Function(FleetBatchTarget target)? onRequestAction,
  VoidCallback? onReturnToMap,
}) => MacosTheme(
  data: MacosThemeData.light(),
  child: MaterialApp(
    home: Scaffold(
      body: SquadSettingsPage(
        scrollController: ScrollController(),
        squads: squads,
        fleet: fleet,
        onRequestAction: onRequestAction ?? (_) {},
        onReturnToMap: onReturnToMap ?? () {},
      ),
    ),
  ),
);

void main() {
  testWidgets('empty page explains why create is disabled', (tester) async {
    final backend = MemoryFleetBackend();
    final fleet = FleetController(backend);
    addTearDown(fleet.dispose);
    final squads = SquadController(fleet, store: _MemorySquadStore());
    addTearDown(squads.dispose);
    await squads.load();

    await tester.pumpWidget(_host(squads, fleet));
    await tester.pumpAndSettle();

    expect(find.text('还没有小队。'), findsOneWidget);
    expect(
      tester
          .widget<PushButton>(find.byKey(const ValueKey('squad-create')))
          .onPressed,
      isNull,
    );
    expect(tester.takeException(), isNull);
  });

  testWidgets('squad page executes confirmed members and returns to map', (
    tester,
  ) async {
    final backend = MemoryFleetBackend();
    final fleet = FleetController(backend);
    addTearDown(fleet.dispose);
    final first = fleet.openLink(FleetConnectionSpec('udp://localhost:12345'));
    final second = fleet.openLink(FleetConnectionSpec('udp://localhost:12346'));
    final squads = SquadController(fleet, store: _MemorySquadStore());
    addTearDown(squads.dispose);
    await squads.load();
    fleet.selectAllVehicles();
    final squad = await squads.createFromSelection('一队');

    FleetBatchTarget? requested;
    var returnedToMap = false;
    await tester.pumpWidget(
      _host(
        squads,
        fleet,
        onRequestAction: (target) => requested = target,
        onReturnToMap: () => returnedToMap = true,
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('一队'), findsOneWidget);
    final exec = find.byKey(
      ValueKey('squad-exec-returnToLaunch-${squad.id}'),
    );
    // 临时身份未确认：按钮不可用。
    await tester.tap(exec);
    await tester.pumpAndSettle();
    expect(requested, isNull);

    await tester.tap(find.byKey(ValueKey('squad-confirm-${squad.id}')));
    await tester.pumpAndSettle();
    await tester.tap(exec);
    await tester.pumpAndSettle();

    expect(requested, isNotNull);
    expect(requested!.action, FleetBatchAction.returnToLaunch);
    expect(requested!.vehicleIds, [first, second]);

    await tester.tap(find.byKey(ValueKey('squad-select-${squad.id}')));
    await tester.pumpAndSettle();
    expect(returnedToMap, isTrue);
    expect(tester.takeException(), isNull);
  });

  testWidgets('squad page adds current selection and removes a member', (
    tester,
  ) async {
    final backend = MemoryFleetBackend();
    final fleet = FleetController(backend);
    addTearDown(fleet.dispose);
    final first = fleet.openLink(FleetConnectionSpec('udp://localhost:12345'));
    final second = fleet.openLink(FleetConnectionSpec('udp://localhost:12346'));
    final third = fleet.openLink(FleetConnectionSpec('udp://localhost:12347'));
    for (final (index, id) in [first, second, third].indexed) {
      backend.vehicles[id] = FleetVehicleSummary(
        id: id,
        systemId: index + 1,
        endpoint: 'udp://localhost:${12345 + index}',
        connected: true,
        hardwareUid: 'uid2:0$index',
      );
    }
    fleet.refresh();
    final store = _MemorySquadStore();
    final squads = SquadController(fleet, store: store);
    addTearDown(squads.dispose);
    await squads.load();
    fleet.selectVehicles([first, second]);
    final squad = await squads.createFromSelection('一队');
    fleet.selectVehicles([third]);

    await tester.pumpWidget(_host(squads, fleet));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(ValueKey('squad-add-${squad.id}')));
    await tester.pumpAndSettle();
    expect(store.squads.single.members, hasLength(3));

    await tester.tap(find.byKey(ValueKey('squad-remove-${squad.id}-uid2:01')));
    await tester.pumpAndSettle();
    await tester.tap(find.text('移除'));
    await tester.pumpAndSettle();

    expect(store.squads.single.members, hasLength(2));
    expect(store.squads.single.members.map((member) => member.identityKey), [
      'uid2:00',
      'uid2:02',
    ]);
    expect(tester.takeException(), isNull);
  });
}
