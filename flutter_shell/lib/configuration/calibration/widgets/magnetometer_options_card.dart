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
    final orientation = controller.parameter('COMPASS_ORIENT');
    final autoRotation =
        parameterNumeric(controller, 'COMPASS_AUTO_ROT', fallback: 0) != 0;
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          Expanded(
            child: _MagnetometerToggle(
              label: '自动磁偏角',
              value:
                  parameterNumeric(
                    controller,
                    'COMPASS_AUTODEC',
                    fallback: 0,
                  ) !=
                  0,
              onChanged: controller.isConnected
                  ? (value) => controller.setParameter(
                      'COMPASS_AUTODEC',
                      value ? 1 : 0,
                    )
                  : null,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: _MagnetometerToggle(
              label: '自动检测安装方向',
              value: autoRotation,
              onChanged: controller.isConnected
                  ? (value) => controller.setParameter(
                      'COMPASS_AUTO_ROT',
                      value ? 1 : 0,
                    )
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
              // 自动检测安装方向打开时飞控会自己改写 COMPASS_ORIENT，
              // 手动选择没有意义，所以置灰。
              onChanged: autoRotation || !controller.isConnected
                  ? null
                  : (value) => controller.setParameter(
                      'COMPASS_ORIENT',
                      value.toDouble(),
                    ),
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
