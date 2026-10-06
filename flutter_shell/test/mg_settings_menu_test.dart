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

    await tester.tap(find.text('参数配置'));
    await tester.pumpAndSettle();
    expect(selected, MgSettingsDestination.configuration);
    expect(tester.takeException(), isNull);
  });
}
