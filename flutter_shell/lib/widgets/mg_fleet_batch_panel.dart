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
    this.onOpenFlightMode,
    this.onClose,
  });

  final FleetController fleet;
  final Future<void> Function(int vehicleId) onSelectVehicle;
  final void Function(FleetBatchTarget target) onRequestAction;
  final ValueChanged<int>? onOpenFlightMode;
  final VoidCallback? onClose;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: fleet,
    builder: (context, _) {
      final colors = context.mgColors;
      final vehicles = fleet.snapshot.vehicles;
      final selected = fleet.selectedVehicleIds;
      final selectedVehicles = vehicles
          .where((vehicle) => selected.contains(vehicle.id))
          .toList(growable: false);
      final connectedSelected = selectedVehicles
          .where((vehicle) => vehicle.stateFresh && vehicle.identified)
          .toList(growable: false);
      final conflict = fleetSystemIdConflict(selectedVehicles);
      final progress = fleet.batchProgress;
      final canBatch =
          conflict == null &&
          connectedSelected.isNotEmpty &&
          !fleet.targetOperationActive;
      final allSelected =
          vehicles.isNotEmpty && selected.length == vehicles.length;
      final canArm = connectedSelected.any((vehicle) => !vehicle.armed);
      final canDisarm = connectedSelected.any((vehicle) => vehicle.armed);
      // 与 captureBatchTarget / 执行器保持同一判据：任务回读有效才可开始。
      // 未解锁的成员会在 start_mission 的既有流程里按 arm_if_needed 处理。
      final canStartMission = connectedSelected.any(
        (vehicle) => vehicle.missionValid && vehicle.missionTotal > 0,
      );

      return LayoutBuilder(
        builder: (context, constraints) {
          final panel = SizedBox(
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
                    _Header(onClose: onClose),
                    const SizedBox(height: 10),
                    Row(
                      children: [
                        const Spacer(),
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
                          label: '取消',
                          onPressed: selected.isEmpty
                              ? null
                              : fleet.clearVehicleSelection,
                        ),
                      ],
                    ),
                    const SizedBox(height: 10),
                    if (vehicles.isEmpty)
                      _EmptyState(colors: colors)
                    else
                      Flexible(
                        child: ConstrainedBox(
                          constraints: const BoxConstraints(maxHeight: 326),
                          child: ListView.builder(
                            shrinkWrap: true,
                            itemCount: vehicles.length,
                            itemBuilder: (context, index) {
                              final vehicle = vehicles[index];
                              return _VehicleCard(
                                key: ValueKey(
                                  'fleet-batch-vehicle-${vehicle.id}',
                                ),
                                vehicle: vehicle,
                                showEndpoint: vehicles.any(
                                  (other) =>
                                      other.id != vehicle.id &&
                                      other.systemId == vehicle.systemId,
                                ),
                                selected: selected.contains(vehicle.id),
                                active:
                                    fleet.snapshot.activeVehicleId ==
                                    vehicle.id,
                                enabled:
                                    vehicle.connected && vehicle.identified,
                                onToggle: () =>
                                    fleet.toggleVehicleSelection(vehicle.id),
                                onSelect: () => onSelectVehicle(vehicle.id),
                                onOpenFlightMode:
                                    vehicle.stateFresh &&
                                        vehicle.identified &&
                                        !fleet.targetOperationActive &&
                                        onOpenFlightMode != null
                                    ? () => onOpenFlightMode!(vehicle.id)
                                    : null,
                              );
                            },
                          ),
                        ),
                      ),
                    const SizedBox(height: 12),
                    Divider(height: 1, color: colors.hairline),
                    const SizedBox(height: 10),
                    Text(
                      '批量操作',
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                    if (progress != null) ...[
                      const SizedBox(height: 6),
                      Text(
                        '${progress.target.action.label} · 已处理 '
                        '${progress.results.length}/${progress.target.vehicleIds.length}',
                        style: MgTypography.caption,
                      ),
                      if (progress.results.isNotEmpty)
                        Tooltip(
                          message: progress.results
                              .map(
                                (result) =>
                                    '系统 ${result.systemId} · ${result.status.label} · ${result.message}',
                              )
                              .join('\n'),
                          child: Text(
                            progress.results
                                .map(
                                  (result) =>
                                      '${result.systemId} ${result.status.label}',
                                )
                                .join(' · '),
                            maxLines: 2,
                            overflow: TextOverflow.ellipsis,
                            style: MgTypography.caption,
                          ),
                        ),
                      if (progress.running) ...[
                        if (progress.stopRequested)
                          const Text('已请求停止后续派发')
                        else
                          PushButton(
                            key: const ValueKey('fleet-batch-stop'),
                            controlSize: ControlSize.small,
                            onPressed: fleet.stopBatchDispatch,
                            child: const Text('停止后续派发'),
                          ),
                      ],
                    ],
                    if (conflict != null) ...[
                      const SizedBox(height: 6),
                      Text(
                        conflict,
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(color: colors.warning),
                        ),
                      ),
                    ],
                    const SizedBox(height: 8),
                    Row(
                      children: [
                        Expanded(
                          child: _BatchActionButton(
                            key: const ValueKey('fleet-batch-arm'),
                            label: '解锁',
                            enabled: canBatch && canArm,
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
                            enabled: canBatch && canDisarm,
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
                            enabled: canBatch,
                            onPressed: () => _request(
                              context,
                              fleet,
                              onRequestAction,
                              FleetBatchAction.returnToLaunch,
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 7),
                    PushButton(
                      key: const ValueKey('fleet-batch-mission-start'),
                      controlSize: ControlSize.regular,
                      onPressed: canBatch && canStartMission
                          ? () => _request(
                              context,
                              fleet,
                              onRequestAction,
                              FleetBatchAction.startMission,
                            )
                          : null,
                      child: const Text('开始任务'),
                    ),
                  ],
                ),
              ),
            ),
          );
          if (constraints.maxHeight < (progress == null ? 320 : 480)) {
            return SingleChildScrollView(
              child: SizedBox(
                height: progress == null ? 360 : 500,
                child: panel,
              ),
            );
          }
          return panel;
        },
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
  const _Header({required this.onClose});

  final VoidCallback? onClose;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      children: [
        Container(
          width: 28,
          height: 28,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: colors.accentSurface,
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          ),
          child: Icon(
            CupertinoIcons.list_bullet,
            size: 15,
            color: colors.accent,
          ),
        ),
        const SizedBox(width: 9),
        Expanded(
          child: Text(
            '多机控制',
            style: MgTypography.sans(
              MgTypography.section.copyWith(color: colors.textPrimary),
            ),
          ),
        ),
        if (onClose != null)
          MacosIconButton(
            key: const ValueKey('fleet-batch-close'),
            icon: const MacosIcon(CupertinoIcons.xmark, size: 12),
            semanticLabel: '收起多机控制面板',
            boxConstraints: const BoxConstraints.tightFor(
              width: 26,
              height: 26,
            ),
            padding: EdgeInsets.zero,
            backgroundColor: colors.transparent,
            hoverColor: colors.mapControlHover,
            onPressed: onClose,
          ),
      ],
    );
  }
}

