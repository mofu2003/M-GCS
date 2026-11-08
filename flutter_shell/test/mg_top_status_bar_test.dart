import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_top_status_bar.dart';
import 'package:mgcs_frontend/widgets/mg_status_telemetry_icons.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';

void main() {
  test('guided mode only adds a class suffix for VTOL vehicles', () {
    const fixedWing = MgTopStatusData(
      connected: true,
      armed: false,
      flying: false,
      flightMode: 'Guided',
      vtolState: 0,
      isVtolVehicle: false,
      gpsFixType: 3,
      satellitesVisible: 12,
      batteryRemainingPercent: 80,
      batteryVoltage: 12.4,
      transportLabel: 'UDP',
      messageCount: 0,
    );
    const multirotor = MgTopStatusData(
      connected: true,
      armed: false,
      flying: false,
      flightMode: 'Guided',
      vtolState: 0,
      isVtolVehicle: false,
      gpsFixType: 3,
      satellitesVisible: 12,
      batteryRemainingPercent: 80,
      batteryVoltage: 12.4,
      transportLabel: 'UDP',
      messageCount: 0,
    );

    const vtol = MgTopStatusData(
      connected: true,
      armed: false,
      flying: false,
      flightMode: 'Guided',
      vtolState: 0,
      isVtolVehicle: true,
      gpsFixType: 3,
      satellitesVisible: 12,
      batteryRemainingPercent: 80,
      batteryVoltage: 12.4,
      transportLabel: 'UDP',
      messageCount: 0,
    );

    expect(fixedWing.modeLabel, 'Guided');
    expect(multirotor.modeLabel, 'Guided');
    // 兜底文案用中文，与四态后缀保持一致（避免同一行中英混排）。
    expect(vtol.modeLabel, 'Guided · 垂直起降');
  });

  test('VTOL transition labels take precedence in guided mode', () {
    const transitioning = MgTopStatusData(
      connected: true,
      armed: false,
      flying: true,
      flightMode: 'Guided',
      vtolState: 1,
      isVtolVehicle: true,
      gpsFixType: 3,
      satellitesVisible: 12,
      batteryRemainingPercent: 80,
      batteryVoltage: 12.4,
      transportLabel: 'UDP',
      messageCount: 0,
    );

    expect(transitioning.modeLabel, 'Guided · 转固定翼');
  });

  Future<void> show(
    WidgetTester tester,
    MgTopStatusData data, {
    VoidCallback? onToggleArm,
    VoidCallback? onEmergencyStop,
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
            body: MgTopStatusBar(
              data: data,
              onToggleArm: onToggleArm,
              onEmergencyStop: onEmergencyStop,
            ),
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

  testWidgets('stale armed state cannot expose arm or emergency stop', (
    tester,
  ) async {
    var invoked = 0;
    await show(
      tester,
      const MgTopStatusData(
        connected: true,
        armed: true,
        flying: false,
        flightMode: 'AUTO',
        vtolState: 0,
        gpsFixType: 3,
        satellitesVisible: 12,
        batteryRemainingPercent: 80,
        batteryVoltage: 12,
        transportLabel: 'UDP',
        messageCount: 0,
        stale: true,
      ),
      onToggleArm: () => invoked++,
      onEmergencyStop: () => invoked++,
    );
    expect(find.text('已解锁'), findsNothing);
    expect(find.text('上锁'), findsNothing);
    expect(find.text('解锁'), findsNothing);
    expect(find.text('急停'), findsNothing);
    expect(find.text('数据过期'), findsWidgets);
    expect(invoked, 0);
    expect(tester.takeException(), isNull);
  });

  testWidgets('stale telemetry is marked expired instead of shown as current', (
    tester,
  ) async {
    // 链路断开后界面仍保留最后一帧，这些值必须变成占位符并显式提示过期，
    // 否则操作者会照着冻结的星数/电量/模式继续判断。
    await show(
      tester,
      const MgTopStatusData(
        connected: false,
        armed: false,
        flying: false,
        flightMode: 'AUTO',
        vtolState: 0,
        gpsFixType: 3,
        satellitesVisible: 16,
        batteryRemainingPercent: 78,
        batteryVoltage: 12.6,
        transportLabel: '串口',
        messageCount: 0,
        stale: true,
      ),
    );

    expect(find.text('数据过期'), findsOneWidget);
    // 过期时不再显示旧链路标签，只剩状态标签的"未连接"。
    expect(find.text('未连接'), findsOneWidget);
    expect(find.text('16'), findsNothing);
    expect(find.text('78'), findsNothing);
    expect(find.text('AUTO'), findsNothing);
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
    for (final label in [
      '飞行中',
      'AUTO · 固定翼',
      'GPS',
      'BAT',
      '星',
      '%',
      '串口',
      '3',
    ]) {
      expect(tester.widget<Text>(find.text(label)).style?.fontSize, 14);
    }
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
        expect(find.text('待命'), findsOneWidget);
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
        var opened = 0;
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
                  onSelectVehicle: (_) {},
                  onOpenFleetBatch: () => opened++,
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
        expect(find.text('已解锁'), findsOneWidget);
        await tester.tap(find.byKey(const ValueKey('fleet-vehicle-selector')));
        expect(opened, 1);
        expect(tester.takeException(), isNull);
      },
    );
  }

  testWidgets(
    'vehicle identity follows mode and shows only aircraft and system IDs',
    (tester) async {
      tester.view.physicalSize = const Size(1100, 600);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      var opened = 0;
      await tester.pumpWidget(
        MacosTheme(
          data: MacosThemeData.light(),
          child: MaterialApp(
            home: Scaffold(
              body: MgTopStatusBar(
                data: const MgTopStatusData(
                  connected: true,
                  armed: false,
                  flying: false,
                  flightMode: 'Guided',
                  vtolState: 0,
                  gpsFixType: 3,
                  satellitesVisible: 12,
                  batteryRemainingPercent: 80,
                  batteryVoltage: 12.4,
                  transportLabel: 'UDP',
                  messageCount: 0,
                ),
                vehicles: const [
                  FleetVehicleSummary(
                    id: 1,
                    systemId: 1,
                    endpoint: 'udp://127.0.0.1:14550',
                    connected: true,
                  ),
                  FleetVehicleSummary(
                    id: 2,
                    systemId: 4,
                    endpoint: 'udp://127.0.0.1:14551',
                    connected: true,
                  ),
                ],
                activeVehicleId: 2,
                selectedVehicleCount: 2,
                selectedVehicleIds: const {1, 2},
                onSelectVehicle: (_) {},
                onOpenFleetBatch: () => opened++,
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('飞机 4'), findsOneWidget);
      expect(find.textContaining('已选 2 架'), findsNothing);
      expect(
        tester.getTopLeft(find.text('Guided')).dx,
        lessThan(
          tester
              .getTopLeft(find.byKey(const ValueKey('fleet-vehicle-selector')))
              .dx,
        ),
      );
      await tester.tap(find.byKey(const ValueKey('fleet-vehicle-selector')));
      expect(opened, 1);
      expect(tester.takeException(), isNull);
    },
  );

  MgStatusTelemetryIcon statusIcon(
    WidgetTester tester,
    MgStatusIconKind kind,
  ) => tester.widget<MgStatusTelemetryIcon>(
    find.byWidgetPredicate(
      (widget) => widget is MgStatusTelemetryIcon && widget.kind == kind,
    ),
  );

  testWidgets('GPS 与电池图标只在读数有效时按数值变色，无数据用中性色', (tester) async {
    MgTopStatusData data({
      required bool connected,
      int? gpsFixType,
      int? batteryPercent,
      bool stale = false,
    }) => MgTopStatusData(
      connected: connected,
      armed: false,
      flying: false,
      flightMode: 'Loiter',
      vtolState: 0,
      gpsFixType: gpsFixType,
      satellitesVisible: gpsFixType == null ? null : 12,
      batteryRemainingPercent: batteryPercent,
      batteryVoltage: batteryPercent == null ? null : 12.4,
      transportLabel: 'UDP',
      messageCount: 0,
      stale: stale,
    );

    // 未连接：两个图标都用中性色，"未知"不画成告警。
    await show(tester, data(connected: false));
    var colors = MgColors.of(tester.element(find.byType(MgTopStatusBar)));
    expect(statusIcon(tester, MgStatusIconKind.gps).color, colors.textSecondary);
    expect(
      statusIcon(tester, MgStatusIconKind.battery).color,
      colors.textSecondary,
    );

    // 读数有效：卫星 3D 定位置绿、电量 80% 置绿。
    await show(tester, data(connected: true, gpsFixType: 3, batteryPercent: 80));
    colors = MgColors.of(tester.element(find.byType(MgTopStatusBar)));
    expect(statusIcon(tester, MgStatusIconKind.gps).color, colors.success);
    expect(statusIcon(tester, MgStatusIconKind.battery).color, colors.success);

    // 读数差：2D 定位告警、电量 12% 告警。
    await show(tester, data(connected: true, gpsFixType: 2, batteryPercent: 12));
    colors = MgColors.of(tester.element(find.byType(MgTopStatusBar)));
    expect(statusIcon(tester, MgStatusIconKind.gps).color, colors.warning);
    expect(statusIcon(tester, MgStatusIconKind.battery).color, colors.danger);

    // 数据过期：按"无数据"处理，仍是中性色，不冒充当前读数。
    await show(
      tester,
      data(connected: true, gpsFixType: 3, batteryPercent: 80, stale: true),
    );
    colors = MgColors.of(tester.element(find.byType(MgTopStatusBar)));
    expect(statusIcon(tester, MgStatusIconKind.gps).color, colors.textSecondary);
    expect(
      statusIcon(tester, MgStatusIconKind.battery).color,
      colors.textSecondary,
    );
  });

  testWidgets('急停入口只在已解锁时出现，并走确认回调（U8）', (tester) async {
    tester.view.physicalSize = const Size(1600, 600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    var emergencyStops = 0;

    Widget build({required bool armed, required bool flying}) => MacosTheme(
      data: MacosThemeData.light(),
      child: MaterialApp(
        home: Scaffold(
          body: MgTopStatusBar(
            data: MgTopStatusData(
              connected: true,
              armed: armed,
              flying: flying,
              flightMode: 'Loiter',
              vtolState: 0,
              gpsFixType: 3,
              satellitesVisible: 12,
              batteryRemainingPercent: 80,
              batteryVoltage: 12.4,
              transportLabel: 'UDP',
              messageCount: 0,
            ),
            onToggleArm: () {},
            onEmergencyStop: () => emergencyStops++,
          ),
        ),
      ),
    );

    // 未解锁：没有急停，因为没有什么可停的。
    await tester.pumpWidget(build(armed: false, flying: false));
    expect(find.text('急停'), findsNothing);

    // 已解锁但未飞行：常规上锁与急停同时可见，且是两条不同的入口。
    await tester.pumpWidget(build(armed: true, flying: false));
    expect(find.text('急停'), findsOneWidget);
    expect(find.text('上锁'), findsOneWidget);

    await tester.tap(find.text('急停'));
    await tester.pump();
    expect(emergencyStops, 1);

    // 飞行中：常规上锁被禁用，急停必须仍然可触达——这正是它存在的理由。
    await tester.pumpWidget(build(armed: true, flying: true));
    expect(find.text('上锁'), findsNothing);
    expect(find.text('急停'), findsOneWidget);
    await tester.tap(find.text('急停'));
    await tester.pump();
    expect(emergencyStops, 2);
  });
}
