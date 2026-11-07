import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection_panel.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';
import 'package:mgcs_frontend/widgets/aligned_field_row.dart';
import 'package:mgcs_frontend/widgets/mg_floating_panel.dart';
import 'package:mgcs_frontend/widgets/parameter_load_progress.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

void main() {
  for (final change in [
    'parameters',
    'message',
    'validation',
    'initialization',
  ]) {
    testWidgets('connection geometry is stable when $change changes', (
      tester,
    ) async {
      final controller = ConnectionController()
        ..initializationError = null
        ..phase = SerialConnectionPhase.connected
        ..parametersLoaded = true;
      addTearDown(controller.dispose);
      await tester.pumpWidget(
        MacosApp(
          home: Scaffold(
            body: Align(
              alignment: Alignment.topLeft,
              child: ConstrainedBox(
                constraints: const BoxConstraints(
                  maxWidth: 400,
                  maxHeight: 500,
                ),
                child: ConnectionFloatingPanel(
                  controller: controller,
                  onClose: () {},
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      if (change == 'validation') {
        await tester.tap(find.text('UDP'));
        await tester.pumpAndSettle();
      }
      final panel = tester.getRect(find.byType(MgFloatingPanel));
      final form = tester.getRect(find.byType(MacPanel));
      final button = tester.getRect(find.widgetWithText(PushButton, '连接'));
      if (change == 'parameters') {
        controller.parametersLoaded = false;
        controller.parameterReceived = 100;
        controller.parameterTotal = 1000;
      } else if (change == 'validation') {
        await tester.enterText(find.byType(MacosTextField).last, 'invalid');
      } else if (change == 'initialization') {
        controller.initializationError = '连接初始化失败。' * 8;
      } else {
        controller.message = '连接失败，请检查设备端口。' * 8;
      }
      controller.connectionChanges.value++;
      await tester.pumpAndSettle();
      expect(tester.getRect(find.byType(MgFloatingPanel)), panel);
      expect(tester.getRect(find.byType(MacPanel)), form);
      expect(tester.getRect(find.widgetWithText(PushButton, '连接')), button);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    });
  }

  for (final size in [const Size(400, 500), const Size(360, 200)]) {
    testWidgets('connection input panel is fixed and centered in $size', (
      tester,
    ) async {
      final controller = ConnectionController()..initializationError = null;
      addTearDown(controller.dispose);
      await tester.pumpWidget(
        MacosApp(
          home: Scaffold(
            body: Align(
              alignment: Alignment.topLeft,
              child: ConstrainedBox(
                constraints: BoxConstraints(
                  maxWidth: size.width,
                  maxHeight: size.height,
                ),
                child: ConnectionFloatingPanel(
                  controller: controller,
                  onClose: () {},
                ),
              ),
            ),
          ),
        ),
      );
      for (final tab in ['UDP', 'TCP']) {
        await tester.ensureVisible(find.text(tab));
        await tester.pumpAndSettle();
        await tester.tap(find.text(tab));
        await tester.pumpAndSettle();
        expect(
          controller.transport,
          tab == 'UDP' ? ConnectionTransport.udp : ConnectionTransport.tcp,
        );
        final panel = tester.getRect(find.byType(MacPanel));
        final rows = find.byType(AlignedFieldRow);
        final first = tester.getRect(rows.first);
        final last = tester.getRect(rows.last);
        expect(panel.height, 96);
        expect(first.top - panel.top, closeTo(panel.bottom - last.bottom, 0.5));
        await tester.enterText(find.byType(MacosTextField).last, 'invalid');
        await tester.pumpAndSettle();
        expect(tester.getRect(find.byType(MacPanel)), panel);
        final action = find.widgetWithText(PushButton, '连接');
        await tester.ensureVisible(action);
        await tester.pumpAndSettle();
        expect(action.hitTestable(), findsOneWidget);
        expect(tester.takeException(), isNull);
      }
      await tester.pumpWidget(const SizedBox());
    });
  }

  testWidgets('toolbar progress follows parameter updates and disconnects', (
    tester,
  ) async {
    final controller = ConnectionController()
      ..phase = SerialConnectionPhase.connected
      ..parametersLoaded = false
      ..parameterTotal = 0;
    addTearDown(controller.dispose);
    await tester.pumpWidget(
      MacosApp(
        home: Scaffold(
          body: SizedBox(
            width: 800,
            height: 52,
            child: Stack(
              children: [
                const SizedBox.expand(),
                Positioned(
                  left: 0,
                  right: 0,
                  bottom: 0,
                  height: 3,
                  child: ToolbarParameterProgress(controller: controller),
                ),
              ],
            ),
          ),
        ),
      ),
    );
    await tester.pump();
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      0.0,
    );
    controller.parameterReceived = 250;
    controller.parameterTotal = 1000;
    controller.parameterChanges.value++;
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 100));
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      allOf(greaterThan(0), lessThan(0.25)),
    );
    await tester.pump(const Duration(milliseconds: 150));
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      0.25,
    );
    expect(tester.getSize(find.byType(LinearProgressIndicator)).height, 3);
    controller.parametersLoaded = true;
    controller.parameterChanges.value++;
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 250));
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      1.0,
    );
    await tester.pump(const Duration(milliseconds: 350));
    expect(find.byType(LinearProgressIndicator), findsNothing);
    controller.parametersLoaded = false;
    controller.parameterReceived = 10;
    controller.parameterTotal = 100;
    controller.connectionChanges.value++;
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 250));
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      0.1,
    );
    controller.parametersLoaded = true;
    controller.parameterChanges.value++;
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 250));
    controller.parametersLoaded = false;
    controller.parameterChanges.value++;
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 600));
    expect(
      tester
          .widget<LinearProgressIndicator>(find.byType(LinearProgressIndicator))
          .value,
      0.1,
    );
    controller.phase = SerialConnectionPhase.disconnected;
    controller.connectionChanges.value++;
    await tester.pump();
    expect(find.byType(LinearProgressIndicator), findsNothing);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });

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
                    FleetLinkSummary(
                      id: 14,
                      routeId: -1,
                      systemId: 1,
                      endpoint: 'udp://127.0.0.1:14553',
                      connected: false,
                      state: 'duplicate_system_id',
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
    expect(find.text('4 条'), findsOneWidget);
    expect(find.text('udp://127.0.0.1:14551'), findsOneWidget);
    expect(find.text('serial:///dev/cu.test?baud=115200'), findsOneWidget);
    expect(find.text('已连接'), findsNWidgets(2));
    expect(find.text('等待心跳'), findsOneWidget);
    expect(find.text('SYSID 冲突'), findsOneWidget);
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
