import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mac_panel.dart';

/// 飞行模式操作台宽度。
///
/// 与航线规划面板一样贴边悬浮，但更窄：它只有一列模式项，不需要任务卡片那么宽。
const double kFlightModePanelWidth = 240;

/// 飞行模式操作台。
///
/// 参考 QGC `FlightModeMenu.qml`（点当前模式弹出模式列表）与 MP
/// `GCSViews/FlightData.cs` 的 `CMB_modes`（按机型给模式表、选中即切）。
/// 列表与 gating 都来自后端 `mg_flight_get_modes`，前端不硬编码模式号。
class FlightModeFloatingPanel extends StatefulWidget {
  const FlightModeFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<FlightModeFloatingPanel> createState() => _FlightModeFloatingPanelState();
}

class _FlightModeFloatingPanelState extends State<FlightModeFloatingPanel> {
  Timer? _pollTimer;
  String? _pendingMode;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_handleControllerChanged);
    unawaited(widget.controller.refreshFlightModes());
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

  Future<void> _selectMode(String name) async {
    if (name == widget.controller.vehicleFlightMode) return;
    setState(() => _pendingMode = name);
    try {
      await widget.controller.setFlightMode(name);
    } on Object catch (error) {
      if (mounted) {
        await showMacosAlertDialog<void>(
          context: context,
          builder: (dialogContext) => MacosAlertDialog(
            appIcon: const Icon(CupertinoIcons.exclamationmark_triangle, size: 40),
            title: const Text('切换飞行模式失败'),
            message: Text('$error'),
            primaryButton: PushButton(
              controlSize: ControlSize.regular,
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
    final connected = widget.controller.isConnected;
    final current = widget.controller.vehicleFlightMode;
    final options = widget.controller.flightModeOptions;

    return SizedBox(
      width: kFlightModePanelWidth,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              MacosIcon(CupertinoIcons.airplane, size: 14),
              const SizedBox(width: 6),
              Text('飞行模式', style: typography.title3),
              const Spacer(),
              MacosIconButton(
                icon: const MacosIcon(CupertinoIcons.xmark, size: 14),
                semanticLabel: '关闭飞行模式',
                onPressed: widget.onClose,
              ),
            ],
          ),
          const SizedBox(height: 6),
          MacPanel(
            padding: const EdgeInsets.fromLTRB(10, 10, 10, 10),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        current.isEmpty ? '—' : current,
                        style: typography.title2,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                    if (widget.controller.vehicleArmed)
                      Text(
                        '已解锁',
                        style: typography.caption1.copyWith(
                          color: colors.warning,
                        ),
                      ),
                  ],
                ),
                const SizedBox(height: 8),
                if (!connected)
                  Text(
                    '未连接飞控，模式表不可用',
                    style: typography.caption1.copyWith(
                      color: colors.textSecondary,
                    ),
                  )
                else if (options.isEmpty)
                  Text(
                    '正在读取模式表…',
                    style: typography.caption1.copyWith(
                      color: colors.textSecondary,
                    ),
                  )
                else
                  ConstrainedBox(
                    constraints: const BoxConstraints(
                      maxHeight: MgMetrics.sidebarWidth,
                    ),
                    child: SingleChildScrollView(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          for (final option in options)
                            _FlightModeRow(
                              option: option,
                              selected: option.name == current,
                              pending: option.name == _pendingMode,
                              enabled: connected && option.settable,
                              onTap: () => unawaited(_selectMode(option.name)),
                            ),
                        ],
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }
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
          margin: const EdgeInsets.only(bottom: 2),
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
