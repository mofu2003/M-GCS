import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_tokens.dart';

final class MacGlassNavigationDock extends StatelessWidget {
  const MacGlassNavigationDock({
    required this.connectionDrawerVisible,
    required this.missionPanelVisible,
    required this.dashboardVisible,
    required this.onNavigationPressed,
    super.key,
  });

  final bool connectionDrawerVisible;
  final bool missionPanelVisible;
  final bool dashboardVisible;
  final ValueChanged<FrontstageNavigationItem> onNavigationPressed;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      key: const ValueKey('mg-navigation-dock'),
      borderRadius: BorderRadius.circular(MacGlassTokens.radiusLarge),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.58),
            border: Border.all(color: Colors.white.withValues(alpha: 0.86)),
            borderRadius: BorderRadius.circular(MacGlassTokens.radiusLarge),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Column(
            children: [
              const SizedBox(height: 16),
              Tooltip(
                message: '连接',
                child: _DockButton(
                  key: const ValueKey('mg-navigation-connection'),
                  icon: Icons.cable_rounded,
                  active: connectionDrawerVisible,
                  activeKey: const ValueKey('mg-navigation-active-connection'),
                  onPressed: () =>
                      onNavigationPressed(FrontstageNavigationItem.connection),
                ),
              ),
              Tooltip(
                message: '任务',
                child: _DockButton(
                  key: const ValueKey('mg-navigation-mission'),
                  icon: Icons.route_rounded,
                  active: missionPanelVisible,
                  activeKey: const ValueKey('mg-navigation-active-mission'),
                  onPressed: () =>
                      onNavigationPressed(FrontstageNavigationItem.mission),
                ),
              ),
              Tooltip(
                message: '仪表盘',
                child: _DockButton(
                  key: const ValueKey('mg-navigation-dashboard'),
                  icon: Icons.space_dashboard_rounded,
                  active: dashboardVisible,
                  activeKey: const ValueKey('mg-navigation-active-dashboard'),
                  onPressed: () =>
                      onNavigationPressed(FrontstageNavigationItem.dashboard),
                ),
              ),
              const Spacer(),
              Tooltip(
                message: '工程配置',
                child: _DockButton(
                  key: const ValueKey('mg-navigation-backstage'),
                  icon: Icons.tune_rounded,
                  onPressed: () =>
                      onNavigationPressed(FrontstageNavigationItem.backstage),
                ),
              ),
              const SizedBox(height: 16),
            ],
          ),
        ),
      ),
    );
  }
}

final class _DockButton extends StatelessWidget {
  const _DockButton({
    required this.icon,
    this.active = false,
    this.activeKey,
    this.onPressed,
    super.key,
  });

  final IconData icon;
  final bool active;
  final Key? activeKey;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: InkWell(
        borderRadius: BorderRadius.circular(16),
        onTap: onPressed,
        child: DecoratedBox(
          key: active ? activeKey : null,
          decoration: BoxDecoration(
            color: active
                ? MacGlassTokens.black
                : Colors.white.withValues(alpha: 0.26),
            borderRadius: BorderRadius.circular(16),
          ),
          child: SizedBox(
            width: 42,
            height: 42,
            child: Center(
              child: Icon(
                icon,
                color: active ? Colors.white : MacGlassTokens.ink,
                size: 22,
              ),
            ),
          ),
        ),
      ),
    );
  }
}
