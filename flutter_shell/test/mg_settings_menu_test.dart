import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_settings_menu.dart';

void main() {
  testWidgets('settings menu exposes connection and configuration', (
    tester,
  ) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: false,
                    linkLabel: 'UDP',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    expect(find.text('连接飞控'), findsOneWidget);
    expect(find.text('小队'), findsOneWidget);
    expect(find.text('参数配置'), findsOneWidget);
    expect(find.text('航线规划'), findsOneWidget);

    expect(find.text('日志与文件'), findsOneWidget);
    expect(find.text('机载文件'), findsNothing);
    expect(find.text('飞行记录'), findsNothing);
    expect(find.text('软件更新'), findsOneWidget);
    expect(
      tester.getTopLeft(find.text('小队')).dy,
      lessThan(tester.getTopLeft(find.text('日志与文件')).dy),
    );

    await tester.tap(find.text('参数配置'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.configuration);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to software updates', (tester) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: true,
                    linkLabel: 'UDP',
                    updateLabel: '发现新版本 1.3.0',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    expect(find.text('发现新版本 1.3.0'), findsOneWidget);
    await tester.tap(find.text('软件更新'));
    await tester.pumpAndSettle();

    expect(selected, MgSettingsDestination.updates);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes squads back to the map overlay', (
    tester,
  ) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: true,
                    linkLabel: 'UDP',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('小队'));
    await tester.pumpAndSettle();

    expect(selected, MgSettingsDestination.squads);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu omits camera and gimbal and keeps other routes', (
    tester,
  ) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: true,
                    linkLabel: 'UDP',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    expect(find.text('相机与云台'), findsNothing);
    expect(find.text('机载日志'), findsNothing);
    expect(find.text('机载文件'), findsNothing);
    expect(find.text('飞行记录'), findsNothing);
    await tester.tap(find.text('日志与文件'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.flightData);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to the independent fence panel', (
    tester,
  ) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: true,
                    linkLabel: 'UDP',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    expect(find.text('围栏与集结点'), findsOneWidget);
    await tester.tap(find.text('围栏与集结点'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.geofence);
  });

  testWidgets('settings menu routes to unified logs and files', (tester) async {
    MgSettingsDestination? selected;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => PushButton(
                controlSize: ControlSize.regular,
                onPressed: () async {
                  selected = await showMgSettingsMenu(
                    context,
                    connected: true,
                    linkLabel: 'UDP',
                  );
                },
                child: const Text('打开设置'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开设置'));
    await tester.pumpAndSettle();
    expect(find.text('机载日志'), findsNothing);
    expect(find.text('机载文件'), findsNothing);
    expect(find.text('飞行记录'), findsNothing);
    await tester.tap(find.text('日志与文件'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.flightData);
    expect(tester.takeException(), isNull);
  });
}