class _EmptyState extends StatelessWidget {
  const _EmptyState({required this.colors});

  final MgColors colors;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 20),
    child: Column(
      children: [
        Icon(
          CupertinoIcons.antenna_radiowaves_left_right,
          size: 22,
          color: colors.textTertiary,
        ),
        const SizedBox(height: 7),
        Text(
          '当前没有已发现的飞机',
          textAlign: TextAlign.center,
          style: MgTypography.sans(
            MgTypography.body.copyWith(color: colors.textSecondary),
          ),
        ),
      ],
    ),
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

/// macOS 风格的机队列表行：活动焦点使用强调色，批量选择使用复选框。
class _VehicleCard extends StatelessWidget {
  const _VehicleCard({
    super.key,
    required this.vehicle,
    required this.showEndpoint,
    required this.selected,
    required this.active,
    required this.enabled,
    required this.onToggle,
    required this.onSelect,
    required this.onOpenFlightMode,
  });

  final FleetVehicleSummary vehicle;
  final bool showEndpoint;
  final bool selected;
  final bool active;
  final bool enabled;
  final VoidCallback onToggle;
  final VoidCallback onSelect;
  final VoidCallback? onOpenFlightMode;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final mode = vehicle.flightMode.isEmpty ? '未知模式' : vehicle.flightMode;
    final rowColor = active ? colors.accentSurface : colors.surface;
    final borderColor = active ? colors.accent : colors.divider;

