import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

/// 系统状态区块标题与可选尾部摘要。
class SystemSectionTitle extends StatelessWidget {
  const SystemSectionTitle({super.key, required this.title, this.trailing});

  final String title;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Text(
        title,
        style: MacosTheme.of(
          context,
        ).typography.title3.copyWith(fontWeight: FontWeight.w600),
      ),
      const Spacer(),
      ?trailing,
    ],
  );
}

/// 飞控传感器健康与故障计数摘要。
class HealthSummary extends StatelessWidget {
  const HealthSummary({super.key, required this.summary});

  final Map<String, Object?>? summary;

  @override
  Widget build(BuildContext context) {
    if (summary == null) return const SizedBox.shrink();
    final colors = context.mgColors;
    final faults = summary!['fault_count'] as int? ?? 0;
    final healthy = summary!['healthy_count'] as int? ?? 0;
    final label = faults > 0 ? '异常 $faults' : '正常 $healthy';
    final color = faults > 0 ? colors.danger : colors.success;
    return Text(
      label,
      style: MacosTheme.of(context).typography.callout.copyWith(color: color),
    );
  }
}

/// 未连接或尚未收到传感器数据时的状态提示。
class SensorDataPendingCard extends StatelessWidget {
  const SensorDataPendingCard({
    super.key,
    required this.connected,
    required this.borderColor,
    required this.background,
  });

  final bool connected;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(16),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
    ),
    child: MacosListTile(
      leading: MacosIcon(
        connected ? CupertinoIcons.waveform_path : CupertinoIcons.info_circle,
        size: 18,
      ),
      title: Text(connected ? '等待传感器状态数据' : '尚未连接飞控'),
      subtitle: Text(
        connected
            ? '飞控尚未上报 SYS_STATUS，收到状态后会显示各传感器是否存在、启用及健康。'
            : '连接飞控后显示固件、机型和传感器健康状态。',
      ),
    ),
  );
}

/// 单个传感器的存在、启用与健康状态卡片。
class SensorStatusCard extends StatelessWidget {
  const SensorStatusCard({
    super.key,
    required this.sensor,
    required this.icon,
    required this.status,
    required this.borderColor,
    required this.background,
  });

  final String sensor;
  final IconData icon;
  final String status;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final (label, color) = switch (status) {
      'healthy' => ('正常', colors.success),
      'fault' => ('异常', colors.danger),
      'disabled' => ('未启用', colors.warning),
      'missing' => ('未检测到', colors.textSecondary),
      _ => ('未知', colors.textSecondary),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: background,
        border: Border.all(color: borderColor),
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
      ),
      child: Row(
        children: [
          MacosIcon(icon, size: 18),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              sensor,
              style: MacosTheme.of(context).typography.body,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          const SizedBox(width: 8),
          Text(
            label,
            style: MacosTheme.of(
              context,
            ).typography.callout.copyWith(color: color),
          ),
        ],
      ),
    );
  }
}

/// 遥测数值与数据有效性状态卡片。
class TelemetryStatusCard extends StatelessWidget {
  const TelemetryStatusCard({
    super.key,
    required this.label,
    required this.icon,
    required this.value,
    required this.detail,
    required this.fresh,
    required this.borderColor,
    required this.background,
  });

  final String label;
  final IconData icon;
  final String value;
  final String detail;
  final bool fresh;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(14),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
    ),
    child: Row(
      children: [
        MacosIcon(icon, size: 21, color: context.mgColors.accent),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(label, style: MacosTheme.of(context).typography.caption1),
              const SizedBox(height: 3),
              Text(
                value,
                style: MacosTheme.of(context).typography.body.copyWith(
                  color: fresh
                      ? context.mgColors.textPrimary
                      : context.mgColors.textSecondary,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                detail,
                style: MacosTheme.of(context).typography.caption1.copyWith(
                  color: context.mgColors.textSecondary,
                ),
              ),
            ],
          ),
        ),
      ],
    ),
  );
}
