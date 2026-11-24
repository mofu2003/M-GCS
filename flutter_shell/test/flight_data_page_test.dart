import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/logging/flight_data_page.dart';
import 'package:mgcs_frontend/logging/onboard_log_page.dart';
import 'package:mgcs_frontend/logging/file_browser_page.dart';
import 'package:mgcs_frontend/logging/flight_log_page.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

void main() {
  final pages = <FlightDataView, Type>{
    FlightDataView.onboardLogs: OnboardLogPage,
    FlightDataView.onboardFiles: FileBrowserPage,
    FlightDataView.flightRecords: FlightLogPage,
  };
  for (final entry in pages.entries) {
    testWidgets(
      '${entry.key} opens independently and returns to flight control',
      (tester) async {
        await tester.binding.setSurfaceSize(const Size(1000, 700));
        addTearDown(() => tester.binding.setSurfaceSize(null));
        final controller = ConnectionController();
        addTearDown(controller.dispose);
        var returned = 0;
        await tester.pumpWidget(
          MacosApp(
            home: Scaffold(
              body: FlightDataPage(
                view: entry.key,
                controller: controller,
                onReturnToFlight: () => returned++,
              ),
            ),
          ),
        );
        await tester.pump();
        expect(find.byType(entry.value), findsOneWidget);
        expect(find.text('日志与文件'), findsOneWidget);
        expect(find.byType(AppWorkspaceShell), findsNothing);
        expect(find.text('参数配置'), findsNothing);
        for (final target in pages.entries) {
          final tab = find.descendant(
            of: find.byType(MacosSegmentedControl),
            matching: find.text(target.key.title),
          );
          await tester.tap(tab);
          await tester.pump();
          await tester.pump(const Duration(milliseconds: 200));
          expect(find.byType(target.value), findsOneWidget);
        }
        await tester.tap(find.text('返回飞行控制页面'));
        expect(returned, 1);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      },
    );
  }
}
