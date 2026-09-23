import 'package:flutter/material.dart';

import '../../../application/fleet_models.dart';
import '../../controllers/fleet_controller.dart';
import '../../ui/skins/mac_glass/mac_glass_surface.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';

/// Compact functional fleet panel. It intentionally consumes only FleetController
/// snapshots so the shell can be replaced without touching multi-vehicle logic.
final class FleetVehiclePanel extends StatelessWidget {
  const FleetVehiclePanel({required this.controller, super.key});

  final FleetController controller;

  Future<void> _batch(BuildContext context, FleetCommandRequest request) async {
    final selected = controller.snapshot.selectedVehicleIds;
    if (selected.isEmpty) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('确认批量控制'),
        content: Text(
          '将对 ${selected.length} 架已选飞行器执行“${_commandLabel(request.kind)}”。',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('确认执行'),
          ),
        ],
      ),
    );
    if (confirmed != true || !context.mounted) return;
    final result = await controller.executeBatch(request);
    if (!context.mounted || result.isEmpty) return;
    final failed = result.where((item) => !item.success).length;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          failed == 0
              ? '批量操作完成：${result.length} 架'
              : '批量操作完成：${result.length - failed} 成功，$failed 失败',
        ),
      ),
    );
  }

  String _commandLabel(FleetCommandKind kind) => switch (kind) {
    FleetCommandKind.arm => '解锁',
    FleetCommandKind.disarm => '上锁',
    FleetCommandKind.setMode => '切换模式',
    FleetCommandKind.rtl => '返航',
    FleetCommandKind.land => '降落',
  };

  Future<void> _chooseMode(BuildContext context) async {
    final modes = await controller.loadFlightModes();
    if (!context.mounted || modes.isEmpty) return;
    final mode = await showDialog<String>(
      context: context,
      builder: (context) => SimpleDialog(
        title: const Text('批量切换模式'),
        children: [
          for (final option in modes)
            SimpleDialogOption(
              onPressed: () => Navigator.pop(context, option.name),
              child: Text(option.displayName),
            ),
        ],
      ),
    );
    if (!context.mounted || mode == null) return;
    await _batch(
      context,
      FleetCommandRequest(kind: FleetCommandKind.setMode, mode: mode),
    );
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: controller,
      builder: (context, _) {
        final snapshot = controller.snapshot;
        if (!controller.available || snapshot.vehicles.isEmpty) {
          return const SizedBox.shrink();
        }
        final enabled = !snapshot.busy;
        return MacGlassSurface(
          radius: MacGlassTokens.radiusLarge,
          emphasis: MacGlassSurfaceEmphasis.strong,
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    const Icon(Icons.flight_takeoff, size: 17),
                    const SizedBox(width: 7),
                    const Expanded(
                      child: Text(
                        '多机管理',
                        style: TextStyle(fontWeight: FontWeight.w800),
                      ),
                    ),
                    IconButton(
                      tooltip: '刷新车辆',
                      onPressed: enabled ? controller.refresh : null,
                      icon: const Icon(Icons.refresh, size: 18),
                    ),
                  ],
                ),
                for (final vehicle in snapshot.vehicles)
                  CheckboxListTile(
                    dense: true,
                    contentPadding: EdgeInsets.zero,
                    value: snapshot.selectedVehicleIds.contains(
                      vehicle.vehicleId,
                    ),
                    onChanged: enabled
                        ? (value) => controller.setSelected(
                            vehicle.vehicleId,
                            value ?? false,
                          )
                        : null,
                    title: Text(
                      vehicle.name.isEmpty
                          ? 'Vehicle ${vehicle.systemId}'
                          : vehicle.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    subtitle: Text(
                      'SYSID ${vehicle.systemId} · ${vehicle.firmwareType.isEmpty ? '固件未知' : vehicle.firmwareType}',
                    ),
                    secondary: snapshot.activeVehicleId == vehicle.vehicleId
                        ? const Icon(
                            Icons.radio_button_checked,
                            color: MacGlassTokens.blue,
                          )
                        : IconButton(
                            tooltip: '切换到此飞行器',
                            onPressed: enabled
                                ? () => controller.selectVehicle(
                                    vehicle.vehicleId,
                                  )
                                : null,
                            icon: const Icon(Icons.swap_horiz, size: 18),
                          ),
                  ),
                const Divider(height: 8),
                Wrap(
                  spacing: 6,
                  runSpacing: 6,
                  children: [
                    _FleetAction(
                      label: '解锁',
                      icon: Icons.lock_open,
                      onPressed: enabled
                          ? () => _batch(
                              context,
                              const FleetCommandRequest(
                                kind: FleetCommandKind.arm,
                              ),
                            )
                          : null,
                    ),
                    _FleetAction(
                      label: '返航',
                      icon: Icons.home,
                      onPressed: enabled
                          ? () => _batch(
                              context,
                              const FleetCommandRequest(
                                kind: FleetCommandKind.rtl,
                              ),
                            )
                          : null,
                    ),
                    _FleetAction(
                      label: '降落',
                      icon: Icons.flight_land,
                      onPressed: enabled
                          ? () => _batch(
                              context,
                              const FleetCommandRequest(
                                kind: FleetCommandKind.land,
                              ),
                            )
                          : null,
                    ),
                    _FleetAction(
                      label: '上锁',
                      icon: Icons.lock,
                      onPressed: enabled
                          ? () => _batch(
                              context,
                              const FleetCommandRequest(
                                kind: FleetCommandKind.disarm,
                              ),
                            )
                          : null,
                    ),
                    _FleetAction(
                      label: '模式',
                      icon: Icons.tune,
                      onPressed: enabled ? () => _chooseMode(context) : null,
                    ),
                  ],
                ),
                if (snapshot.error != null) ...[
                  const SizedBox(height: 6),
                  Text(
                    snapshot.error!,
                    style: const TextStyle(
                      color: Colors.redAccent,
                      fontSize: 11,
                    ),
                  ),
                ],
              ],
            ),
          ),
        );
      },
    );
  }
}

final class _FleetAction extends StatelessWidget {
  const _FleetAction({required this.label, required this.icon, this.onPressed});

  final String label;
  final IconData icon;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) => OutlinedButton.icon(
    onPressed: onPressed,
    icon: Icon(icon, size: 15),
    label: Text(label),
  );
}
