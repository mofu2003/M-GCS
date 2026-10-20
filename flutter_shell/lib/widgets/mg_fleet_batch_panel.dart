import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../fleet/fleet_controller.dart';
import '../fleet/fleet_models.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_floating_panel.dart';

/// 多机批量控制面板。
///
/// 固定在地图右上角悬浮显示，不使用模态遮罩；active vehicle 和批量 selection
/// 仍然是两个独立概念：点卡片只切换 active，勾选框只改变批量选择。
class MgFleetBatchPanel extends StatelessWidget {
  const MgFleetBatchPanel({
    super.key,
    required this.fleet,
    required this.onSelectVehicle,
    required this.onRequestAction,
    this.onClose,
  });

  final FleetController fleet;
  final Future<void> Function(int vehicleId) onSelectVehicle;
  final void Function(FleetBatchTarget target) onRequestAction;
  final VoidCallback? onClose;

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
                _Header(selectedCount: selected.length, onClose: onClose),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        '已选飞机：${selected.isEmpty ? '-' : selected.length}',
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
                    const SizedBox(width: 6),
                    _SmallActionButton(
                      key: const ValueKey('fleet-clear-selection'),
                      label: '取消全选',
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
                  ConstrainedBox(
                    constraints: const BoxConstraints(maxHeight: 320),
                    child: ListView.builder(
                      shrinkWrap: true,
                      itemCount: vehicles.length,
                      itemBuilder: (context, index) {
                        final vehicle = vehicles[index];
                        return _VehicleCard(
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
                  '多机动作',
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(
                      color: context.mgColors.textPrimary,
                    ),
                  ),
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: _BatchActionButton(
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
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: _BatchActionButton(
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
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: _BatchActionButton(
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
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: _BatchActionButton(
                        key: const ValueKey('fleet-batch-mission-start'),
                        label: '开始任务',
                        enabled:
                            connectedSelected.isNotEmpty && canStartMission,
                        onPressed: () => _request(
                          context,
                          fleet,
                          onRequestAction,
                          FleetBatchAction.startMission,
                        ),
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
      onRequestAction(fleet.captureBatchTarget(action));
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
  final VoidCallback? onClose;

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
      if (onClose != null) ...[
        const SizedBox(width: 5),
        MacosIconButton(
          key: const ValueKey('fleet-batch-close'),
          icon: const MacosIcon(CupertinoIcons.xmark, size: 12),
          semanticLabel: '收起多机控制面板',
          boxConstraints: const BoxConstraints.tightFor(width: 24, height: 24),
          padding: EdgeInsets.zero,
          backgroundColor: context.mgColors.transparent,
          hoverColor: context.mgColors.mapControlHover,
          onPressed: onClose,
        ),
      ],
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
  Widget build(BuildContext context) => PushButton(
    controlSize: ControlSize.regular,
    onPressed: enabled ? onPressed : null,
    secondary: true,
    child: Text(label, maxLines: 1, overflow: TextOverflow.ellipsis),
  );
}

/// QGC 风格的多机卡片：ID、飞行模式、锁状态、航向、高度、速度。
class _VehicleCard extends StatelessWidget {
  const _VehicleCard({
    super.key,
    required this.vehicle,
    required this.selected,
    required this.active,
    required this.enabled,
    required this.onToggle,
    required this.onSelect,
  });

  /// QGC 当前飞机卡片用绿色，其余飞机用灰色。
  static const _activeColor = Color(0xFF1F7A1F);
  static const _idleColor = Color(0xFF6E6E73);

  final FleetVehicleSummary vehicle;
  final bool selected;
  final bool active;
  final bool enabled;
  final VoidCallback onToggle;
  final VoidCallback onSelect;

  @override
  Widget build(BuildContext context) {
    final background = active ? _activeColor : _idleColor;
    final id = vehicle.systemId > 0 ? vehicle.systemId : vehicle.id;
    final mode = vehicle.flightMode.isEmpty ? '未知模式' : vehicle.flightMode;
    final heading = vehicle.headingDegrees;
    final altitude = vehicle.altitudeMeters;
    final speed = vehicle.groundSpeedMps;
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Material(
        color: enabled ? background : background.withValues(alpha: 0.55),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        child: InkWell(
          borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          onTap: enabled ? onSelect : null,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(4, 8, 10, 8),
            child: Row(
              children: [
                MacosCheckbox(
                  value: selected,
                  semanticLabel: '选择飞机 $id',
                  onChanged: enabled ? (_) => onToggle() : null,
                ),
                const SizedBox(width: 2),
                SizedBox(
                  width: 30,
                  child: Text(
                    '$id',
                    textAlign: TextAlign.center,
                    style: MgTypography.sans(
                      MgTypography.title.copyWith(
                        color: Colors.white,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                ),
                Container(
                  width: 1,
                  height: 44,
                  margin: const EdgeInsets.symmetric(horizontal: 8),
                  color: Colors.white24,
                ),
                Expanded(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          Expanded(
                            child: Text(
                              mode,
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                              style: MgTypography.sans(
                                MgTypography.section.copyWith(
                                  color: Colors.white,
                                ),
                              ),
                            ),
                          ),
                          Text(
                            vehicle.armed ? '已解锁' : '未解锁',
                            style: MgTypography.sans(
                              MgTypography.caption.copyWith(
                                color: vehicle.armed
                                    ? Colors.white
                                    : const Color(0xFFFF8A80),
                              ),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 4),
                      Text(
                        '航向 ${_heading(heading)}   高度 ${_meters(altitude)}   速度 ${_speed(speed)}',
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(color: Colors.white70),
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  static String _heading(double? value) =>
      value == null ? '—' : '${value.round()}°';

  static String _meters(double? value) =>
      value == null ? '—' : '${value.toStringAsFixed(1)} m';

  static String _speed(double? value) =>
      value == null ? '—' : '${value.toStringAsFixed(1)} m/s';
}
