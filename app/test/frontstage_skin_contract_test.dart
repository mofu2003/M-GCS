import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/presentation/features/frontstage/frontstage_models.dart';
import 'package:mgcs/presentation/features/map/mg_map_catalog.dart';
import 'package:mgcs/presentation/ui/skin/mg_skin_factory.dart';
import 'package:mgcs/presentation/ui/shell/mg_frontstage_shell_contract.dart';

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
          model: MgFrontstageShellModel(
            frontstage: model,
            mapStyle: MgMapStyle.standard,
          ),
          actions: MgFrontstageShellActions(
            onConnect: () {},
            onDisconnect: () {},
            onNavigation: (_) {},
            onTransportChanged: (_) {},
            onMapStyleChanged: (_) {},
          ),
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-frontstage-shell')), findsOneWidget);
  });
}
