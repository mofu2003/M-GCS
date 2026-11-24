import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../fleet/fleet_models.dart';
import '../flight/flight_mode_labels.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_animated_number.dart';
import 'mg_status_telemetry_icons.dart';
import 'mg_command_confirmation.dart';
import 'mg_floating_panel.dart';

double mgTopStatusBarHeight(double width, {required bool multipleVehicles}) =>
    width < (multipleVehicles ? 980 : 760) ? 88 : MgMetrics.toolbarHeight;

Future<void> showMgFlightModeMenu(
  BuildContext context, {
  required ConnectionController controller,
  required List<FlightModeOption> options,
  String? vehicleLabel,
  Alignment alignment = Alignment.topLeft,
}) async {
  final focusRevision = controller.fleet.focusRevision;
  await showGeneralDialog<void>(
    context: context,
    barrierDismissible: true,
    barrierLabel: '关闭飞行模式列表',
    barrierColor: context.mgColors.shadow.withValues(alpha: 0.12),
    transitionDuration: const Duration(milliseconds: 120),
    pageBuilder: (dialogContext, animation, secondaryAnimation) => Align(
      alignment: alignment,
      child: Padding(
        padding: EdgeInsets.only(
          top:
              mgTopStatusBarHeight(
                MediaQuery.sizeOf(context).width,
                multipleVehicles: controller.fleet.snapshot.vehicles.length > 1,
              ) +
              8,
          left: alignment == Alignment.topLeft ? 88 : 18,
          right: 18,
        ),
        child: _FlightModeMenu(
          controller: controller,
          options: options,
          focusRevision: focusRevision,
          vehicleLabel: vehicleLabel,
        ),
      ),
    ),
    transitionBuilder: (context, animation, secondaryAnimation, child) =>
        FadeTransition(
          opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
          child: child,
        ),
  );
}

class _FlightModeMenu extends StatelessWidget {
  const _FlightModeMenu({
    required this.controller,
    required this.options,
    required this.focusRevision,
    this.vehicleLabel,
  });

  final ConnectionController controller;
  final List<FlightModeOption> options;
  final int focusRevision;
  final String? vehicleLabel;

