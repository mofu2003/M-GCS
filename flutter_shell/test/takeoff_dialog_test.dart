import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/mission/mission_planning_panel.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

void main() {
  testWidgets('起飞弹窗直接提供高度输入和确认，不显示引导模式选项', (
    tester,
  ) async {
    final controller = ConnectionController();
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light().copyWith(
          typography: MgTypography.macos(Brightness.light),
        ),
        child: MaterialApp(
          home: Builder(
            builder: (context) => TextButton(
              onPressed: () => showTakeoffDialog(
                context,
                controller: controller,
              ),
              child: const Text('打开起飞'),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开起飞'));
    await tester.pumpAndSettle();

    expect(find.byType(MacosSheet), findsOneWidget);
    expect(find.text('起飞'), findsOneWidget);
    expect(find.text('高度 (m)'), findsOneWidget);
    expect(find.text('确认'), findsOneWidget);
    expect(find.text('引导模式'), findsNothing);
    expect(find.text('暂停/继续'), findsNothing);
    expect(find.text('设置兴趣点'), findsNothing);
  });
}
