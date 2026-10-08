import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../models/pid_models.dart';
import 'pid_gain_tile.dart';

/// PID 参数卡片，按可用宽度排列增益字段。
class PidCardView extends StatelessWidget {
  const PidCardView({super.key, required this.card, required this.controller});

  final PidCard card;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: context.mgColors.surface,
        border: Border.all(color: context.mgColors.divider),
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            card.title,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.typography.headline,
          ),
          const SizedBox(height: 10),
          LayoutBuilder(
            builder: (context, constraints) {
              final columns = card.fields.length <= 1 ? 1 : 2;
              final width =
                  (constraints.maxWidth - (columns - 1) * 8) / columns;
              return Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  for (final field in card.fields)
                    SizedBox(
                      width: width,
                      child: PidGainTile(field: field, controller: controller),
                    ),
                ],
              );
            },
          ),
        ],
      ),
    );
  }
}
