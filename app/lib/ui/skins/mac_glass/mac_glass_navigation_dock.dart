import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import 'mac_glass_tokens.dart';

enum MacGlassBackstageSection {
  parameters,
  airframe,
  servoOutputs,
  serialPorts,
  remoteControl,
  pidTuning,
  calibration,
  safety,
}

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
    return _MacGlassDockFrame(
      frameKey: const ValueKey('mg-navigation-dock'),
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
            message: '参数配置',
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
    );
  }
}

final class MacGlassBackstageNavigationDock extends StatelessWidget {
  const MacGlassBackstageNavigationDock({
    required this.selectedSection,
    required this.onSectionChanged,
    required this.onReturnToFrontstage,
    super.key,
  });

  final MacGlassBackstageSection selectedSection;
  final ValueChanged<MacGlassBackstageSection> onSectionChanged;
  final VoidCallback onReturnToFrontstage;

  @override
  Widget build(BuildContext context) => _MacGlassDockFrame(
    frameKey: const ValueKey('mg-backstage-navigation-dock'),
    child: Column(
      children: [
        const SizedBox(height: 16),
        Tooltip(
          message: '参数配置',
          child: _DockButton(
            key: const ValueKey('mg-backstage-parameters'),
            icon: Icons.tune_rounded,
            active: selectedSection == MacGlassBackstageSection.parameters,
            activeKey: const ValueKey('mg-backstage-active-parameters'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.parameters),
          ),
        ),
        Tooltip(
          message: '机架类型',
          child: _DockButton(
            key: const ValueKey('mg-backstage-airframe'),
            icon: Icons.airplanemode_active_rounded,
            active: selectedSection == MacGlassBackstageSection.airframe,
            activeKey: const ValueKey('mg-backstage-active-airframe'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.airframe),
          ),
        ),
        Tooltip(
          message: '舵机输出',
          child: _DockButton(
            key: const ValueKey('mg-backstage-servo-outputs'),
            icon: Icons.settings_input_component_rounded,
            active: selectedSection == MacGlassBackstageSection.servoOutputs,
            activeKey: const ValueKey('mg-backstage-active-servo-outputs'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.servoOutputs),
          ),
        ),
        Tooltip(
          message: '串口配置',
          child: _DockButton(
            key: const ValueKey('mg-backstage-serial-ports'),
            icon: Icons.usb_rounded,
            active: selectedSection == MacGlassBackstageSection.serialPorts,
            activeKey: const ValueKey('mg-backstage-active-serial-ports'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.serialPorts),
          ),
        ),
        Tooltip(
          message: '遥控配置',
          child: _DockButton(
            key: const ValueKey('mg-backstage-remote-control'),
            icon: Icons.settings_remote_rounded,
            active: selectedSection == MacGlassBackstageSection.remoteControl,
            activeKey: const ValueKey('mg-backstage-active-remote-control'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.remoteControl),
          ),
        ),
        Tooltip(
          message: 'PID 调参',
          child: _DockButton(
            key: const ValueKey('mg-backstage-pid-tuning'),
            icon: Icons.tune_rounded,
            active: selectedSection == MacGlassBackstageSection.pidTuning,
            activeKey: const ValueKey('mg-backstage-active-pid-tuning'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.pidTuning),
          ),
        ),
        Tooltip(
          message: '传感器/外设配置',
          child: _DockButton(
            key: const ValueKey('mg-backstage-calibration'),
            icon: Icons.explore_rounded,
            active: selectedSection == MacGlassBackstageSection.calibration,
            activeKey: const ValueKey('mg-backstage-active-calibration'),
            onPressed: () =>
                onSectionChanged(MacGlassBackstageSection.calibration),
          ),
        ),
        Tooltip(
          message: '安全设置',
          child: _DockButton(
            key: const ValueKey('mg-backstage-safety'),
            icon: Icons.shield_outlined,
            active: selectedSection == MacGlassBackstageSection.safety,
            activeKey: const ValueKey('mg-backstage-active-safety'),
            onPressed: () => onSectionChanged(MacGlassBackstageSection.safety),
          ),
        ),
        const Spacer(),
        Tooltip(
          message: '返回前台',
          child: _DockButton(
            key: const ValueKey('mg-backstage-return-frontstage'),
            icon: Icons.map_outlined,
            onPressed: onReturnToFrontstage,
          ),
        ),
        const SizedBox(height: 16),
      ],
    ),
  );
}

final class _MacGlassDockFrame extends StatelessWidget {
  const _MacGlassDockFrame({required this.frameKey, required this.child});

  final Key frameKey;
  final Widget child;

  @override
  Widget build(BuildContext context) => ClipRRect(
    key: frameKey,
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
        child: child,
      ),
    ),
  );
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
