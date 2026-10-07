import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_floating_panel.dart';

enum MgSettingsDestination { connection, configuration, missionPlanning, gcs }

/// 顶部设置按钮打开的紧凑工具面板。
Future<MgSettingsDestination?> showMgSettingsMenu(
  BuildContext context, {
  required bool connected,
  required String linkLabel,
}) {
  return showGeneralDialog<MgSettingsDestination>(
    context: context,
    barrierDismissible: true,
    barrierLabel: '关闭设置',
    barrierColor: context.mgColors.shadow.withValues(alpha: 0.16),
    transitionDuration: const Duration(milliseconds: 120),
    pageBuilder: (dialogContext, animation, secondaryAnimation) => Align(
      alignment: Alignment.topRight,
      child: Padding(
        padding: const EdgeInsets.only(
          top: MgMetrics.toolbarHeight + 8,
          right: 12,
        ),
        child: _MgSettingsPanel(connected: connected, linkLabel: linkLabel),
      ),
    ),
    transitionBuilder: (context, animation, secondaryAnimation, child) =>
        FadeTransition(
          opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
          child: child,
        ),
  );
}

class _MgSettingsPanel extends StatelessWidget {
  const _MgSettingsPanel({required this.connected, required this.linkLabel});

  final bool connected;
  final String linkLabel;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return SizedBox(
      width: 286,
      child: MgFloatingPanel(
        // Material 必须在带底色的浮层之内，InkWell 的水波纹才不会被盖住。
        child: Material(
          color: Colors.transparent,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Padding(
                padding: const EdgeInsets.fromLTRB(12, 10, 12, 8),
                child: Align(
                  alignment: Alignment.centerLeft,
                  child: Text(
                    '设置',
                    style: MgTypography.sans(
                      MgTypography.bodyStrong.copyWith(
                        color: colors.textPrimary,
                      ),
                    ),
                  ),
                ),
              ),
              Divider(height: 1, color: colors.hairline),
              _SettingsRow(
                icon: Icons.link,
                title: '连接飞控',
                subtitle: connected
                    ? '已连接 · $linkLabel'
                    : '未连接 · 串口 / UDP / TCP',
                onTap: () =>
                    Navigator.of(context).pop(MgSettingsDestination.connection),
              ),
              Divider(height: 1, color: colors.hairline),
              _SettingsRow(
                icon: CupertinoIcons.slider_horizontal_3,
                title: '参数配置',
                subtitle: '飞控参数、校准、端口与传感器',
                onTap: () => Navigator.of(
                  context,
                ).pop(MgSettingsDestination.configuration),
              ),
              Divider(height: 1, color: colors.hairline),
              _SettingsRow(
                icon: CupertinoIcons.map,
                title: '航线规划',
                subtitle: '任务航点、上传下载与执行进度',
                onTap: () => Navigator.of(
                  context,
                ).pop(MgSettingsDestination.missionPlanning),
              ),
              Divider(height: 1, color: colors.hairline),
              _SettingsRow(
                icon: CupertinoIcons.gear_alt,
                title: '地面站配置',
                subtitle: '地图来源、地面站本地显示选项',
                onTap: () =>
                    Navigator.of(context).pop(MgSettingsDestination.gcs),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _SettingsRow extends StatelessWidget {
  const _SettingsRow({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Material(
      color: colors.transparent,
      child: InkWell(
        onTap: onTap,
        hoverColor: colors.mapControlHover,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(12, 10, 10, 10),
          child: Row(
            children: [
              Icon(icon, size: 18, color: colors.accent),
              const SizedBox(width: 10),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      title,
                      style: MgTypography.sans(
                        MgTypography.body.copyWith(color: colors.textPrimary),
                      ),
                    ),
                    const SizedBox(height: 1),
                    Text(
                      subtitle,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              Icon(
                CupertinoIcons.chevron_right,
                size: 13,
                color: colors.textTertiary,
              ),
            ],
          ),
        ),
      ),
    );
  }
}