  Future<void> _setMode(BuildContext context, FlightModeOption option) async {
    if (!option.settable) return;
    controller.fleet.refresh();
    if (controller.fleet.focusRevision != focusRevision) {
      if (context.mounted) Navigator.of(context).pop();
      return;
    }
    try {
      await controller.setFlightMode(option.name);
      if (context.mounted) Navigator.of(context).pop();
    } catch (_) {
      // 后端状态消息会在顶部状态栏和飞控消息面板中呈现。
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return SizedBox(
      width: 250,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: colors.surface,
          borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
          border: Border.all(color: colors.divider),
          boxShadow: [
            BoxShadow(
              color: colors.shadow,
              blurRadius: 24,
              offset: const Offset(0, 8),
            ),
          ],
        ),
        // Material 必须在带背景色的 DecoratedBox 之内，否则水波纹和高亮会被
        // DecoratedBox 盖住（debug 下会直接抛断言）。
        child: ClipRRect(
          borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
          child: Material(
            color: colors.transparent,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 8, 12, 6),
                  child: Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      vehicleLabel == null ? '按住切换' : '$vehicleLabel · 按住切换模式',
                      style: MgTypography.sans(
                        MgTypography.toolbar.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                  ),
                ),
                Divider(height: 1, color: colors.hairline),
                Flexible(
                  child: ListView(
                    shrinkWrap: true,
                    padding: const EdgeInsets.symmetric(vertical: 6),
                    children: [
                      for (final option in options)
                        MgFlightModeRow(
                          option: option,
                          selected: option.name == controller.vehicleFlightMode,
                          onTriggered: () => _setMode(context, option),
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
}

/// 单个飞行模式条目：**只有按住才切换**。
///
/// 交互照 QGC 的 hold-to-confirm（`TimedProgressTracker` + GCS 控制指示器那套）：
/// 按住条目时一条进度从左往右填满；中途松手或移出即取消；
/// 进度拉满才真正下发模式切换。不弹确认框，也不做"短按直接切换"。
class MgFlightModeRow extends StatefulWidget {
  const MgFlightModeRow({
    super.key,
    required this.option,
    required this.selected,
    required this.onTriggered,
  });

  final FlightModeOption option;
  final bool selected;
  final VoidCallback onTriggered;

  @override
  State<MgFlightModeRow> createState() => MgFlightModeRowState();
}

class MgFlightModeRowState extends State<MgFlightModeRow>
    with SingleTickerProviderStateMixin {
  /// 按住多久算确认。统一使用 M-GCS 的 0.5 s 标准。
  static const Duration holdDuration = kMgHoldToConfirmDuration;

  late final AnimationController _hold =
      AnimationController(vsync: this, duration: holdDuration)
        ..addStatusListener((status) {
          if (status != AnimationStatus.completed) return;
          _hold.reset();
          widget.onTriggered();
        });

  bool get _enabled => widget.option.settable;

  @override
  void dispose() {
    _hold.dispose();
    super.dispose();
  }

  void _start() => _hold.forward(from: 0);

  void _cancel() {
    if (_hold.status == AnimationStatus.dismissed) return;
    _hold.reverse();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final enabled = _enabled;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      // 不注册 onTap / onLongPress：只认"按住到满"。
      onTapDown: enabled ? (_) => _start() : null,
      onTapUp: enabled ? (_) => _cancel() : null,
      onTapCancel: enabled ? _cancel : null,
      child: AnimatedBuilder(
        animation: _hold,
        builder: (context, _) {
          final progress = _hold.value;
          // 当前模式用底色高亮表示，不用对勾。
          final textStyle = MgTypography.sans(
            MgTypography.toolbar.copyWith(
              color: !enabled
                  ? colors.textTertiary
                  : widget.selected
                  ? colors.accent
                  : colors.textPrimary,
            ),
          );
          return Stack(
            children: [
              if (widget.selected)
                Positioned.fill(child: ColoredBox(color: colors.accentSurface)),
              if (progress > 0)
                Positioned.fill(
                  child: Align(
                    alignment: Alignment.centerLeft,
                    child: FractionallySizedBox(
                      widthFactor: progress,
                      heightFactor: 1,
                      child: ColoredBox(
                        // 按住进度比"选中高亮"更实，两者不会看混。
                        color: colors.accent.withValues(alpha: 0.45),
                      ),
                    ),
                  ),
                ),
              SizedBox(
                height: 38,
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                  child: Row(
                    children: [
                      Expanded(
                        child: Text(
                          widget.option.name,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: textStyle,
                        ),
                      ),
                      // 中文显示名只出现在列表里；同名（未收录）就不重复显示。
                      if (flightModeLabel(widget.option.name) !=
                          widget.option.name) ...[
                        const SizedBox(width: 8),
                        Flexible(
                          child: Text(
                            flightModeLabel(widget.option.name),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            textAlign: TextAlign.right,
                            style: MgTypography.sans(
                              MgTypography.toolbar.copyWith(
                                color: enabled
                                    ? colors.textSecondary
                                    : colors.textTertiary,
                              ),
                            ),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

/// 顶部状态栏的数据快照。
///
/// 控件本身只负责布局和显示，字段来自现有 [ConnectionController]，不新增后端契约。
/// 这一层分开后，测试不需要启动 FFI 或真实飞控链路。
@immutable
class MgTopStatusData {
  const MgTopStatusData({
    required this.connected,
    required this.armed,
    required this.flying,
    required this.flightMode,
    required this.vtolState,
    required this.gpsFixType,
    required this.satellitesVisible,
    required this.batteryRemainingPercent,
    required this.batteryVoltage,
    required this.transportLabel,
    required this.messageCount,
    this.stale = false,
    this.isVtolVehicle = false,
  });

  factory MgTopStatusData.fromController(ConnectionController controller) {
    return MgTopStatusData(
      connected: controller.isConnected,
      armed: controller.vehicleStateFresh && controller.vehicleArmed,
      flying: controller.vehicleStateFresh && controller.vehicleFlying,
      flightMode: controller.vehicleFlightMode,
      vtolState: controller.vehicleVtolState,
      gpsFixType: controller.gpsFixType,
      satellitesVisible: controller.satellitesVisible,
      batteryRemainingPercent: controller.batteryRemainingPercent,
      batteryVoltage: controller.batteryVoltage,
      transportLabel: controller.selectedTransportLabel,
      messageCount: controller.flightMessages.length,
      stale:
          controller.telemetryStale ||
          (controller.isConnected && !controller.vehicleStateFresh),
      isVtolVehicle: controller.firmwareInfo?['vehicle_class'] == 'vtol',
    );
  }

  final bool connected;
  final bool armed;
  final bool flying;
  final String flightMode;
  final int vtolState;
  final int? gpsFixType;
  final int? satellitesVisible;
  final int? batteryRemainingPercent;
  final double? batteryVoltage;
  final String transportLabel;
  final int messageCount;

  /// 遥测读数已过期（链路断开或心跳超时后仍保留最后一帧）。
  /// 为真时读数必须显示为占位符，不能当成当前值。
  final bool stale;
  final bool isVtolVehicle;

  String get statusLabel {
    if (!connected) return '未连接';
    if (stale) return '数据过期';
    if (flying) return '飞行中';
    if (armed) return '已解锁';
    return '待命';
  }

  /// VTOL 状态后缀（对应 MAV_VTOL_STATE）。
  ///
  /// 界面以中文为主，所以这里用中文描述状态，不写 MC/FW：
  /// 1=TRANSITION_TO_FW、2=TRANSITION_TO_MC、3=MC、4=FW；
  /// 0=UNDEFINED（非 VTOL 或还没上报）时不加后缀。
  ///
  /// 兜底分支（vtolState==0 但已知是 VTOL 机型）只在 EXTENDED_SYS_STATE
  /// 到达之前短暂出现——后端已按 QGC 的做法以 1Hz 请求该消息。
  /// 文案同样用中文，避免同一行里中英混排。
  String get modeLabel {
    if (flightMode.isEmpty) return '—';
    final vtolSuffix = switch (vtolState) {
      1 => '转固定翼',
      2 => '转多旋翼',
      3 => '多旋翼',
      4 => '固定翼',
      _ => null,
    };
    if (vtolSuffix != null) return '$flightMode · $vtolSuffix';
    if (flightMode.toLowerCase() == 'guided' && isVtolVehicle) {
      return '$flightMode · 垂直起降';
    }
    return flightMode;
  }

  String get gpsFixLabel => switch (gpsFixType) {
    2 => '2D',
    3 => '3D',
    4 => 'DGPS',
    5 => 'RTK Float',
    6 => 'RTK Fixed',
    null => '—',
    _ => 'Fix $gpsFixType',
  };
}

/// QGC 风格的主页面顶部状态栏。
///
/// 左侧承载连接状态和飞行模式，右侧放运行中必须持续可见的 GPS、电池和链路状态。
class MgTopStatusBar extends StatelessWidget {
  const MgTopStatusBar({
    super.key,
    required this.data,
    this.vehicles = const [],
    this.activeVehicleId,
    this.selectedVehicleCount = 0,
    this.selectedVehicleIds = const {},
    this.onSelectVehicle,
    this.onOpenFleetBatch,
    this.onOpenFlightMessages,
    this.onToggleArm,
    this.onEmergencyStop,
    this.onOpenFlightMode,
    this.onOpenSettings,
    this.hasUpdateAvailable = false,
  });

  final MgTopStatusData data;
  final List<FleetVehicleSummary> vehicles;
  final int? activeVehicleId;
  final int selectedVehicleCount;
  final Set<int> selectedVehicleIds;
  final ValueChanged<int>? onSelectVehicle;
  final VoidCallback? onOpenFleetBatch;
  final VoidCallback? onOpenFlightMessages;
  final VoidCallback? onToggleArm;

  /// 急停（强制上锁）入口。仅在已解锁时出现——飞行中也要能触达。
  final VoidCallback? onEmergencyStop;
  final VoidCallback? onOpenFlightMode;
  final VoidCallback? onOpenSettings;
  final bool hasUpdateAvailable;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MgFloatingPanel(
      // 通栏：不要圆角与投影，只保留一条底边线。
      borderRadius: BorderRadius.zero,
      border: Border(bottom: BorderSide(color: colors.hairline)),
      showShadow: false,
      child: Material(
        color: colors.transparent,
        child: LayoutBuilder(
          builder: (context, constraints) {
            final width = constraints.maxWidth;
            final multiple = vehicles.length > 1;
            final height = mgTopStatusBarHeight(
              width,
              multipleVehicles: multiple,
            );
            final stacked = height > MgMetrics.toolbarHeight;
            final compactFleet = width < MgMetrics.fleetSelectorWideBreakpoint;
            final showSecondary = width >= (multiple ? 1260 : 1020);
            final primary = Row(
              children: [
                _MainStatus(data: data),
                const SizedBox(width: 12),
                Flexible(
                  child: _FlightMode(data: data, onPressed: onOpenFlightMode),
                ),
                if (multiple) ...[
                  const SizedBox(width: MgMetrics.fleetToolbarGap + 4),
                  _FleetVehicleSelector(
                    vehicles: vehicles,
                    compact: compactFleet,
                    activeVehicleId: activeVehicleId,
                    onPressed: onOpenFleetBatch,
                  ),
                ],
              ],
            );
            final metrics = Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                _GpsIndicator(data: data),
                const SizedBox(width: 20),
                _BatteryIndicator(data: data),
                if (showSecondary) ...[
                  const SizedBox(width: 20),
                  _LinkIndicator(data: data),
                ],
                const SizedBox(width: 12),
                if (data.connected && !data.stale && !data.flying) ...[
                  _ArmButton(data: data, onPressed: onToggleArm),
                  const SizedBox(width: 4),
                ],
                // 急停与常规上锁分开：常规上锁在飞行中被禁用，急停必须仍可触达。
                if (data.connected && !data.stale && data.armed) ...[
                  _EmergencyStopButton(data: data, onPressed: onEmergencyStop),
                  const SizedBox(width: 4),
                ],
                _TopActionButton(
                  icon: CupertinoIcons.text_bubble,
                  tooltip: '飞控消息',
                  badgeCount: data.messageCount,
                  onPressed: onOpenFlightMessages,
                ),
                if (onOpenSettings != null) ...[
                  const SizedBox(width: 4),
                  _SettingsButton(
                    onPressed: onOpenSettings!,
                    hasUpdate: hasUpdateAvailable,
                  ),
                ],
              ],
            );
            return SizedBox(
              height: height,
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 12),
                child: stacked
                    ? Column(
                        children: [
                          Expanded(child: primary),
                          Expanded(
                            child: Align(
                              alignment: Alignment.centerRight,
                              child: metrics,
                            ),
                          ),
                        ],
                      )
                    : Row(
                        children: [
                          Expanded(child: primary),
                          const SizedBox(width: 12),
                          metrics,
                        ],
                      ),
              ),
            );
          },
        ),
      ),
    );
  }
}

class _FleetVehicleSelector extends StatelessWidget {
  const _FleetVehicleSelector({
    required this.vehicles,
    required this.activeVehicleId,
    required this.onPressed,
    required this.compact,
  });

  final bool compact;
  final List<FleetVehicleSummary> vehicles;
  final int? activeVehicleId;
  final VoidCallback? onPressed;

  String _label(FleetVehicleSummary vehicle) =>
      fleetVehicleLabel(vehicle, vehicles);

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final selected = vehicles.any((vehicle) => vehicle.id == activeVehicleId)
        ? activeVehicleId
        : vehicles.first.id;
    final active = vehicles.firstWhere((vehicle) => vehicle.id == selected);
    return Tooltip(
      message: '选择飞机：${_label(active)}',
      child: SizedBox(
        key: const ValueKey('fleet-vehicle-selector'),
        width: compact ? 148 : 202,
        child: Material(
          color: colors.surfaceMuted,
          borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          child: InkWell(
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
            onTap: onPressed,
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      _label(active),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.toolbar.copyWith(
                          color: colors.textPrimary,
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 4),
                  Icon(
                    CupertinoIcons.chevron_down,
                    size: 14,
                    color: colors.textTertiary,
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _EmergencyStopButton extends StatelessWidget {
  const _EmergencyStopButton({required this.data, required this.onPressed});

  final MgTopStatusData data;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final enabled = data.connected && !data.stale && onPressed != null;
    return MacosTooltip(
      message: data.connected ? '急停：跳过解锁前检查强制上锁，立即停止电机' : '未连接飞控',
      child: PushButton(
        controlSize: ControlSize.regular,
        onPressed: enabled ? onPressed : null,
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.dangerous_outlined,
              size: 18,
              color: enabled ? colors.danger : colors.textTertiary,
            ),
            const SizedBox(width: 7),
            Text(
              '急停',
              style: MgTypography.sans(
                MgTypography.toolbar.copyWith(
                  color: enabled ? colors.danger : null,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ArmButton extends StatelessWidget {
  const _ArmButton({required this.data, required this.onPressed});

  final MgTopStatusData data;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final enabled =
        data.connected && !data.stale && !data.flying && onPressed != null;
    final label = data.armed ? '上锁' : '解锁';
    final tooltip = !data.connected
        ? '未连接飞控'
        : data.flying
        ? '飞行中禁止上锁'
        : data.armed
        ? '上锁并停止电机'
        : '解锁飞控';
    return MacosTooltip(
      message: tooltip,
      child: PushButton(
        controlSize: ControlSize.regular,
        secondary: data.armed,
        onPressed: enabled ? onPressed : null,
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              data.armed ? Icons.lock : Icons.lock_open,
              size: 18,
              color: enabled
                  ? (data.armed ? colors.textPrimary : colors.danger)
                  : colors.textTertiary,
            ),
            const SizedBox(width: 7),
            Text(label, style: MgTypography.sans(MgTypography.toolbar)),
          ],
        ),
      ),
    );
  }
}

class _TopActionButton extends StatelessWidget {
  const _TopActionButton({
    required this.icon,
    required this.tooltip,
    required this.onPressed,
    this.badgeCount = 0,
  });

  final IconData icon;
  final String tooltip;
  final VoidCallback? onPressed;
  final int badgeCount;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MacosTooltip(
      message: tooltip,
      child: Stack(
        clipBehavior: Clip.none,
        children: [
          MacosIconButton(
            icon: MacosIcon(icon, size: 18),
            semanticLabel: tooltip,
            boxConstraints: const BoxConstraints.tightFor(
              width: 36,
              height: 36,
            ),
            padding: EdgeInsets.zero,
            backgroundColor: colors.transparent,
            hoverColor: colors.mapControlHover,
            onPressed: onPressed,
          ),
          if (badgeCount > 0)
            Positioned(
              right: -1,
              top: -2,
              child: Container(
                constraints: const BoxConstraints(minWidth: 20, minHeight: 20),
                padding: const EdgeInsets.symmetric(horizontal: 3),
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: colors.danger,
                  borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
                ),
                child: Text(
                  '$badgeCount',
                  style: MgTypography.sans(
                    MgTypography.toolbar.copyWith(
                      height: 1,
                      color: colors.onAccent,
                    ),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _SettingsButton extends StatelessWidget {
  const _SettingsButton({required this.onPressed, required this.hasUpdate});

  final VoidCallback onPressed;
  final bool hasUpdate;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MacosTooltip(
      message: hasUpdate ? '设置 · 有可用更新' : '设置',
      child: Stack(
        clipBehavior: Clip.none,
        children: [
          MacosIconButton(
            icon: MacosIcon(CupertinoIcons.gear, size: 18),
            semanticLabel: hasUpdate ? '设置，有可用更新' : '设置',
            boxConstraints: const BoxConstraints.tightFor(
              width: 36,
              height: 36,
            ),
            padding: EdgeInsets.zero,
            backgroundColor: colors.transparent,
            hoverColor: colors.mapControlHover,
            onPressed: onPressed,
          ),
          if (hasUpdate)
            Positioned(
              right: 3,
              top: 4,
              child: Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(
                  color: colors.warning,
                  shape: BoxShape.circle,
                  border: Border.all(color: colors.toolbar, width: 1.5),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _MainStatus extends StatelessWidget {
  const _MainStatus({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final statusColor = !data.connected
        ? colors.danger
        : data.flying
        ? colors.accent
        : data.armed
        ? colors.success
        : colors.warning;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: statusColor.withValues(alpha: colors.isDark ? 0.18 : 0.10),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            AnimatedContainer(
              duration: const Duration(milliseconds: 200),
              width: 8,
              height: 8,
              decoration: BoxDecoration(
                color: statusColor,
                shape: BoxShape.circle,
              ),
            ),
            const SizedBox(width: 7),
            Text(
              data.statusLabel,
              style: MgTypography.sans(
                MgTypography.toolbar.copyWith(color: colors.textPrimary),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FlightMode extends StatelessWidget {
  const _FlightMode({required this.data, this.onPressed});

  final MgTopStatusData data;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    // 过期（含断链后保留的最后一帧）时不能把旧模式名当成当前模式。
    final stale = data.stale;
    return InkWell(
      onTap: data.connected && !stale ? onPressed : null,
      borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
        child: Text(
          stale ? '—' : data.modeLabel,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MgTypography.sans(
            MgTypography.toolbar.copyWith(
              color: stale ? colors.textTertiary : colors.textPrimary,
            ),
          ),
        ),
      ),
    );
  }
}

class _GpsIndicator extends StatelessWidget {
  const _GpsIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final stale = data.stale;
    final fixType = data.gpsFixType;
    final satellites = data.satellitesVisible;
    // 没有可用读数时用中性色，不把"未知"画成告警；有读数才按定位质量变色。
    final hasData = data.connected && !stale && fixType != null;
    final fix = fixType ?? 0;
    final color = !hasData
        ? colors.textSecondary
        : fix >= 3
        ? colors.success
        : colors.warning;
    return MacosTooltip(
      message: stale ? 'GPS：数据已过期' : 'GPS：${data.gpsFixLabel}',
      child: _TopBarMetric(
        icon: MgStatusTelemetryIcon(
          kind: MgStatusIconKind.gps,
          value: hasData ? (satellites ?? 0) / 20 : 0,
          color: color,
        ),
        value: MgAnimatedNumber(
          // 过期时直接用占位符，避免把最后一帧星数当成当前值。
          value: stale ? double.nan : (satellites?.toDouble() ?? double.nan),
          fractionDigits: 0,
          textAlign: TextAlign.left,
          style: MgTypography.toolbar,
          color: colors.textPrimary,
          placeholder: '—',
        ),
        suffix: stale || satellites == null ? null : '星',
      ),
    );
  }
}

class _BatteryIndicator extends StatelessWidget {
  const _BatteryIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final stale = data.stale;
    final voltage = stale ? null : data.batteryVoltage;
    final percent = stale ? null : data.batteryRemainingPercent;
    // 同样：没有电量读数时用中性色，读数有效时才按电量分档（绿 / 黄 / 红）。
    final hasData = data.connected && !stale && percent != null;
    final charge = percent ?? 0;
    final color = !hasData
        ? colors.textSecondary
        : charge <= 20
        ? colors.danger
        : charge <= 35
        ? colors.warning
        : colors.success;
    return MacosTooltip(
      message: stale
          ? '电池：数据已过期'
          : '电池：${percent == null ? '—' : '$percent%'}'
                '${voltage == null ? '' : ' · ${voltage.toStringAsFixed(1)} V'}',
      child: _TopBarMetric(
        icon: MgStatusTelemetryIcon(
          kind: MgStatusIconKind.battery,
          value: (percent ?? 0) / 100,
          color: color,
        ),
        value: percent == null
            ? Text(
                voltage == null ? '—' : '${voltage.toStringAsFixed(1)} V',
                style: MgTypography.sans(
                  MgTypography.toolbar.copyWith(color: colors.textPrimary),
                ),
              )
            : MgAnimatedNumber(
                value: percent.toDouble(),
                fractionDigits: 0,
                textAlign: TextAlign.left,
                style: MgTypography.toolbar,
                color: colors.textPrimary,
              ),
        suffix: percent == null ? null : '%',
      ),
    );
  }
}

class _LinkIndicator extends StatelessWidget {
  const _LinkIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final stale = data.stale;
    return MacosTooltip(
      message: stale
          ? (data.connected ? '链路在，但遥测数据已过期' : '链路已断开，遥测数据已过期')
          : (data.connected ? '链路已连接' : '链路未连接'),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          AnimatedSwitcher(
            duration: const Duration(milliseconds: 180),
            child: Icon(
              stale ? Icons.warning_amber_rounded : CupertinoIcons.link,
              key: ValueKey((stale, data.connected)),
              size: 18,
              color: stale
                  ? colors.warning
                  : (data.connected ? colors.success : colors.danger),
            ),
          ),
          const SizedBox(width: 8),
          Text(
            stale ? '数据过期' : (data.connected ? data.transportLabel : '未连接'),
            style: MgTypography.sans(
              MgTypography.toolbar.copyWith(
                color: stale ? colors.warning : colors.textSecondary,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _TopBarMetric extends StatelessWidget {
  const _TopBarMetric({required this.icon, required this.value, this.suffix});

  final Widget icon;
  final Widget value;
  final String? suffix;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        icon,
        const SizedBox(width: 8),
        value,
        if (suffix != null) ...[
          const SizedBox(width: 2),
          Text(
            suffix!,
            style: MgTypography.sans(
              MgTypography.toolbar.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ],
    );
  }
}
