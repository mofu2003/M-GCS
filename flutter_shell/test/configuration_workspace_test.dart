import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';
import 'package:mgcs_frontend/theme/mg_material_theme.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/configuration/arming/arming_checks_page.dart';
import 'package:mgcs_frontend/configuration/battery/battery_configuration_page.dart';
import 'package:mgcs_frontend/configuration/calibration/calibration_page.dart';
import 'package:mgcs_frontend/configuration/flight_modes/flight_modes_page.dart';
import 'package:mgcs_frontend/configuration/motors/motor_output_page.dart';
import 'package:mgcs_frontend/configuration/parameters/all_parameters_page.dart';
import 'package:mgcs_frontend/configuration/pid/pid_tuning_page.dart';
import 'package:mgcs_frontend/configuration/ports/ports_configuration_page.dart';
import 'package:mgcs_frontend/configuration/system/system_status_page.dart';
import 'package:mgcs_frontend/configuration/widgets/mac_tab_surface.dart';
import 'package:mgcs_frontend/configuration/configuration_workspace.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

void main() {
  testWidgets(
    'Material bridge resolves system dark mode without an explicit Cupertino brightness',
    (tester) async {
      ThemeData? resolved;
      await tester.pumpWidget(
        CupertinoApp(
          home: MediaQuery(
            data: const MediaQueryData(platformBrightness: Brightness.dark),
            child: MgMaterialTheme(
              child: Builder(
                builder: (context) {
                  resolved = Theme.of(context);
                  return const SizedBox();
                },
              ),
            ),
          ),
        ),
      );
      expect(resolved!.brightness, Brightness.dark);
    },
  );

  testWidgets('Material bridge updates when Cupertino brightness changes', (
    tester,
  ) async {
    ThemeData? resolved;
    Future<void> show(Brightness brightness) async {
      await tester.pumpWidget(
        CupertinoApp(
          home: CupertinoTheme(
            data: CupertinoThemeData(brightness: brightness),
            child: MgMaterialTheme(
              child: Builder(
                builder: (context) {
                  resolved = Theme.of(context);
                  return const SizedBox();
                },
              ),
            ),
          ),
        ),
      );
      await tester.pump();
    }

    await show(Brightness.light);
    expect(resolved!.brightness, Brightness.light);
    final light = resolved;
    await show(Brightness.dark);
    expect(resolved!.brightness, Brightness.dark);
    expect(identical(resolved, light), isFalse);
  });

  testWidgets('workspace reuses a Material theme inside the macOS app', (
    tester,
  ) async {
    ThemeData? first;
    ThemeData? second;
    await tester.pumpWidget(
      MacosApp(
        home: AppWorkspaceShell(
          items: const [
            AppWorkspaceItem(id: 'test', label: '测试', icon: Icons.settings),
          ],
          selectedId: 'test',
          onSelected: (_) {},
          onReturnToMap: () {},
          contentBuilder: (context, scroll) => Builder(
            builder: (context) {
              first = Theme.of(context);
              second = Theme.of(context);
              return const SizedBox();
            },
          ),
        ),
      ),
    );
    await tester.pump();
    expect(
      identical(first, second),
      isTrue,
      reason: 'Material controls must reuse the inherited theme',
    );
  });

  testWidgets('parameter routes preserve disconnected page skeletons', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1600, 1000);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    // Uses the normal controller in the disconnected state. No session is
    // opened: these tests never touch a serial port or send flight commands.
    final controller = ConnectionController();
    var returnedToMap = false;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light().copyWith(
          typography: MgTypography.macos(Brightness.light),
        ),
        child: MaterialApp(
          home: ConfigurationWorkspace(
            connectionController: controller,
            onReturnToMap: () => returnedToMap = true,
          ),
        ),
      ),
    );
    await tester.pump();
    expect(controller.hasSession, isFalse);
    controller.preloadConfigurationResources();
    final log = controller.logController;
    final tlog = controller.tlogController;
    final files = controller.fileController;
    controller.preloadConfigurationResources();
    expect(identical(controller.logController, log), isTrue);
    expect(identical(controller.tlogController, tlog), isTrue);
    expect(identical(controller.fileController, files), isTrue);
    expect(find.byType(SystemStatusPage), findsOneWidget);
    expect(find.text('相机与云台'), findsNothing);
    final routes = <String, Type>{
      'system': SystemStatusPage,
      'calibration': CalibrationPage,
      'ports': PortsConfigurationPage,
      'motors': MotorOutputPage,
      'flightModes': FlightModesPage,
      'pid': PidTuningPage,
      'battery': BatteryConfigurationPage,
      'arming': ArmingChecksPage,
      'allParameters': AllParametersPage,
    };
    expect(
      tester
          .widget<AppWorkspaceShell>(find.byType(AppWorkspaceShell))
          .items
          .map((item) => item.id),
      routes.keys,
    );
    for (final entry in routes.entries) {
      tester
          .widget<AppWorkspaceShell>(find.byType(AppWorkspaceShell))
          .onSelected(entry.key);
      await tester.pump();
      expect(
        find.byType(entry.value),
        findsOneWidget,
        reason: "page must appear in the navigation frame",
      );
      await tester.pump();
      expect(tester.takeException(), isNull, reason: entry.key);
      if (entry.key == 'calibration') {
        final tabs = tester.widget<MacTabSurface>(find.byType(MacTabSurface));
        expect(tabs.labels, ['加速度校准', '磁力计校准', '空速计校准', '遥控器校准']);
        for (var index = 0; index < 4; index++) {
          tabs.controller!.index = index;
          await tester.pump();
          expect(tester.takeException(), isNull);
        }
      }
      if (entry.key == 'pid') {
        await tester.tap(find.text('扩展'));
        await tester.pump();
        expect(find.text('响应限制'), findsOneWidget);
        expect(tester.takeException(), isNull);
      }
    }
    final shell = tester.widget<AppWorkspaceShell>(
      find.byType(AppWorkspaceShell),
    );
    shell.onSelected('allParameters');
    await tester.pumpAndSettle();
    await tester.enterText(find.byType(EditableText), 'COMPASS');
    shell.onSelected('system');
    await tester.pumpAndSettle();
    shell.onSelected('allParameters');
    await tester.pumpAndSettle();
    expect(
      tester.widget<EditableText>(find.byType(EditableText)).controller.text,
      'COMPASS',
      reason: 'switching pages preserves the search',
    );
    controller.parameters['COMPASS_ENABLE'] = const VehicleParameter(
      name: 'COMPASS_ENABLE',
      value: 1,
      mavlinkType: 9,
      displayName: '启用罗盘',
      group: '传感器',
    );
    controller.parameterChanges.value++;
    await tester.pumpAndSettle();
    expect(find.text('COMPASS_ENABLE'), findsOneWidget);
    controller.parameters['COMPASS_ENABLE'] = controller
        .parameters['COMPASS_ENABLE']!
        .copyWith(displayName: '罗盘开关');
    controller.parameterChanges.value++;
    await tester.pumpAndSettle();
    await tester.enterText(find.byType(EditableText), '罗盘开关');
    await tester.pumpAndSettle();
    expect(
      find.text('COMPASS_ENABLE'),
      findsOneWidget,
      reason: 'metadata changes invalidate the search index',
    );
    tester.view.physicalSize = const Size(1000, 600);
    shell.onSelected('ports');
    await tester.pumpAndSettle();
    Finder portScroll() => find
        .descendant(
          of: find.byType(PortsConfigurationPage),
          matching: find.byType(Scrollable),
        )
        .first;
    expect(
      tester.state<ScrollableState>(portScroll()).position.maxScrollExtent,
      greaterThan(100),
    );
    tester.state<ScrollableState>(portScroll()).position.jumpTo(100);
    await tester.pump();
    shell.onSelected('system');
    await tester.pumpAndSettle();
    shell.onSelected('ports');
    await tester.pumpAndSettle();
    expect(
      tester.state<ScrollableState>(portScroll()).position.pixels,
      100,
      reason: 'each page restores its own scroll offset',
    );
    for (final size in [const Size(640, 700), const Size(1000, 800)]) {
      tester.view.physicalSize = size;
      for (final page in ['system', 'ports', 'allParameters']) {
        tester
            .widget<AppWorkspaceShell>(find.byType(AppWorkspaceShell))
            .onSelected(page);
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull, reason: '$page at $size');
      }
    }
    await tester.tap(find.text('返回飞行控制页面'));
    expect(returnedToMap, isTrue);
    await tester.pumpWidget(const SizedBox.shrink());
    // Updating after removal must not notify any disposed page state.
    controller.profileChanges.value++;
    controller.calibrationChanges.value++;
    controller.waveform.changes.value++;
    await tester.pump();
    controller.dispose();
    expect(tester.takeException(), isNull);
  });
}
