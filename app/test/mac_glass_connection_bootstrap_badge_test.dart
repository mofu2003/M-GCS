import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/features/frontstage/frontstage_models.dart';
import 'package:mgcs/ui/skins/mac_glass/mac_glass_connection_bootstrap_badge.dart';
import 'package:mgcs/ui/skins/mac_glass/mac_glass_status_bar.dart';

void main() {
  Widget buildBadge(FrontstageConnectionBootstrapModel model) {
    return MaterialApp(
      home: Scaffold(body: MacGlassConnectionBootstrapBadge(model: model)),
    );
  }

  testWidgets('shows circular parameter progress while downloading', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      buildBadge(
        const FrontstageConnectionBootstrapModel(
          state: ConnectionBootstrapState.downloading,
          progressPercent: 42,
          parameterLabel: '523 / 1046',
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-bootstrap-progress')), findsOneWidget);
    expect(find.textContaining('42%'), findsOneWidget);
    expect(find.text('正在下载参数'), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-bootstrap-identity')), findsNothing);
  });

  testWidgets(
    'shows a stable default tag before a flight controller connects',
    (WidgetTester tester) async {
      await tester.pumpWidget(
        buildBadge(const FrontstageConnectionBootstrapModel()),
      );

      expect(find.byKey(const ValueKey('mg-bootstrap-idle')), findsOneWidget);
      expect(find.text('未连接飞控'), findsOneWidget);
      expect(find.text('选择连接方式'), findsOneWidget);
    },
  );

  testWidgets('replaces progress with identity and signal labels', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      buildBadge(
        const FrontstageConnectionBootstrapModel(
          state: ConnectionBootstrapState.complete,
          name: 'M-GCS Plane',
          airframe: 'Copter',
          firmwareLabel: 'ArduPilot 4.5.7',
          linkLabel: '链路 96%',
          remoteControlLabel: '遥控器 72%',
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-bootstrap-progress')), findsNothing);
    expect(find.byKey(const ValueKey('mg-bootstrap-identity')), findsOneWidget);
    expect(find.text('M-GCS Plane'), findsOneWidget);
    expect(find.text('机架类型：Copter'), findsOneWidget);
    expect(find.text('ArduPilot 4.5.7'), findsOneWidget);
    expect(find.text('链路 96%'), findsOneWidget);
    expect(find.text('遥控器 72%'), findsOneWidget);
  });

  testWidgets('labels unavailable remote control signal explicitly', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      buildBadge(
        const FrontstageConnectionBootstrapModel(
          state: ConnectionBootstrapState.complete,
          remoteControlLabel: '遥控器 不可用',
        ),
      ),
    );

    expect(find.text('遥控器 不可用'), findsOneWidget);
  });

  testWidgets('places the flight controller tag before GPS in the status bar', (
    WidgetTester tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(1200, 800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    const bootstrap = FrontstageConnectionBootstrapModel(
      state: ConnectionBootstrapState.complete,
      name: 'M-GCS Plane',
      airframe: 'Copter',
    );
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SizedBox(
            width: 1200,
            child: MacGlassStatusBar(
              model: FrontstageModel.demoDisconnected(),
              bootstrap: bootstrap,
              outputPanelOpen: false,
              onOutputPressed: () {},
            ),
          ),
        ),
      ),
    );

    final bootstrapRect = tester.getRect(
      find.byKey(const ValueKey('mg-bootstrap-identity')),
    );
    final gpsRect = tester.getRect(find.text('GPS信息'));
    expect(bootstrapRect.left, lessThan(gpsRect.left));
    expect(gpsRect.left, greaterThan(400));
  });
}
