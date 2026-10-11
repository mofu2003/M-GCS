import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/widgets/adaptive_popup_button.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';

import '../../widgets/parameter_helpers.dart';

/// 罗盘安装方向与自动检测配置，选项来自飞控参数描述。
class MagnetometerOptionsCard extends StatelessWidget {
  const MagnetometerOptionsCard({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    // 参数名只由后端角色表提供（safety.md S6）：角色标异步到位，缺失时控件先置灰。
    final orientName = controller.roleParam('compass.orient');
    final autoRotName = controller.roleParam('compass.auto_rot');
    final autoDecName = controller.roleParam('compass.auto_dec');
    final orientation = orientName == null
        ? null
        : controller.parameter(orientName);
    final autoRotation =
        autoRotName != null &&
        (controller.parameter(autoRotName)?.value ?? 0) != 0;
    final autoDec =
        autoDecName != null &&
        (controller.parameter(autoDecName)?.value ?? 0) != 0;
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          Expanded(
            child: _MagnetometerToggle(
              label: '自动磁偏角',
              value: autoDec,
              onChanged: controller.isConnected && autoDecName != null
                  ? (value) =>
                        controller.setParameter(autoDecName, value ? 1 : 0)
                  : null,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: _MagnetometerToggle(
              label: '自动检测安装方向',
              value: autoRotation,
              onChanged: controller.isConnected && autoRotName != null
                  ? (value) =>
                        controller.setParameter(autoRotName, value ? 1 : 0)
                  : null,
            ),
          ),
          const SizedBox(width: 12),
          SizedBox(
            width: 260,
            child: AdaptivePopupButton<int>(
              value: orientation?.value.round() ?? 0,
              options: enumOptionsFor(orientation, const [0]),
              labelOf: (value) => enumLabelFor(orientation, value),
              // 自动检测安装方向打开时飞控会自己改写安装方向参数，
              // 手动选择没有意义，所以置灰。
              onChanged:
                  autoRotation || !controller.isConnected || orientName == null
                  ? null
                  : (value) =>
                        controller.setParameter(orientName, value.toDouble()),
            ),
          ),
        ],
      ),
    );
  }
}

class _MagnetometerToggle extends StatelessWidget {
  const _MagnetometerToggle({
    required this.label,
    required this.value,
    required this.onChanged,
  });

  final String label;
  final bool value;
  final ValueChanged<bool>? onChanged;

  @override
  Widget build(BuildContext context) => Container(
    height: 42,
    padding: const EdgeInsets.symmetric(horizontal: 12),
    decoration: BoxDecoration(
      color: context.mgColors.valueTile,
      borderRadius: BorderRadius.circular(9),
    ),
    child: Row(
      children: [
        Expanded(child: Text(label)),
        MacosSwitch(
          value: value,
          size: ControlSize.small,
          onChanged: onChanged,
        ),
      ],
    ),
  );
}
