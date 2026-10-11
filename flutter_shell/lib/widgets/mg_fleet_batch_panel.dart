import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../fleet/fleet_controller.dart';
import '../fleet/fleet_models.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_floating_panel.dart';

/// 多机批量控制面板。active vehicle 和批量 selection 是两个独立概念：
/// 点击飞机行只切换 active，勾选框只改变批量选择，不触发飞行命令。
Future<void> showMgFleetBatchPanel(
  BuildContext context, {
  required FleetController fleet,
  required Future<void> Function(int vehicleId) onSelectVehicle,
  required void Function(FleetBatchTarget target) onRequestAction,
  bool alignToLeft = false,
}) => showGeneralDialog<void>(
  context: context,
  barrierDismissible: true,
  barrierLabel: '关闭多机控制面板',
  barrierColor: context.mgColors.shadow.withValues(alpha: 0.12),
  transitionDuration: const Duration(milliseconds: 120),
  pageBuilder: (dialogContext, animation, secondaryAnimation) => Align(
    alignment: alignToLeft ? Alignment.topLeft : Alignment.topRight,
    child: Padding(
      padding: alignToLeft
          ? const EdgeInsets.only(
              top: MgMetrics.toolbarHeight + 8,
              left: MgMetrics.sidebarWidth - MgMetrics.fleetPanelAnchorInset,
            )
          : const EdgeInsets.only(
              top: MgMetrics.toolbarHeight + 8,
              right: MgMetrics.fleetPanelAnchorInset,
            ),
      child: ConstrainedBox(
        constraints: BoxConstraints(
          maxHeight: MediaQuery.sizeOf(dialogContext).height - 120,
        ),
        child: MgFleetBatchPanel(
          fleet: fleet,
          onSelectVehicle: onSelectVehicle,
          onRequestAction: onRequestAction,
        ),
      ),
    ),
  ),
  transitionBuilder: (context, animation, secondaryAnimation, child) =>
      FadeTransition(
        opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
        child: child,
      ),
);

class MgFleetBatchPanel extends StatelessWidget {
  const MgFleetBatchPanel({
    super.key,
    required this.fleet,
    required this.onSelectVehicle,
    required this.onRequestAction,
  });

