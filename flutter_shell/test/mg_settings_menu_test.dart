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
    expect(find.text('参数配置'), findsOneWidget);
    expect(find.text('航线规划'), findsOneWidget);

    expect(find.text('机载日志'), findsOneWidget);
    expect(find.text('机载文件'), findsOneWidget);

    await tester.tap(find.text('参数配置'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.configuration);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to survey planning', (tester) async {
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
    await tester.tap(find.text('测绘规划'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.survey);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to camera and gimbal page', (tester) async {
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
    await tester.tap(find.text('相机与云台'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.payload);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to onboard logs', (tester) async {
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
    await tester.tap(find.text('机载日志'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.logs);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to flight logs', (tester) async {
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
    await tester.tap(find.text('飞行记录'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.flightLog);
    expect(tester.takeException(), isNull);
  });

  testWidgets('settings menu routes to onboard files', (tester) async {
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
    await tester.tap(find.text('机载文件'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.files);
    expect(tester.takeException(), isNull);
  });
}