    return Padding(
      padding: const EdgeInsets.only(bottom: 7),
      child: Material(
        color: colors.transparent,
        child: Ink(
          decoration: BoxDecoration(
            color: rowColor,
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
            border: Border.all(color: borderColor, width: active ? 1.2 : 1),
          ),
          child: InkWell(
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
            onTap: enabled ? onSelect : null,
            child: Padding(
              padding: const EdgeInsets.fromLTRB(8, 9, 10, 9),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Semantics(
                    label:
                        '选择${vehicle.displayName}${showEndpoint ? ' · ${vehicle.connectionLabel}' : ''}',
                    checked: selected,
                    enabled: enabled,
                    child: InkWell(
                      key: ValueKey('fleet-select-${vehicle.id}'),
                      onTap: enabled ? onToggle : null,
                      borderRadius: BorderRadius.circular(6),
                      child: SizedBox(
                        width: 40,
                        height: 40,
                        child: Align(
                          alignment: Alignment.topCenter,
                          child: Container(
                            width: 22,
                            height: 22,
                            decoration: BoxDecoration(
                              color: selected ? colors.accent : colors.surface,
                              borderRadius: BorderRadius.circular(5),
                              border: Border.all(
                                color: selected
                                    ? colors.accent
                                    : colors.textSecondary,
                                width: 1.5,
                              ),
                            ),
                            child: selected
                                ? const Icon(
                                    CupertinoIcons.check_mark,
                                    size: 16,
                                    color: CupertinoColors.white,
                                  )
                                : null,
                          ),
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 7),
                  Expanded(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Text(
                              vehicle.displayName,
                              style: MgTypography.sans(
                                MgTypography.section.copyWith(
                                  fontWeight: FontWeight.w500,
                                  color: enabled
                                      ? colors.textPrimary
                                      : colors.textSecondary,
                                ),
                              ),
                            ),
                            const SizedBox(width: 7),
                            SizedBox(
                              width: 100,
                              child: PushButton(
                                key: ValueKey('fleet-mode-${vehicle.id}'),
                                controlSize: ControlSize.small,
                                secondary: true,
                                onPressed: onOpenFlightMode,
                                child: Row(
                                  children: [
                                    Expanded(
                                      child: Text(
                                        mode,
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                        style: MgTypography.sans(
                                          MgTypography.body,
                                        ),
                                      ),
                                    ),
                                    const SizedBox(width: 4),
                                    const Icon(
                                      CupertinoIcons.chevron_down,
                                      size: 10,
                                    ),
                                  ],
                                ),
                              ),
                            ),
                            const Spacer(),
                            _StatusBadge(vehicle: vehicle, enabled: enabled),
                          ],
                        ),
                        if (showEndpoint) ...[
                          const SizedBox(height: 4),
                          Tooltip(
                            message: vehicle.endpoint,
                            child: Text(
                              vehicle.connectionLabel,
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                              style: MgTypography.sans(
                                MgTypography.caption.copyWith(
                                  color: colors.textSecondary,
                                ),
                              ),
                            ),
                          ),
                        ],
                        const SizedBox(height: 8),
                        Row(
                          children: [
                            Expanded(
                              child: _Metric(
                                label: '航向',
                                value: _heading(vehicle.headingDegrees),
                              ),
                            ),
                            Expanded(
                              child: _Metric(
                                label: '相对高度',
                                value: _meters(vehicle.relativeAltitudeMeters),
                              ),
                            ),
                            Expanded(
                              child: _Metric(
                                label: '地速',
                                value: _speed(vehicle.groundSpeedMps),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
              ),
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

class _Metric extends StatelessWidget {
  const _Metric({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label,
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textTertiary),
          ),
        ),
        const SizedBox(height: 1),
        Text(
          value,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textPrimary),
          ),
        ),
      ],
    );
  }
}

class _StatusBadge extends StatelessWidget {
  const _StatusBadge({required this.vehicle, required this.enabled});

  final FleetVehicleSummary vehicle;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final (label, color) = !enabled
        ? ('离线', colors.danger)
        : vehicle.armed
        ? ('已解锁', colors.warning)
        : ('未解锁', colors.textSecondary);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(4),
      ),
      child: Text(
        label,
        style: MgTypography.sans(MgTypography.caption.copyWith(color: color)),
      ),
    );
  }
}
