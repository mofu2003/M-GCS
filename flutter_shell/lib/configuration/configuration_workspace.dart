import 'package:flutter/cupertino.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/widgets/app_workspace_shell.dart';

import 'arming/arming_checks_page.dart';
import 'battery/battery_configuration_page.dart';
import 'calibration/calibration_page.dart';
import 'flight_modes/flight_modes_page.dart';
import 'motors/motor_output_page.dart';
import 'parameters/all_parameters_page.dart';
import 'pid/pid_tuning_page.dart';
import 'ports/ports_configuration_page.dart';
import 'system/system_status_page.dart';

/// 参数配置模式：左侧固定导航，右侧切换独立配置页面。
class ConfigurationWorkspace extends StatefulWidget {
  const ConfigurationWorkspace({
    super.key,
    required this.onReturnToMap,
    required this.connectionController,
  });

  final VoidCallback onReturnToMap;
  final ConnectionController connectionController;

  @override
  State<ConfigurationWorkspace> createState() => _ConfigurationWorkspaceState();
}

class _ConfigurationWorkspaceState extends State<ConfigurationWorkspace> {
  static const _basePages = <_ConfigPageInfo>[
    _ConfigPageInfo('system', '系统状态', CupertinoIcons.heart),
    _ConfigPageInfo('calibration', '飞控校准', CupertinoIcons.scope),
    _ConfigPageInfo(
      'ports',
      '端口配置',
      CupertinoIcons.antenna_radiowaves_left_right,
    ),
    _ConfigPageInfo('motors', '电机输出', CupertinoIcons.slider_horizontal_3),
    _ConfigPageInfo('flightModes', '飞行模式', CupertinoIcons.airplane),
    _ConfigPageInfo('pid', 'PID 调参', CupertinoIcons.waveform_path_ecg),
    _ConfigPageInfo('battery', '电池配置', CupertinoIcons.battery_100),
    _ConfigPageInfo('arming', '解锁检查', CupertinoIcons.checkmark_shield),
    _ConfigPageInfo('allParameters', '全部参数', CupertinoIcons.list_number),
  ];

  String _selectedPageId = 'motors';

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.connectionController.profileChanges,
    builder: (context, _) {
      final pages = _pagesForProfile(
        widget.connectionController.controlProfile,
      );
      final selectedPageId = pages.any((page) => page.id == _selectedPageId)
          ? _selectedPageId
          : 'motors';
      final selectedIndex = pages.indexWhere(
        (page) => page.id == selectedPageId,
      );
      return AppWorkspaceShell(
        items: [
          for (final page in pages)
            AppWorkspaceItem(id: page.id, label: page.label, icon: page.icon),
        ],
        selectedId: pages[selectedIndex].id,
        onSelected: (id) => setState(() => _selectedPageId = id),
        onReturnToMap: widget.onReturnToMap,
        contentBuilder: (context, scrollController) => _buildSelectedPage(
          context,
          scrollController,
          pageId: selectedPageId,
        ),
      );
    },
  );

  List<_ConfigPageInfo> _pagesForProfile(VehicleControlProfile profile) => [
    for (final page in _basePages) page,
  ];

  Widget _buildSelectedPage(
    BuildContext context,
    ScrollController scrollController, {
    required String pageId,
  }) => switch (pageId) {
    'ports' => PortsConfigurationPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'motors' => MotorOutputPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'flightModes' => FlightModesPage(controller: widget.connectionController),
    'pid' => PidTuningPage(controller: widget.connectionController),
    'battery' => const BatteryConfigurationPage(),
    'arming' => ArmingChecksPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'allParameters' => AllParametersPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'system' => SystemStatusPage(controller: widget.connectionController),
    'calibration' => CalibrationPage(controller: widget.connectionController),
    _ => AllParametersPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
  };
}

class _ConfigPageInfo {
  const _ConfigPageInfo(this.id, this.label, this.icon);

  final String id;
  final String label;
  final IconData icon;
}
