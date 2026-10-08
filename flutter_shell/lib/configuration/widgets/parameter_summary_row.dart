import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import 'parameter_value_control.dart';

/// 参数名称、元数据、数值控件与单位组成的共享列表行。
class ParameterSummaryRow extends StatelessWidget {
  const ParameterSummaryRow({
    super.key,
    required this.parameter,
    required this.controller,
  });

  final VehicleParameter parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    final description = parameter.description;
    final unit = parameter.unit;
    // 参数键和状态标记合成一行小字，避免每项单独占一行。
    final meta = [
      parameter.name,
      if (parameter.requiresReboot) '需重启',
      if (parameter.readOnly) '只读',
    ].join(' · ');
    final caption = MgTypography.sans(
      MgTypography.caption.copyWith(color: colors.textSecondary),
    );
    final note = MgTypography.sans(
      MgTypography.caption.copyWith(color: colors.textTertiary),
    );

    // 表格行：左边中英文名称与说明，右边数值与单位，行与行之间用横线分隔。
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      parameter.displayName ?? parameter.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.typography.body.copyWith(
                        color: context.mgColors.textPrimary,
                      ),
                    ),
                    Text(
                      meta,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: caption,
                    ),
                    if (description != null && description.isNotEmpty)
                      Tooltip(
                        message: description,
                        waitDuration: const Duration(milliseconds: 400),
                        child: Text(
                          description,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: note,
                        ),
                      ),
                  ],
                ),
              ),
              const SizedBox(width: 16),
              SizedBox(
                width: 200,
                child: ParameterValueControl(
                  parameter: parameter,
                  controller: controller,
                ),
              ),
              // 单位紧贴数值，空单位时留白保持一致。
              SizedBox(
                width: 52,
                child: Padding(
                  padding: const EdgeInsets.only(left: 6),
                  child: Text(
                    unit == null || unit.isEmpty ? '' : unit,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: caption,
                  ),
                ),
              ),
            ],
          ),
        ),
        Divider(height: 1, thickness: 1, color: colors.divider),
      ],
    );
  }
}