  final FleetController fleet;
  final Future<void> Function(int vehicleId) onSelectVehicle;
  final void Function(FleetBatchTarget target) onRequestAction;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: fleet,
    builder: (context, _) {
      final vehicles = fleet.snapshot.vehicles;
      final selected = fleet.selectedVehicleIds;
      final selectedVehicles = vehicles
          .where((vehicle) => selected.contains(vehicle.id))
          .toList(growable: false);
      final connectedSelected = selectedVehicles
          .where((vehicle) => vehicle.stateFresh && vehicle.identified)
          .toList(growable: false);
      final allSelected =
          vehicles.isNotEmpty && selected.length == vehicles.length;
      final canArm = connectedSelected.any((vehicle) => !vehicle.armed);
      final canDisarm = connectedSelected.any((vehicle) => vehicle.armed);
      final canStartMission = connectedSelected.any(
        (vehicle) =>
            vehicle.armed && vehicle.missionValid && vehicle.missionTotal > 0,
      );
      return SizedBox(
        width: MgMetrics.fleetPanelWidth,
        child: MgFloatingPanel(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(
              MgMetrics.fleetPanelInset,
              MgMetrics.fleetPanelHeaderInset,
              MgMetrics.fleetPanelInset,
              MgMetrics.fleetPanelInset,
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _Header(
                  selectedCount: selected.length,
                  onClose: () => Navigator.of(context).pop(),
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        selected.isEmpty
                            ? '勾选飞机后使用批量动作'
                            : '已选 ${selected.length} 架飞机',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: context.mgColors.textSecondary,
                          ),
                        ),
                      ),
                    ),
                    _SmallActionButton(
                      key: const ValueKey('fleet-select-all'),
                      label: '全选',
                      onPressed: allSelected || vehicles.isEmpty
                          ? null
                          : fleet.selectAllVehicles,
                    ),
                    const SizedBox(width: 4),
                    _SmallActionButton(
                      key: const ValueKey('fleet-clear-selection'),
                      label: '清空',
                      onPressed: selected.isEmpty
                          ? null
                          : fleet.clearVehicleSelection,
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                if (vehicles.isEmpty)
                  Padding(
                    padding: const EdgeInsets.symmetric(vertical: 18),
                    child: Text(
                      '当前没有已发现的飞机',
                      textAlign: TextAlign.center,
                      style: MgTypography.sans(
                        MgTypography.body.copyWith(
                          color: context.mgColors.textTertiary,
                        ),
                      ),
                    ),
                  )
                else
                  Flexible(
                    child: ListView.separated(
                      shrinkWrap: true,
                      itemCount: vehicles.length,
                      separatorBuilder: (context, index) =>
                          Divider(height: 1, color: context.mgColors.hairline),
                      itemBuilder: (context, index) {
                        final vehicle = vehicles[index];
                        return _VehicleRow(
                          key: ValueKey('fleet-batch-vehicle-${vehicle.id}'),
                          vehicle: vehicle,
                          selected: selected.contains(vehicle.id),
                          active: fleet.snapshot.activeVehicleId == vehicle.id,
                          enabled: vehicle.connected && vehicle.identified,
                          onToggle: () =>
                              fleet.toggleVehicleSelection(vehicle.id),
                          onSelect: () => onSelectVehicle(vehicle.id),
                        );
                      },
                    ),
                  ),
                const SizedBox(height: 12),
                Divider(height: 1, color: context.mgColors.hairline),
                const SizedBox(height: 10),
                Text(
                  '批量动作',
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(
                      color: context.mgColors.textPrimary,
                    ),
                  ),
                ),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 6,
                  runSpacing: 6,
                  children: [
                    _BatchActionButton(
                      key: const ValueKey('fleet-batch-arm'),
                      label: '解锁',
                      enabled: connectedSelected.isNotEmpty && canArm,
                      onPressed: () => _request(
                        context,
                        fleet,
                        onRequestAction,
                        FleetBatchAction.arm,
                      ),
                    ),
                    _BatchActionButton(
                      key: const ValueKey('fleet-batch-disarm'),
                      label: '上锁',
                      enabled: connectedSelected.isNotEmpty && canDisarm,
                      onPressed: () => _request(
                        context,
                        fleet,
                        onRequestAction,
                        FleetBatchAction.disarm,
                      ),
                    ),
                    _BatchActionButton(
                      key: const ValueKey('fleet-batch-rtl'),
                      label: '返航',
                      enabled: connectedSelected.isNotEmpty,
                      onPressed: () => _request(
                        context,
                        fleet,
                        onRequestAction,
                        FleetBatchAction.returnToLaunch,
                      ),
                    ),
                    _BatchActionButton(
                      key: const ValueKey('fleet-batch-mission-start'),
                      label: '开始任务',
                      enabled: connectedSelected.isNotEmpty && canStartMission,
                      onPressed: () => _request(
                        context,
                        fleet,
                        onRequestAction,
                        FleetBatchAction.startMission,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      );
    },
  );

  void _request(
    BuildContext context,
    FleetController fleet,
    void Function(FleetBatchTarget target) onRequestAction,
    FleetBatchAction action,
  ) {
    try {
      final target = fleet.captureBatchTarget(action);
      Navigator.of(context).pop();
      onRequestAction(target);
    } on Object catch (error) {
      ScaffoldMessenger.maybeOf(context)?.showSnackBar(
        SnackBar(
          content: Text(error.toString().replaceFirst('Bad state: ', '')),
        ),
      );
    }
  }
}

class _Header extends StatelessWidget {
  const _Header({required this.selectedCount, required this.onClose});

