import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/presentation/features/frontstage/frontstage_models.dart';
import 'package:mgcs/presentation/features/map/mg_map_catalog.dart';
import 'package:mgcs/presentation/ui/shell/mg_frontstage_shell_contract.dart';
import 'package:mgcs/presentation/ui/skin/mg_skin_factory.dart';

void main() {
  testWidgets('MacGlass frontstage uses full-map floating layout', (
    WidgetTester tester,
  ) async {
    final skin = createDefaultSkin();
    MgMapStyle? selectedMapStyle;

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: MgFrontstageShellModel(
            frontstage: FrontstageModel.demoDisconnected(),
            mapStyle: MgMapStyle.standard,
          ),
          actions: MgFrontstageShellActions(
            onConnect: () {},
            onDisconnect: () {},
            onNavigation: (_) {},
            onTransportChanged: (_) {},
            onMapStyleChanged: (style) {
              selectedMapStyle = style;
            },
          ),
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-frontstage-shell')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-map-repaint-boundary')),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('mg-map-surface')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-map-fallback-background')),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('mg-map-attribution')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-map-toolbar')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-map-style-switcher')), findsOneWidget);
    final attributionRect = tester.getRect(
      find.byKey(const ValueKey('mg-map-attribution')),
    );
    final mapSwitcherRect = tester.getRect(
      find.byKey(const ValueKey('mg-map-style-switcher')),
    );
    expect(mapSwitcherRect.width, 220);
    expect(
      find.descendant(
        of: find.byKey(const ValueKey('mg-map-style-switcher')),
        matching: find.byType(FittedBox),
      ),
      findsOneWidget,
    );
    expect(attributionRect.right, closeTo(mapSwitcherRect.right, 0.01));
    expect(attributionRect.top, greaterThan(mapSwitcherRect.bottom));
    expect(
      find.byKey(const ValueKey('mg-map-style-menu-button')),
      findsOneWidget,
    );
    expect(find.byType(MenuAnchor), findsOneWidget);
    expect(find.byIcon(Icons.keyboard_arrow_up_rounded), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-top-status-bar')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-navigation-dock')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-dashboard-transition')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-connection-transition')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-flight-info-transition')),
      findsOneWidget,
    );
    final attitudePanelSize = tester.getSize(
      find.byKey(const ValueKey('mg-attitude-panel')),
    );
    expect(attitudePanelSize.width, 450);
    expect(attitudePanelSize.height, 216);
    expect(find.byKey(const ValueKey('mg-mission-empty-state')), findsNothing);
    expect(find.text('M-GCS 前台任务工作台'), findsNothing);
    expect(find.text('地图 / 任务 / 遥测'), findsNothing);
    expect(find.text('M'), findsNothing);
    expect(find.text('GPS信息'), findsOneWidget);
    expect(find.text('飞行模式'), findsOneWidget);
    expect(find.text('解锁状态'), findsOneWidget);
    expect(find.text('电池信息'), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-battery-capacity')), findsOneWidget);
    expect(find.text('--%'), findsOneWidget);
    expect(find.text('飞控信息'), findsOneWidget);
    expect(find.text('当前无任务'), findsNothing);
    expect(find.text('新建任务'), findsNothing);
    expect(find.text('导入任务'), findsNothing);
    expect(find.text('从飞控下载'), findsNothing);
    expect(find.text('OpenStreetMap HOT 标准地图'), findsOneWidget);
    expect(
      tester.widget<Text>(find.text('OpenStreetMap HOT 标准地图')).style?.fontSize,
      11,
    );
    expect(find.text('OpenHikingMap 地形地图'), findsNothing);

    await tester.tap(find.byKey(const ValueKey('mg-map-style-menu-button')));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 250));

    expect(
      find.byKey(const ValueKey('mg-map-style-option-standard')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-map-style-option-terrain')),
      findsOneWidget,
    );
    expect(find.text('OpenHikingMap 地形地图'), findsOneWidget);
    final terrainOptionRect = tester.getRect(
      find.byKey(const ValueKey('mg-map-style-option-terrain')),
    );
    expect(terrainOptionRect.bottom, lessThan(mapSwitcherRect.top));

    await tester.tap(find.byKey(const ValueKey('mg-map-style-option-terrain')));
    await tester.pump(const Duration(milliseconds: 250));
    expect(selectedMapStyle, MgMapStyle.terrain);
  });

  test('frontstage demo model hides mission empty state by default', () {
    final model = FrontstageModel.demoDisconnected();

    expect(model.missionEmptyState.visible, isFalse);
  });
}
