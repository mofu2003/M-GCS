import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_dialog.dart';

void main() {
  testWidgets('alert dialogs fade in without scale or slide transitions', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Builder(
            builder: (context) => TextButton(
              onPressed: () => showMgAlertDialog<void>(
                context: context,
                builder: (dialogContext) => MacosAlertDialog(
                  appIcon: const Icon(Icons.info_outline, size: 44),
                  title: const Text('测试对话框'),
                  message: const Text('测试内容'),
                  primaryButton: PushButton(
                    controlSize: ControlSize.large,
                    onPressed: () => Navigator.of(dialogContext).pop(),
                    child: const Text('好'),
                  ),
                ),
              ),
              child: const Text('打开'),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开'));
    await tester.pump();

    final dialog = find.byType(MacosAlertDialog);
    expect(
      find.ancestor(of: dialog, matching: find.byType(FadeTransition)),
      findsWidgets,
    );
    expect(
      find.ancestor(of: dialog, matching: find.byType(ScaleTransition)),
      findsNothing,
    );
    expect(
      find.ancestor(of: dialog, matching: find.byType(SlideTransition)),
      findsNothing,
    );

    await tester.pumpAndSettle();
    expect(find.text('测试对话框'), findsOneWidget);
  });

  testWidgets('sheets fade in without scale or slide transitions', (
    tester,
  ) async {
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Builder(
            builder: (context) => TextButton(
              onPressed: () => showMgSheet<void>(
                context: context,
                builder: (sheetContext) => Center(
                  child: MacosSheet(
                    child: const SizedBox(
                      width: 220,
                      height: 100,
                      child: Center(child: Text('测试面板')),
                    ),
                  ),
                ),
              ),
              child: const Text('打开面板'),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('打开面板'));
    await tester.pump();

    final sheet = find.byType(MacosSheet);
    expect(
      find.ancestor(of: sheet, matching: find.byType(FadeTransition)),
      findsWidgets,
    );
    expect(
      find.ancestor(of: sheet, matching: find.byType(ScaleTransition)),
      findsNothing,
    );
    expect(
      find.ancestor(of: sheet, matching: find.byType(SlideTransition)),
      findsNothing,
    );

    await tester.pumpAndSettle();
    expect(find.text('测试面板'), findsOneWidget);
  });
}
