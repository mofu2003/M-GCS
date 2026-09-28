import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

void main() {
  const items = [
    AppWorkspaceItem(id: 'system', label: '系统状态', icon: Icons.favorite_outline),
    AppWorkspaceItem(id: 'calibration', label: '飞控校准', icon: Icons.tune),
    AppWorkspaceItem(
      id: 'ports',
      label: '端口配置',
      icon: Icons.settings_input_antenna,
    ),
  ];

  testWidgets('sidebar navigation and search use Flutter-only controls', (
    tester,
  ) async {
    String? selected;
    var returnedToMap = false;
    await tester.pumpWidget(
      MaterialApp(
        home: AppWorkspaceShell(
          items: items,
          selectedId: 'system',
          onSelected: (id) => selected = id,
          onReturnToMap: () => returnedToMap = true,
          contentBuilder: (_, _) => const Center(child: Text('内容区域')),
        ),
      ),
    );

    expect(find.text('系统状态'), findsNWidgets(2));
    expect(find.text('内容区域'), findsOneWidget);
    await tester.tap(find.text('端口配置'));
    expect(selected, 'ports');

    await tester.enterText(find.byType(TextField), '飞控');
    await tester.pump();
    expect(find.text('飞控校准'), findsOneWidget);
    expect(find.text('端口配置'), findsNothing);

    await tester.tap(find.text('返回前台地图'));
    expect(returnedToMap, isTrue);
  });

  testWidgets('compact workspace keeps navigation and content accessible', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(780, 1200);
    tester.view.devicePixelRatio = 2;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    String? selected;
    await tester.pumpWidget(
      MaterialApp(
        home: AppWorkspaceShell(
          items: items,
          selectedId: 'system',
          onSelected: (id) => selected = id,
          onReturnToMap: () {},
          contentBuilder: (_, _) => const Center(child: Text('内容区域')),
        ),
      ),
    );

    expect(find.byType(TextField), findsNothing);
    expect(find.text('内容区域'), findsOneWidget);
    await tester.tap(find.byIcon(Icons.tune));
    expect(selected, 'calibration');
    expect(tester.takeException(), isNull);
  });

  testWidgets('workspace background follows system brightness', (tester) async {
    Future<void> show(Brightness brightness) async {
      await tester.pumpWidget(
        MaterialApp(
          home: MediaQuery(
            data: MediaQueryData(platformBrightness: brightness),
            child: AppWorkspaceShell(
              items: items,
              selectedId: 'system',
              onSelected: (_) {},
              onReturnToMap: () {},
              contentBuilder: (_, _) => const SizedBox(),
            ),
          ),
        ),
      );
    }

    await show(Brightness.light);
    final light = tester.widget<Material>(find.byType(Material).first).color;
    await show(Brightness.dark);
    final dark = tester.widget<Material>(find.byType(Material).first).color;
    expect(light, isNot(dark));
    expect(light, const Color(0xFFF6F6F6));
    expect(dark, const Color(0xFF28282A));
    expect(tester.takeException(), isNull);
  });
}
