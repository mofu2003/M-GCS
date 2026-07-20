import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/features/frontstage/frontstage_models.dart';
import 'package:mgcs/features/map/mg_map_catalog.dart';
import 'package:mgcs/ui/skin/mg_skin_factory.dart';

void main() {
  test('default skin exposes MacGlass frontstage contract', () {
    final skin = createDefaultSkin();

    expect(skin.id, 'mac_glass');
    expect(skin.name, 'MacGlassSkin');
    expect(skin.tokens.radiusPanel, greaterThanOrEqualTo(18));
    expect(skin.themeData.useMaterial3, isTrue);
  });

  testWidgets('skin can build a frontstage shell from stable models', (
    WidgetTester tester,
  ) async {
    final skin = createDefaultSkin();
    final model = FrontstageModel.demoDisconnected();

    await tester.pumpWidget(
      MaterialApp(
        theme: skin.themeData,
        home: skin.buildFrontstageShell(
          model: model,
          onConnectPressed: () {},
          onDisconnectPressed: () {},
          onNavigationPressed: (_) {},
          onConnectionTransportChanged: (_) {},
          mapStyle: MgMapStyle.standard,
          onMapStyleChanged: (_) {},
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-frontstage-shell')), findsOneWidget);
  });
}
