import 'package:flutter/material.dart';
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
import 'package:mgcs_frontend/logging/file_browser_page.dart';
import 'package:mgcs_frontend/logging/flight_log_page.dart';
import 'package:mgcs_frontend/logging/onboard_log_page.dart';
import 'package:mgcs_frontend/payload/camera_gimbal_page.dart';
import 'package:mgcs_frontend/survey/survey_planning_page.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

void main() {
  testWidgets('all fourteen routes preserve disconnected page skeletons', (
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
    expect(controller.hasSession, isFalse);
    expect(find.byType(MotorOutputPage), findsOneWidget);
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
      'survey': SurveyPlanningPage,
      'payload': CameraGimbalPage,
      'logs': OnboardLogPage,
      'flightLog': FlightLogPage,
      'files': FileBrowserPage,
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
      expect(find.byType(entry.value), findsOneWidget);
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
    await tester.tap(find.text('返回前台地图'));
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
