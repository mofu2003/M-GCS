import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/demo_connection_session_backend.dart';
import 'package:mgcs/features/shell/mgcs_app.dart';
import 'package:mgcs/ui/skins/mac_glass/mac_glass_close_button.dart';

void useMinimumDesktopViewport(WidgetTester tester) {
  tester.view.physicalSize = const Size(1040, 720);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
}

Future<void> completeOverlayAnimation(WidgetTester tester) async {
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 250));
}

void main() {
  testWidgets('app renders MacGlass frontstage shell by default', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.text('M-GCS 前台任务工作台'), findsNothing);
    expect(find.byKey(const ValueKey('mg-frontstage-shell')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-map-surface')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-map-toolbar')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-map-style-switcher')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-top-status-bar')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-navigation-dock')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsNothing);
    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsOneWidget,
    );
    expect(find.text('当前无任务'), findsNothing);
  });

  testWidgets('navigation panels toggle independently', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    final attitudeRectBeforeOpening = tester.getRect(
      find.byKey(const ValueKey('mg-attitude-panel')),
    );

    await tester.tap(find.byKey(const ValueKey('mg-navigation-connection')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-connection')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsOneWidget,
    );
    final connectionRect = tester.getRect(
      find.byKey(const ValueKey('mg-connection-drawer')),
    );
    final attitudeRect = tester.getRect(
      find.byKey(const ValueKey('mg-attitude-panel')),
    );
    final dockRect = tester.getRect(
      find.byKey(const ValueKey('mg-navigation-dock')),
    );
    expect(attitudeRect, attitudeRectBeforeOpening);
    expect(connectionRect.top, closeTo(dockRect.top, 0.01));
    expect(connectionRect.left, closeTo(attitudeRect.left, 0.01));
    expect(connectionRect.bottom + 12, closeTo(attitudeRect.top, 0.01));

    await tester.tap(find.byKey(const ValueKey('mg-navigation-connection')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsNothing);
    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-connection')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('mg-navigation-mission')));
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsNothing);
    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-mission')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('mg-navigation-mission')));
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-mission')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('mg-navigation-dashboard')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsNothing);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-dashboard')),
      findsNothing,
    );
  });

  testWidgets('frontstage and backstage chrome stay on the same anchors', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    final frontHeader = tester.getRect(
      find.byKey(const ValueKey('mg-top-status-bar')),
    );
    final frontDock = tester.getRect(
      find.byKey(const ValueKey('mg-navigation-dock')),
    );
    final frontSwitch = tester.getRect(
      find.byKey(const ValueKey('mg-navigation-backstage')),
    );

    await tester.tap(find.byKey(const ValueKey('mg-navigation-backstage')));
    await tester.pumpAndSettle();

    final backstageHeader = tester.getRect(
      find.byKey(const ValueKey('mg-backstage-header')),
    );
    final backstageDock = tester.getRect(
      find.byKey(const ValueKey('mg-backstage-navigation-dock')),
    );
    final backstageSwitch = tester.getRect(
      find.byKey(const ValueKey('mg-backstage-return-frontstage')),
    );

    expect(backstageHeader, frontHeader);
    expect(backstageDock, frontDock);
    expect(backstageSwitch.center, frontSwitch.center);
    expect(backstageSwitch.size, frontSwitch.size);
  });

  testWidgets('output status opens an empty flight information panel', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.byKey(const ValueKey('mg-flight-info-popover')), findsNothing);
    expect(find.text('未连接'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('mg-output-status-chip')));
    await completeOverlayAnimation(tester);

    expect(
      find.byKey(const ValueKey('mg-flight-info-popover')),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('mg-flight-info-anchor')), findsOneWidget);
    expect(find.text('飞控信息'), findsWidgets);
    expect(find.text('演示消息'), findsNothing);
    expect(find.text('10:24:01'), findsNothing);
    expect(find.textContaining('飞控消息示例'), findsNothing);

    await tester.tap(find.byKey(const ValueKey('mg-output-status-chip')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-flight-info-popover')), findsNothing);
  });

  testWidgets('flight information panel shows disconnected state', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    await tester.tap(find.byKey(const ValueKey('mg-output-status-chip')));
    await completeOverlayAnimation(tester);

    expect(
      find.descendant(
        of: find.byKey(const ValueKey('mg-flight-info-disconnected-state')),
        matching: find.text('未连接飞控'),
      ),
      findsOneWidget,
    );
    expect(find.text('飞控连接已断开'), findsNothing);
  });

  testWidgets('connection button is front-end preview only', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    await tester.tap(find.byKey(const ValueKey('mg-navigation-connection')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsOneWidget);

    await tester.tap(find.text('连接').last);
    await tester.pump(const Duration(milliseconds: 250));

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsOneWidget);
    expect(find.text('界面预览，尚未接入'), findsNothing);
    expect(find.text('已连接'), findsNothing);
  });

  testWidgets('connection drawer can be closed while disconnected', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    await tester.tap(find.byKey(const ValueKey('mg-navigation-connection')));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsOneWidget);

    await tester.tap(find.byTooltip('关闭连接面板'));
    await completeOverlayAnimation(tester);

    expect(find.byKey(const ValueKey('mg-connection-drawer')), findsNothing);
    expect(
      find.byKey(const ValueKey('mg-navigation-active-connection')),
      findsNothing,
    );
  });

  testWidgets('connection and flight info use the shared close button', (
    WidgetTester tester,
  ) async {
    useMinimumDesktopViewport(tester);
    await tester.pumpWidget(
      MgcsApp(backendFactory: () => DemoConnectionSessionBackend()),
    );
    await tester.pump(const Duration(milliseconds: 250));

    await tester.tap(find.byKey(const ValueKey('mg-navigation-connection')));
    await tester.tap(find.byKey(const ValueKey('mg-output-status-chip')));
    await completeOverlayAnimation(tester);

    expect(find.byType(MacGlassCloseButton), findsNWidgets(2));
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-close-connection'))),
      const Size.square(28),
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-close-flight-info'))),
      const Size.square(28),
    );
  });
}
