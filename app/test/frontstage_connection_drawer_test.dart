import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/features/frontstage/frontstage_models.dart';
import 'package:mgcs/features/map/mg_map_catalog.dart';
import 'package:mgcs/ui/skin/mg_skin_factory.dart';

void main() {
  testWidgets('connection drawer shows four front-end transport pages', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final selected = <FrontstageConnectionTransport>[
      FrontstageConnectionTransport.serial,
    ];
    String? selectedPort;
    int? selectedBaudRate;

    Widget build(FrontstageConnectionTransport transport) {
      final skin = createDefaultSkin();
      final model = FrontstageModel.demoDisconnected().copyWith(
        connectionDrawerVisible: true,
        connection: FrontstageModel.demoDisconnected().connection.copyWith(
          selectedTransport: transport,
          serialPorts: const [
            ConnectionSerialPort(name: 'Cube USB', path: '/dev/cu.usbmodem101'),
          ],
        ),
      );
      return MaterialApp(
        theme: skin.themeData,
        home: StatefulBuilder(
          builder: (context, setState) {
            final liveModel = model.copyWith(
              connection: model.connection.copyWith(
                selectedTransport: selected.last,
              ),
            );
            return skin.buildFrontstageShell(
              model: liveModel,
              onConnectPressed: () {},
              onDisconnectPressed: () {},
              onNavigationPressed: (_) {},
              onConnectionTransportChanged: (transport) {
                setState(() {
                  selected.add(transport);
                });
              },
              onSerialPortChanged: (port) {
                selectedPort = port;
              },
              onBaudRateChanged: (baudRate) {
                selectedBaudRate = baudRate;
              },
              mapStyle: MgMapStyle.standard,
              onMapStyleChanged: (_) {},
            );
          },
        ),
      );
    }

    await tester.pumpWidget(build(FrontstageConnectionTransport.serial));
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsOneWidget);
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-connection-drawer'))),
      const Size(330, 452),
    );
    final drawerRect = tester.getRect(
      find.byKey(const ValueKey('mg-connection-drawer')),
    );
    final dockRect = tester.getRect(
      find.byKey(const ValueKey('mg-navigation-dock')),
    );
    final attitudeRect = tester.getRect(
      find.byKey(const ValueKey('mg-attitude-panel')),
    );
    expect(drawerRect.top, closeTo(dockRect.top, 0.01));
    expect(drawerRect.left, closeTo(attitudeRect.left, 0.01));
    expect(drawerRect.bottom + 12, closeTo(attitudeRect.top, 0.01));
    expect(find.text('连接'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-connection-transport-selector')),
      findsOneWidget,
    );
    expect(find.text('Serial'), findsAtLeastNWidgets(1));
    expect(find.text('串口连接'), findsOneWidget);
    expect(find.text('UDP'), findsOneWidget);
    expect(find.text('TCP'), findsOneWidget);
    expect(find.text('蓝牙'), findsOneWidget);
    expect(find.text('传输方式'), findsNothing);
    expect(find.text('飞控识别'), findsNothing);
    expect(find.text('未识别飞控'), findsNothing);
    expect(find.text('连接飞控后进入任务工作台。'), findsNothing);
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-connection-port-row'))),
      const Size(302, 48),
    );
    expect(
      find.descendant(
        of: find.byKey(const ValueKey('mg-connection-port-row')),
        matching: find.byType(MenuAnchor),
      ),
      findsOneWidget,
    );
    expect(
      find.descendant(
        of: find.byKey(const ValueKey('mg-connection-baud-row')),
        matching: find.byType(MenuAnchor),
      ),
      findsOneWidget,
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-connection-action'))).height,
      44,
    );
    await tester.tap(find.byKey(const ValueKey('mg-connection-port-selector')));
    await tester.pump(const Duration(milliseconds: 250));
    await tester.tap(find.text('Cube USB (/dev/cu.usbmodem101)').last);
    await tester.pump(const Duration(milliseconds: 250));
    expect(selectedPort, '/dev/cu.usbmodem101');
    await tester.tap(find.byKey(const ValueKey('mg-connection-baud-selector')));
    await tester.pump(const Duration(milliseconds: 250));
    await tester.tap(find.text('57600').last);
    await tester.pump(const Duration(milliseconds: 250));
    expect(selectedBaudRate, 57600);

    await tester.tap(find.text('UDP'));
    await tester.pump(const Duration(milliseconds: 250));
    expect(find.text('0.0.0.0'), findsOneWidget);
    expect(find.text('14550'), findsOneWidget);
    expect(
      tester.getSize(
        find.byKey(const ValueKey('mg-connection-network-address')),
      ),
      const Size(302, 48),
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-connection-network-port'))),
      const Size(302, 48),
    );
    expect(find.text('本轮仅完成前端布局，暂不调用飞控连接接口。'), findsNothing);

    await tester.tap(find.text('TCP'));
    await tester.pumpAndSettle();
    expect(find.text('127.0.0.1'), findsOneWidget);
    expect(find.text('5760'), findsOneWidget);
    expect(
      tester.getSize(
        find.byKey(const ValueKey('mg-connection-network-address')),
      ),
      const Size(302, 48),
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-connection-network-port'))),
      const Size(302, 48),
    );

    await tester.tap(find.text('蓝牙'));
    await tester.pumpAndSettle();
    expect(find.text('蓝牙设备扫描'), findsOneWidget);
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-bluetooth-device-1'))),
      const Size(302, 48),
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-bluetooth-device-2'))),
      const Size(302, 48),
    );
    expect(find.text('暂未启用'), findsNothing);
    expect(
      tester.widget<OutlinedButton>(find.byType(OutlinedButton)).onPressed,
      isNull,
    );
    expect(
      tester.widget<FilledButton>(find.byType(FilledButton).last).onPressed,
      isNull,
    );
  });

  testWidgets('connection drawer is hidden outside connection navigation', (
    WidgetTester tester,
  ) async {
    final skin = createDefaultSkin();
    final connected = FrontstageModel.demoDisconnected().copyWith(
      connection: FrontstageModel.demoDisconnected().connection.copyWith(
        connected: true,
        portLabel: '/dev/cu.usbmodem1101',
        vehicleLabel: 'ArduCopter / SYSID 1',
      ),
    );

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: connected,
          onConnectPressed: () {},
          onDisconnectPressed: () {},
          onNavigationPressed: (_) {},
          onConnectionTransportChanged: (_) {},
          mapStyle: MgMapStyle.standard,
          onMapStyleChanged: (_) {},
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsNothing);
  });

  testWidgets('serial connection action invokes the application callback', (
    WidgetTester tester,
  ) async {
    var calls = 0;
    final skin = createDefaultSkin();

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: FrontstageModel.demoDisconnected().copyWith(
            connectionDrawerVisible: true,
            connection: FrontstageModel.demoDisconnected().connection.copyWith(
              portLabel: '/dev/cu.usbmodem101',
              serialPorts: const [
                ConnectionSerialPort(
                  name: 'Cube USB',
                  path: '/dev/cu.usbmodem101',
                ),
              ],
            ),
          ),
          onConnectPressed: () {
            calls++;
          },
          onDisconnectPressed: () {
            calls++;
          },
          onNavigationPressed: (_) {},
          onConnectionTransportChanged: (_) {},
          mapStyle: MgMapStyle.standard,
          onMapStyleChanged: (_) {},
        ),
      ),
    );

    await tester.tap(find.text('连接').last);
    await tester.pump(const Duration(milliseconds: 250));

    expect(calls, 1);
    expect(find.text('界面预览，尚未接入'), findsNothing);
  });

  testWidgets('UDP connection action sends the listener endpoint', (
    WidgetTester tester,
  ) async {
    String? endpoint;
    final skin = createDefaultSkin();

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: FrontstageModel.demoDisconnected().copyWith(
            connectionDrawerVisible: true,
            connection: FrontstageModel.demoDisconnected().connection.copyWith(
              selectedTransport: FrontstageConnectionTransport.udp,
            ),
          ),
          onConnectPressed: () {},
          onUdpConnectPressed: (value) {
            endpoint = value;
          },
          onDisconnectPressed: () {},
          onNavigationPressed: (_) {},
          onConnectionTransportChanged: (_) {},
          mapStyle: MgMapStyle.standard,
          onMapStyleChanged: (_) {},
        ),
      ),
    );

    await tester.tap(find.text('连接').last);
    await tester.pump(const Duration(milliseconds: 250));

    expect(endpoint, '0.0.0.0:14550');
  });

  testWidgets('TCP connection action sends the client endpoint', (
    WidgetTester tester,
  ) async {
    String? endpoint;
    final skin = createDefaultSkin();

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: FrontstageModel.demoDisconnected().copyWith(
            connectionDrawerVisible: true,
            connection: FrontstageModel.demoDisconnected().connection.copyWith(
              selectedTransport: FrontstageConnectionTransport.tcp,
            ),
          ),
          onConnectPressed: () {},
          onTcpConnectPressed: (value) {
            endpoint = value;
          },
          onDisconnectPressed: () {},
          onNavigationPressed: (_) {},
          onConnectionTransportChanged: (_) {},
          mapStyle: MgMapStyle.standard,
          onMapStyleChanged: (_) {},
        ),
      ),
    );

    await tester.tap(find.text('连接').last);
    await tester.pump(const Duration(milliseconds: 250));

    expect(endpoint, '127.0.0.1:5760');
  });
}
