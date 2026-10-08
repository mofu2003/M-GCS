import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';

import '../../widgets/parameter_value_control.dart';
import '../models/pid_models.dart';

/// 单个 PID 增益标签与参数编辑控件。
class PidGainTile extends StatelessWidget {
  const PidGainTile({super.key, required this.field, required this.controller});

  final PidGainField field;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final parameter = controller.parameters[field.parameterName];
    // 外层已经是卡片，这里只排标签和输入框，避免卡片套卡片。
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          field.label,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: theme.typography.caption1.copyWith(
            color: context.mgColors.textSecondary,
          ),
        ),
        const SizedBox(height: 5),
        ParameterValueControl(
          parameter: parameter,
          controller: controller,
          center: true,
        ),
      ],
    );
  }
}
