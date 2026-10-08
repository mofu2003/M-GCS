import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/theme/mg_colors.dart';

/// 连接、固件、机型与飞控版本概览卡片。
class SystemOverviewCard extends StatelessWidget {
  const SystemOverviewCard({
    super.key,
    required this.connected,
    required this.connectionState,
    required this.firmware,
    required this.vehicle,
    required this.systemId,
    required this.firmwareVersion,
    required this.background,
    required this.borderColor,
  });

  final bool connected;
  final String connectionState;
  final String firmware;
  final String vehicle;
  final int? systemId;
  final String firmwareVersion;
  final Color background;
  final Color borderColor;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(18),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(12),
    ),
    child: LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth >= 850 ? 4 : 2;
        final itemWidth = (constraints.maxWidth - (columns - 1) * 20) / columns;
        return Wrap(
          spacing: 20,
          runSpacing: 16,
          children: [
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '连接状态',
                value: connectionState,
                icon: connected ? CupertinoIcons.link : CupertinoIcons.link,
                valueColor: connected
                    ? context.mgColors.success
                    : context.mgColors.textSecondary,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '固件类型',
                value: firmware,
                icon: CupertinoIcons.antenna_radiowaves_left_right,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '机型',
                value: vehicle,
                icon: CupertinoIcons.airplane,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '飞控信息',
                value: [
                  if (systemId != null) '系统 ID $systemId',
                  if (firmwareVersion != '尚未上报') firmwareVersion,
                  if (systemId == null && firmwareVersion == '尚未上报') '等待数据',
                ].join(' · '),
                icon: CupertinoIcons.info_circle,
              ),
            ),
          ],
        );
      },
    ),
  );
}

class _OverviewValue extends StatelessWidget {
  const _OverviewValue({
    required this.label,
    required this.value,
    required this.icon,
    this.valueColor,
  });

  final String label;
  final String value;
  final IconData icon;
  final Color? valueColor;

  @override
  Widget build(BuildContext context) {
    final typography = MacosTheme.of(context).typography;
    final colors = context.mgColors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(top: 2),
          child: MacosIcon(icon, size: 17, color: colors.accent),
        ),
        const SizedBox(width: 9),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                label,
                style: typography.caption1.copyWith(
                  color: colors.textSecondary,
                ),
              ),
              const SizedBox(height: 3),
              Text(
                value,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                // 值（如 ArduPilot、FixedWing）不是标题，不加粗。
                style: typography.body.copyWith(
                  color: valueColor ?? context.mgColors.textPrimary,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