  final int selectedCount;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Icon(
        CupertinoIcons.list_bullet,
        size: 15,
        color: context.mgColors.accent,
      ),
      const SizedBox(width: 7),
      Expanded(
        child: Text(
          '多机控制',
          style: MgTypography.sans(
            MgTypography.body.copyWith(color: context.mgColors.textPrimary),
          ),
        ),
      ),
      if (selectedCount > 0)
        Text(
          '已选 $selectedCount',
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: context.mgColors.accent),
          ),
        ),
      const SizedBox(width: 5),
      MacosIconButton(
        key: const ValueKey('fleet-batch-close'),
        icon: const MacosIcon(CupertinoIcons.xmark, size: 12),
        semanticLabel: '关闭多机控制面板',
        boxConstraints: const BoxConstraints.tightFor(width: 24, height: 24),
        padding: EdgeInsets.zero,
        backgroundColor: context.mgColors.transparent,
        hoverColor: context.mgColors.mapControlHover,
        onPressed: onClose,
      ),
    ],
  );
}

class _SmallActionButton extends StatelessWidget {
  const _SmallActionButton({
    super.key,
    required this.label,
    required this.onPressed,
  });

  final String label;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) => PushButton(
    controlSize: ControlSize.small,
    onPressed: onPressed,
    secondary: true,
    child: Text(label),
  );
}

class _BatchActionButton extends StatelessWidget {
  const _BatchActionButton({
    super.key,
    required this.label,
    required this.enabled,
    required this.onPressed,
  });

  final String label;
  final bool enabled;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => SizedBox(
    width: MgMetrics.fleetPanelActionWidth,
    child: PushButton(
      controlSize: ControlSize.regular,
      onPressed: enabled ? onPressed : null,
      secondary: true,
      child: Text(label, maxLines: 1, overflow: TextOverflow.ellipsis),
    ),
  );
}

class _VehicleRow extends StatelessWidget {
  const _VehicleRow({
    super.key,
    required this.vehicle,
    required this.selected,
    required this.active,
    required this.enabled,
    required this.onToggle,
    required this.onSelect,
  });

  final FleetVehicleSummary vehicle;
  final bool selected;
  final bool active;
  final bool enabled;
  final VoidCallback onToggle;
  final VoidCallback onSelect;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final details = <String>[];
    if (vehicle.flightMode.isNotEmpty) details.add(vehicle.flightMode);
    details.add(vehicle.armed ? '已解锁' : '未解锁');
    if (vehicle.communicationLost) {
      details.add('链路丢失');
    } else if (vehicle.linkCount > 0) {
      details.add('链路 ${vehicle.healthyLinkCount}/${vehicle.linkCount}');
    }
    return SizedBox(
      height: MgMetrics.fleetLinkRowHeight,
      child: Row(
        children: [
          MacosCheckbox(
            value: selected,
            semanticLabel: '选择飞机 ${vehicle.id}',
            onChanged: enabled ? (_) => onToggle() : null,
          ),
          const SizedBox(width: 5),
          Expanded(
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: enabled ? onSelect : null,
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Flexible(
                        child: Text(
                          '飞机 ${vehicle.id} · 系统 ${vehicle.systemId}',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: MgTypography.sans(
                            MgTypography.body.copyWith(
                              color: enabled
                                  ? colors.textPrimary
                                  : colors.textTertiary,
                            ),
                          ),
                        ),
                      ),
                      if (active) ...[
                        const SizedBox(width: 6),
                        Container(
                          width: 6,
                          height: 6,
                          decoration: BoxDecoration(
                            color: colors.accent,
                            shape: BoxShape.circle,
                          ),
                        ),
                        const SizedBox(width: 4),
                        Text(
                          '当前',
                          style: MgTypography.sans(
                            MgTypography.caption.copyWith(color: colors.accent),
                          ),
                        ),
                      ],
                    ],
                  ),
                  Text(
                    details.join(' · '),
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
          ),
        ],
      ),
    );
  }
}
