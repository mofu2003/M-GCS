import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../flight/flight_mode_labels.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_animated_number.dart';
import 'mg_floating_panel.dart';

Future<void> showMgFlightModeMenu(
  BuildContext context, {
  required ConnectionController controller,
  required List<FlightModeOption> options,
}) async {
  await showGeneralDialog<void>(
    context: context,
    barrierDismissible: true,
    barrierLabel: '关闭飞行模式列表',
    barrierColor: context.mgColors.shadow.withValues(alpha: 0.12),
    transitionDuration: const Duration(milliseconds: 120),
    pageBuilder: (dialogContext, animation, secondaryAnimation) => Align(
      alignment: Alignment.topLeft,
      child: Padding(
        padding: const EdgeInsets.only(
          top: MgMetrics.toolbarHeight + 8,
          left: 168,
        ),
        child: _FlightModeMenu(controller: controller, options: options),
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
  const _FlightModeMenu({required this.controller, required this.options});

  final ConnectionController controller;
  final List<FlightModeOption> options;

  Future<void> _setMode(BuildContext context, FlightModeOption option) async {
    if (!option.settable) return;
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
            color: Colors.transparent,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 8, 12, 6),
                  child: Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      '按住切换',
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
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
  /// 按住多久算确认。QGC 的 hold-to-confirm 量级是 1~2 秒，这里取 1 s。
  static const Duration holdDuration = Duration(seconds: 1);

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
            MgTypography.body.copyWith(
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
                height: MgMetrics.controlHeight,
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
                              MgTypography.caption.copyWith(
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
  });

  factory MgTopStatusData.fromController(ConnectionController controller) {
    return MgTopStatusData(
      connected: controller.isConnected,
      armed: controller.vehicleArmed,
      flying: controller.vehicleFlying,
      flightMode: controller.vehicleFlightMode,
      vtolState: controller.vehicleVtolState,
      gpsFixType: controller.gpsFixType,
      satellitesVisible: controller.satellitesVisible,
      batteryRemainingPercent: controller.batteryRemainingPercent,
      batteryVoltage: controller.batteryVoltage,
      transportLabel: controller.selectedTransportLabel,
      messageCount: controller.flightMessages.length,
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

  String get statusLabel {
    if (!connected) return '未连接';
    if (flying) return '飞行中';
    if (armed) return '已解锁';
    return '待命';
  }

  /// VTOL 状态后缀（对应 MAV_VTOL_STATE）。
  ///
  /// 界面以中文为主，所以这里用中文描述状态，不写 MC/FW：
  /// 1=TRANSITION_TO_FW、2=TRANSITION_TO_MC、3=MC、4=FW；
  /// 0=UNDEFINED（非 VTOL 或还没上报）时不加后缀。
  String get modeLabel {
    if (flightMode.isEmpty) return '—';
    return switch (vtolState) {
      1 => '$flightMode · 转固定翼',
      2 => '$flightMode · 转多旋翼',
      3 => '$flightMode · 多旋翼',
      4 => '$flightMode · 固定翼',
      _ => flightMode,
    };
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
    this.onOpenFlightMessages,
    this.onToggleArm,
    this.onOpenFlightMode,
    this.onOpenSettings,
  });

  final MgTopStatusData data;
  final VoidCallback? onOpenFlightMessages;
  final VoidCallback? onToggleArm;
  final VoidCallback? onOpenFlightMode;
  final VoidCallback? onOpenSettings;

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
        child: SizedBox(
          height: MgMetrics.toolbarHeight,
          child: LayoutBuilder(
            builder: (context, constraints) {
              final width = constraints.maxWidth;
              // 窄窗口逐级收起次要信息，防止右侧操作组被挤出（QGC 工具栏同样分级隐藏）。
              final showSecondary = width >= 860;
              final showStatusText = width >= 660;
              final showMetricLabels = width >= 700;
              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 12),
                child: Row(
                  children: [
                    _MainStatus(data: data, showText: showStatusText),
                    const SizedBox(width: 18),
                    _FlightMode(data: data, onPressed: onOpenFlightMode),
                    // 运行中必须持续可见的 GPS / 电池 / 链路 / 操作组统一贴右。
                    const Spacer(),
                    const SizedBox(width: 12),
                    _GpsIndicator(data: data, showLabel: showMetricLabels),
                    const SizedBox(width: 10),
                    _BatteryIndicator(data: data, showLabel: showMetricLabels),
                    if (showSecondary) ...[
                      const SizedBox(width: 16),
                      _LinkIndicator(data: data),
                    ],
                    const SizedBox(width: 12),
                    if (data.connected && !data.flying) ...[
                      _ArmButton(data: data, onPressed: onToggleArm),
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
                      _SettingsButton(onPressed: onOpenSettings!),
                    ],
                  ],
                ),
              );
            },
          ),
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
    final enabled = data.connected && !data.flying && onPressed != null;
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
              size: 14,
              color: enabled
                  ? (data.armed ? colors.textPrimary : colors.danger)
                  : colors.textTertiary,
            ),
            const SizedBox(width: 5),
            Text(label),
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
            icon: MacosIcon(icon, size: 16),
            semanticLabel: tooltip,
            boxConstraints: const BoxConstraints.tightFor(
              width: 30,
              height: 30,
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
                constraints: const BoxConstraints(minWidth: 13, minHeight: 13),
                padding: const EdgeInsets.symmetric(horizontal: 3),
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: colors.danger,
                  borderRadius: BorderRadius.circular(7),
                ),
                child: Text(
                  '$badgeCount',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(
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
  const _SettingsButton({required this.onPressed});

  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MacosTooltip(
      message: '设置',
      child: MacosIconButton(
        icon: MacosIcon(CupertinoIcons.gear, size: 16),
        semanticLabel: '设置',
        boxConstraints: const BoxConstraints.tightFor(width: 30, height: 30),
        padding: EdgeInsets.zero,
        backgroundColor: colors.transparent,
        hoverColor: colors.mapControlHover,
        onPressed: onPressed,
      ),
    );
  }
}

class _MainStatus extends StatelessWidget {
  const _MainStatus({required this.data, this.showText = true});

  final MgTopStatusData data;
  final bool showText;

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
            Container(
              width: 7,
              height: 7,
              decoration: BoxDecoration(
                color: statusColor,
                shape: BoxShape.circle,
              ),
            ),
            if (showText) ...[
              const SizedBox(width: 7),
              Text(
                data.statusLabel,
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textPrimary),
                ),
              ),
            ],
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
    return InkWell(
      onTap: data.connected ? onPressed : null,
      borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              data.modeLabel,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: MgTypography.sans(
                MgTypography.data.copyWith(color: colors.textPrimary),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _GpsIndicator extends StatelessWidget {
  const _GpsIndicator({required this.data, this.showLabel = true});

  final MgTopStatusData data;
  final bool showLabel;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final color = !data.connected || (data.gpsFixType ?? 0) < 3
        ? colors.warning
        : colors.success;
    return MacosTooltip(
      message: 'GPS：${data.gpsFixLabel}',
      child: _TopBarMetric(
        icon: Icons.gps_fixed,
        label: 'GPS',
        showLabel: showLabel,
        color: color,
        value: MgAnimatedNumber(
          value: data.satellitesVisible?.toDouble() ?? double.nan,
          fractionDigits: 0,
          textAlign: TextAlign.left,
          style: MgTypography.data,
          color: colors.textPrimary,
          placeholder: '—',
        ),
        suffix: data.satellitesVisible == null ? null : '星',
      ),
    );
  }
}

class _BatteryIndicator extends StatelessWidget {
  const _BatteryIndicator({required this.data, this.showLabel = true});

  final MgTopStatusData data;
  final bool showLabel;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final percent = data.batteryRemainingPercent;
    final color = switch (percent) {
      null => colors.textSecondary,
      <= 20 => colors.danger,
      <= 35 => colors.warning,
      _ => colors.success,
    };
    final voltage = data.batteryVoltage;
    return MacosTooltip(
      message:
          '电池：${percent == null ? '—' : '$percent%'}'
          '${voltage == null ? '' : ' · ${voltage.toStringAsFixed(1)} V'}',
      child: _TopBarMetric(
        icon: Icons.battery_std,
        label: 'BAT',
        showLabel: showLabel,
        color: color,
        value: percent == null
            ? Text(
                voltage == null ? '—' : '${voltage.toStringAsFixed(1)} V',
                style: MgTypography.sans(
                  MgTypography.data.copyWith(color: colors.textPrimary),
                ),
              )
            : MgAnimatedNumber(
                value: percent.toDouble(),
                fractionDigits: 0,
                textAlign: TextAlign.left,
                style: MgTypography.data,
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
    return MacosTooltip(
      message: data.connected ? '链路已连接' : '链路未连接',
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            Icons.link,
            size: 15,
            color: data.connected ? colors.success : colors.danger,
          ),
          const SizedBox(width: 6),
          Text(
            data.connected ? data.transportLabel : '未连接',
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ),
    );
  }
}

class _TopBarMetric extends StatelessWidget {
  const _TopBarMetric({
    required this.icon,
    required this.label,
    required this.color,
    required this.value,
    this.showLabel = true,
    this.suffix,
  });

  final IconData icon;
  final String label;
  final Color color;
  final Widget value;
  final bool showLabel;
  final String? suffix;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 15, color: color),
        const SizedBox(width: 6),
        if (showLabel) ...[
          Text(
            label,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
          const SizedBox(width: 5),
        ],
        value,
        if (suffix != null) ...[
          const SizedBox(width: 2),
          Text(
            suffix!,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ],
    );
  }
}
