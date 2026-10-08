import 'package:flutter/material.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';

import '../widgets/mac_tab_surface.dart';
import 'pid_parameters_page.dart';

/// PID 调参页面外壳：按固件决定显示哪些调参标签页。
class PidTuningPage extends StatelessWidget {
  const PidTuningPage({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: controller.profileChanges,
    builder: (context, _) {
      final labels = switch (controller.controlProfile) {
        VehicleControlProfile.copter => const ['多旋翼 PID'],
        VehicleControlProfile.fixedWing => const ['固定翼 PID'],
        VehicleControlProfile.planeVtol => const ['固定翼 PID', 'VTOL PID'],
        VehicleControlProfile.unknown => const ['PID 调参'],
      };
      return MacTabSurface(
        key: ValueKey(labels.join('|')),
        labels: labels,
        children: [
          for (var index = 0; index < labels.length; index++)
            PidParametersPage(
              controller: controller,
              profile: controller.controlProfile,
              vtol: index == 1,
            ),
        ],
      );
    },
  );
}
