import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_dialog.dart';

/// 飞行模式操作台宽度。
///
/// 比航线规划面板窄：它只有一列模式项，不需要任务卡片那么宽。
const double kFlightModePanelWidth = 220;

/// 飞行模式操作台。
///
/// 参考 QGC `FlightModeMenu.qml`（列出模式表、点选即切）与 MP
/// `GCSViews/FlightData.cs` 的 `CMB_modes`（按机型给模式表）。模式表与可用性都来自
/// 后端 `mg_flight_get_modes`，前端不硬编码模式号。
class FlightModeFloatingPanel extends StatefulWidget {
  const FlightModeFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<FlightModeFloatingPanel> createState() =>
      _FlightModeFloatingPanelState();
}

class _FlightModeFloatingPanelState extends State<FlightModeFloatingPanel> {
  Timer? _pollTimer;
  String? _pendingMode;
  bool _armBusy = false;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_handleControllerChanged);
    // 首帧之后再拉模式表：刷新会同步 notifyListeners，直接在 initState 里调用会在
    // build 期间触发 setState（"setState() called during build"）。
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) unawaited(widget.controller.refreshFlightModes());
    });
    // 面板只在显示时挂载，轮询随面板出现/消失自动开始和停止。
    _pollTimer = Timer.periodic(
      const Duration(seconds: 1),
      (_) => unawaited(widget.controller.refreshFlightModes()),
    );
  }

  void _handleControllerChanged() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    widget.controller.removeListener(_handleControllerChanged);
    super.dispose();
  }

  /// 解锁 / 上锁。两者都是安全敏感动作，按 MP 的习惯先确认再发。
  Future<void> _toggleArmed() async {
    final controller = widget.controller;
    final armed = controller.vehicleArmed;
    final confirmed = await _confirm(
      title: armed ? '上锁？' : '解锁？',
      message: armed
          ? '上锁会立即停止电机。飞行中上锁会掉飞机，地面测试请先拆桨。'
          : '解锁后电机可以转动。请确认机体周围无人、已拆桨或已做好安全防护。',
      confirmLabel: armed ? '上锁' : '解锁',
    );
    if (!confirmed || !mounted) return;
    setState(() => _armBusy = true);
    try {
      if (armed) {
        await controller.disarmVehicle();
      } else {
        await controller.armVehicle();
      }
    } on Object catch (error) {
      if (mounted) {
        await _confirm(
          title: armed ? '上锁失败' : '解锁失败',
          message: '$error',
          confirmLabel: '知道了',
          singleAction: true,
        );
      }
    } finally {
      if (mounted) setState(() => _armBusy = false);
    }
  }

  Future<bool> _confirm({
    required String title,
    required String message,
    required String confirmLabel,
    bool singleAction = false,
  }) async {
    final result = await showMgAlertDialog<bool>(
      context: context,
      builder: (dialogContext) => MacosAlertDialog(
        appIcon: MacosIcon(
          CupertinoIcons.exclamationmark_triangle,
          size: 44,
          color: dialogContext.mgColors.warning,
        ),
        title: Text(title),
        message: Text(message),
        // 确认键永远返回 true；之前这里 pop 的是 singleAction（默认 false），
        // 导致在弹窗里点「解锁」被当成"没确认"，命令根本发不出去。
        primaryButton: PushButton(
          controlSize: ControlSize.large,
          onPressed: () => Navigator.of(dialogContext).pop(true),
          child: Text(confirmLabel),
        ),
        // 单按钮提示（错误弹窗）不需要取消。
        secondaryButton: singleAction
            ? null
            : PushButton(
                controlSize: ControlSize.large,
                secondary: true,
                onPressed: () => Navigator.of(dialogContext).pop(false),
                child: const Text('取消'),
              ),
      ),
    );
    return result ?? false;
  }

  Future<void> _selectMode(String name) async {
    if (name == widget.controller.vehicleFlightMode) return;
    setState(() => _pendingMode = name);
    try {
      await widget.controller.setFlightMode(name);
    } on Object catch (error) {
      if (mounted) {
        await showMgAlertDialog<void>(
          context: context,
          builder: (dialogContext) => MacosAlertDialog(
            appIcon: MacosIcon(
              CupertinoIcons.exclamationmark_triangle,
              size: 44,
              color: dialogContext.mgColors.warning,
            ),
            title: const Text('切换飞行模式失败'),
            message: Text('$error'),
            primaryButton: PushButton(
              controlSize: ControlSize.large,
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('知道了'),
            ),
          ),
        );
      }
    } finally {
      if (mounted) setState(() => _pendingMode = null);
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    return SizedBox(
      width: kFlightModePanelWidth,
      child: Container(
        // 与航线规划面板共用同一套外框：surfaceMuted + panelRadius + 分隔线 + 投影。
        decoration: BoxDecoration(
          color: colors.surfaceMuted,
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
        clipBehavior: Clip.antiAlias,
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(14, 10, 8, 10),
              child: Row(
                children: [
                  Text('飞行模式', style: typography.title3),
                  const Spacer(),
                  Tooltip(
                    message: '关闭飞行模式',
                    child: MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 14),
                      semanticLabel: '关闭飞行模式',
                      onPressed: widget.onClose,
                    ),
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: colors.divider),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(12, 10, 12, 12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    _CurrentModeHeader(
                      mode: widget.controller.vehicleFlightMode,
                      armed: widget.controller.vehicleArmed,
                      vtolState: widget.controller.vehicleVtolState,
                    ),
                    const SizedBox(height: 8),
                    _ArmButton(
                      armed: widget.controller.vehicleArmed,
                      enabled: widget.controller.isConnected && !_armBusy,
                      busy: _armBusy,
                      onPressed: () => unawaited(_toggleArmed()),
                    ),
                    const SizedBox(height: 8),
                    Divider(height: 1, color: colors.divider),
                    const SizedBox(height: 8),
                    Expanded(child: _buildModeList(context)),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildModeList(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    if (!widget.controller.isConnected) {
      return _Hint(text: '未连接飞控，模式表不可用', color: colors.textSecondary);
    }
    final options = widget.controller.flightModeOptions;
    if (options.isEmpty) {
      return _Hint(text: '正在读取模式表…', color: colors.textSecondary);
    }
    final current = widget.controller.vehicleFlightMode;
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          for (final option in options)
            _FlightModeRow(
              option: option,
              selected: option.name == current,
              pending: option.name == _pendingMode,
              enabled: option.settable,
              onTap: () => unawaited(_selectMode(option.name)),
            ),
          const SizedBox(height: 2),
          Text(
            '仅显示当前机型支持的模式',
            style: typography.caption2.copyWith(color: colors.textTertiary),
          ),
        ],
      ),
    );
  }
}

/// 解锁 / 上锁按钮。整行宽，避免和模式列表抢横向空间。
class _ArmButton extends StatelessWidget {
  const _ArmButton({
    required this.armed,
    required this.enabled,
    required this.busy,
    required this.onPressed,
  });

  final bool armed;
  final bool enabled;
  final bool busy;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    // 解锁是"让电机能转"，用危险色提醒；上锁是回到安全态，用普通按钮。
    final style = armed
        ? PushButton(
            controlSize: ControlSize.regular,
            secondary: true,
            onPressed: enabled ? onPressed : null,
            child: const Text('上锁'),
          )
        : PushButton(
            controlSize: ControlSize.regular,
            onPressed: enabled ? onPressed : null,
            child: Text(
              '解锁',
              style: MacosTheme.of(
                context,
              ).typography.body.copyWith(color: colors.onAccent),
            ),
          );
    return SizedBox(
      width: double.infinity,
      child: busy
          ? const Align(
              alignment: Alignment.center,
              child: ProgressCircle(radius: 8),
            )
          : style,
    );
  }
}

class _CurrentModeHeader extends StatelessWidget {
  const _CurrentModeHeader({
    required this.mode,
    required this.armed,
    required this.vtolState,
  });

  final String mode;
  final bool armed;
  final int vtolState;

  String get _displayMode {
    final base = mode.isEmpty ? '—' : mode;
    return switch (vtolState) {
      1 => '$base · 正在转换为固定翼',
      2 => '$base · 正在转换为多旋翼',
      3 => '$base · MC',
      4 => '$base · FW',
      _ => base,
    };
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    return Row(
      children: [
        MacosIcon(
          CupertinoIcons.location_north,
          size: 13,
          color: colors.accent,
        ),
        const SizedBox(width: 6),
        Expanded(
          child: Text(
            _displayMode,
            style: typography.title3,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        if (armed)
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 1),
            decoration: BoxDecoration(
              color: colors.warning.withValues(alpha: 0.16),
              borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
            ),
            child: Text(
              '已解锁',
              style: typography.caption2.copyWith(color: colors.warning),
            ),
          ),
      ],
    );
  }
}

class _Hint extends StatelessWidget {
  const _Hint({required this.text, required this.color});

  final String text;
  final Color color;

  @override
  Widget build(BuildContext context) => Align(
    alignment: Alignment.topLeft,
    child: Text(
      text,
      style: MacosTheme.of(context).typography.caption1.copyWith(color: color),
    ),
  );
}

class _FlightModeRow extends StatelessWidget {
  const _FlightModeRow({
    required this.option,
    required this.selected,
    required this.pending,
    required this.enabled,
    required this.onTap,
  });

  final FlightModeOption option;
  final bool selected;
  final bool pending;
  final bool enabled;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    final foreground = !enabled
        ? colors.textTertiary
        : selected
        ? colors.accent
        : colors.textPrimary;
    return MouseRegion(
      cursor: enabled ? SystemMouseCursors.click : SystemMouseCursors.basic,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: enabled ? onTap : null,
        child: Container(
          height: MgMetrics.controlHeight,
          padding: const EdgeInsets.symmetric(horizontal: 8),
          decoration: BoxDecoration(
            color: selected ? colors.accentSurface : colors.transparent,
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          ),
          child: Row(
            children: [
              Expanded(
                child: Text(
                  option.name,
                  style: typography.body.copyWith(color: foreground),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
              if (pending)
                const SizedBox(
                  width: 12,
                  height: 12,
                  child: ProgressCircle(radius: 6),
                )
              else if (selected)
                MacosIcon(
                  CupertinoIcons.checkmark_alt,
                  size: 12,
                  color: colors.accent,
                )
              else if (!option.settable)
                Text(
                  '不可切换',
                  style: typography.caption2.copyWith(
                    color: colors.textTertiary,
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
