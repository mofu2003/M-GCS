import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/mg_confirm.dart';

void main() {
  testWidgets('showMgConfirmDialog reports confirm vs cancel decisions', (
    tester,
  ) async {
    Future<bool>? pending;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light().copyWith(
          typography: MgTypography.macos(Brightness.light),
        ),
        child: MaterialApp(
          home: Builder(
            builder: (context) => Center(
              child: MaterialButton(
                onPressed: () {
                  pending = showMgConfirmDialog(
                    context,
                    title: '下发密钥到飞控？',
                    message: '密钥不一致会让 GCS 与飞控失联。',
                    confirmLabel: '下发密钥',
                  );
                },
                child: const Text('open'),
              ),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    expect(find.text('下发密钥到飞控？'), findsOneWidget);

    await tester.tap(find.text('取消'));
    await tester.pumpAndSettle();
    expect(await pending, isFalse);

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('下发密钥'));
    await tester.pumpAndSettle();
    expect(await pending, isTrue);
  });
}
