import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_top_status_bar.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

void main() {
  Future<void> show(
    WidgetTester tester,
    MgTopStatusData data, {
    VoidCallback? onToggleArm,
  }) async {
    tester.view.physicalSize = const Size(1200, 600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: MgTopStatusBar(data: data, onToggleArm: onToggleArm),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
  }

  testWidgets('top status bar shows the disconnected state without values', (
    tester,
  ) async {
    await show(
      tester,
      const MgTopStatusData(
        connected: false,
        armed: false,
        flying: false,
        flightMode: '',
        vtolState: 0,
        gpsFixType: null,
        satellitesVisible: null,
        batteryRemainingPercent: null,
        batteryVoltage: null,
        transportLabel: '串口',
        messageCount: 0,
      ),
    );

    expect(find.text('未连接'), findsNWidgets(2));
    expect(find.text('—'), findsNWidgets(3));
    expect(tester.takeException(), isNull);
  });

  testWidgets('top status bar mirrors the active vehicle state', (
    tester,
  ) async {
    await show(
      tester,
      const MgTopStatusData(
        connected: true,
        armed: true,
        flying: true,
        flightMode: 'AUTO',
        vtolState: 4,
        gpsFixType: 3,
        satellitesVisible: 16,
        batteryRemainingPercent: 78,
        batteryVoltage: 12.6,
        transportLabel: '串口',
        messageCount: 3,
      ),
    );

    expect(find.text('飞行中'), findsOneWidget);
    expect(find.text('AUTO · 固定翼'), findsOneWidget);
    expect(find.text('16'), findsOneWidget);
    expect(find.text('78'), findsOneWidget);
    expect(find.text('串口'), findsOneWidget);
    expect(find.text('3'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  for (final width in <double>[980, 820, 760, 720, 660, 640]) {
    testWidgets(
      'top status bar lays out at ${width.toInt()} px without overflow',
      (tester) async {
        tester.view.physicalSize = Size(width, 600);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);

        await tester.pumpWidget(
          MacosTheme(
            data: MacosThemeData.light(),
            child: MaterialApp(
              home: Scaffold(
                body: MgTopStatusBar(
                  // 最宽的一档右栏：已连接、未飞行（解锁按钮可见）、高星数、满状态。
                  data: const MgTopStatusData(
                    connected: true,
                    armed: false,
                    flying: false,
                    flightMode: 'STABILIZE',
                    vtolState: 0,
                    gpsFixType: 6,
                    satellitesVisible: 18,
                    batteryRemainingPercent: 42,
                    batteryVoltage: 15.7,
                    transportLabel: 'UDP 192.168.1.20',
                    messageCount: 12,
                  ),
                  onOpenFlightMessages: () {},
                  onToggleArm: () {},
                  onOpenFlightMode: () {},
                  onOpenSettings: () {},
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull);
      },
    );
  }

  testWidgets('arm action is exposed when connected and disarmed', (
    tester,
  ) async {
    var toggled = false;
    await show(
      tester,
      const MgTopStatusData(
        connected: true,
        armed: false,
        flying: false,
        flightMode: 'STABILIZE',
        vtolState: 0,
        gpsFixType: 3,
        satellitesVisible: 12,
        batteryRemainingPercent: 80,
        batteryVoltage: 12.4,
        transportLabel: 'UDP',
        messageCount: 0,
      ),
      onToggleArm: () => toggled = true,
    );

    expect(find.text('解锁'), findsOneWidget);
    await tester.tap(find.text('解锁'));
    expect(toggled, isTrue);
    expect(tester.takeException(), isNull);
  });
  for (final width in [640.0, 760.0, 820.0, 1024.0, 1280.0]) {
    testWidgets(
      'multi-vehicle selector remains usable without overflow at $width',
      (tester) async {
        tester.view.physicalSize = Size(width, 600);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final selected = <int>[];
        await tester.pumpWidget(
          MacosTheme(
            data: MacosThemeData.light(),
            child: MaterialApp(
              home: Scaffold(
                body: MgTopStatusBar(
                  data: const MgTopStatusData(
                    connected: true,
                    armed: true,
                    flying: false,
                    flightMode: 'Auto Mission',
                    vtolState: 4,
                    gpsFixType: 6,
                    satellitesVisible: 18,
                    batteryRemainingPercent: 89,
                    batteryVoltage: 25.2,
                    transportLabel: 'UDP',
                    messageCount: 20,
                  ),
                  vehicles: const [
                    FleetVehicleSummary(
                      id: 1,
                      systemId: 1,
                      endpoint: 'udp://127.0.0.1:15001',
                      connected: true,
                    ),
                    FleetVehicleSummary(
                      id: 2,
                      systemId: 2,
                      endpoint: 'udp://127.0.0.1:15002',
                      connected: true,
                    ),
                  ],
                  activeVehicleId: 1,
                  onSelectVehicle: selected.add,
                  onOpenSettings: () {},
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(
          find.byKey(const ValueKey('fleet-vehicle-selector')),
          findsOneWidget,
        );
        expect(tester.takeException(), isNull);
        if (width < 1180) {
          await tester.tap(find.text('飞机 1').first);
          await tester.pumpAndSettle();
          await tester.tap(find.text('飞机 2').last);
          await tester.pumpAndSettle();
          expect(selected, [2]);
          expect(tester.takeException(), isNull);
        }
      },
    );
  }
}
