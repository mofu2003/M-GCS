import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 统一的按住确认时长。
///
/// 飞行模式列表和地图悬浮指令确认按钮共用这一处定义，避免不同长按入口出现
/// 一个 300 ms、一个 1 s 的体验偏差。
const Duration kMgHoldToConfirmDuration = Duration(milliseconds: 500);

/// 浮在地图上方、水平居中的指令确认条。
///
/// 它独立于顶部状态栏：状态栏继续只展示飞行状态，确认按钮可以按需出现和消失，
/// 不会挤压 GPS、电池、链路等持续可见信息。
class MgCommandConfirmationOverlay extends StatelessWidget {
  const MgCommandConfirmationOverlay({
    super.key,
    required this.command,
    required this.onConfirmed,
    this.onCancelled,
  });

  final MgCommandConfirmation command;
  final VoidCallback? onConfirmed;
  final VoidCallback? onCancelled;

  @override
  Widget build(BuildContext context) {
    return Align(
      alignment: Alignment.topCenter,
      child: Padding(
        padding: const EdgeInsets.only(top: MgMetrics.commandConfirmTopInset),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Row(
              key: const ValueKey('command-confirmation-overlay-row'),
              mainAxisSize: MainAxisSize.min,
              children: [
                MgHoldToConfirmButton(
                  key: ValueKey(command.label),
                  label: command.label,
                  enabled: onConfirmed != null,
                  onConfirmed: onConfirmed ?? () {},
                ),
                if (onCancelled != null) ...[
                  const SizedBox(width: 3),
                  MacosTooltip(
                    message: '取消指令确认',
                    child: MacosIconButton(
                      key: const ValueKey('cancel-command-confirmation'),
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 12),
                      semanticLabel: '取消指令确认',
                      boxConstraints: const BoxConstraints.tightFor(
                        width: 26,
                        height: 26,
                      ),
                      padding: EdgeInsets.zero,
                      backgroundColor: context.mgColors.transparent,
                      hoverColor: context.mgColors.mapControlHover,
                      onPressed: onCancelled,
                    ),
                  ),
                ],
              ],
            ),
            if (command.detail != null && command.detail!.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  command.detail!,
                  key: const ValueKey('command-confirmation-detail'),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(
                      color: context.mgColors.textPrimary,
                      shadows: [
                        Shadow(
                          color: context.mgColors.shadow.withValues(
                            alpha: 0.35,
                          ),
                          blurRadius: 3,
                        ),
                      ],
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}

/// 一条等待用户长按确认的指令。
///
/// 只有 [execute] 会被真正下发；按钮本身不关心业务状态，地图、返航、任务启动
/// 等入口都通过这个对象把“待确认指令”交给地图悬浮确认层。
@immutable
class MgCommandConfirmation {
  const MgCommandConfirmation({
    required this.label,
    required this.execute,
    this.detail,
  });

  /// 顶部确认按钮显示的命令文案，例如“移动到此处”“返航”“开始任务”。
  final String label;

  /// 可选的第二行目标摘要，用于批量指令显示飞机数量与系统 ID。
  final String? detail;

  /// 长按确认后执行的实际命令。
  final Future<void> Function() execute;
}

/// 地图上方悬浮确认层使用的长按确认按钮。
///
/// 交互与飞行模式行一致：按住开始填进度，松手或移出取消，进度拉满后触发
/// [onConfirmed]。这里不注册短按动作，避免用户误触直接下发飞行指令。
class MgHoldToConfirmButton extends StatefulWidget {
  const MgHoldToConfirmButton({
    super.key,
    required this.label,
    required this.onConfirmed,
    this.enabled = true,
    this.compact = false,
  });

  final String label;
  final VoidCallback onConfirmed;
  final bool enabled;

  /// 窄窗口下隐藏“长按确认”提示，只保留命令名和进度。
  final bool compact;

  @override
  State<MgHoldToConfirmButton> createState() => _MgHoldToConfirmButtonState();
}

class _MgHoldToConfirmButtonState extends State<MgHoldToConfirmButton>
    with TickerProviderStateMixin {
  static const Duration _checkDuration = Duration(milliseconds: 180);

  late final AnimationController _hold = AnimationController(
    vsync: this,
    duration: kMgHoldToConfirmDuration,
  );
  late final AnimationController _check = AnimationController(
    vsync: this,
    duration: _checkDuration,
  );
  bool _confirmed = false;

  @override
  void initState() {
    super.initState();
    _hold.addStatusListener(_handleHoldStatus);
    _check.addStatusListener(_handleCheckStatus);
  }

  void _handleHoldStatus(AnimationStatus status) {
    if (status != AnimationStatus.completed || _confirmed) return;
    _confirmed = true;
    _check.forward(from: 0);
  }

  void _handleCheckStatus(AnimationStatus status) {
    if (status != AnimationStatus.completed) return;
    _reset();
    widget.onConfirmed();
  }

  void _reset() {
    _confirmed = false;
    _hold.reset();
    _check.reset();
  }

  @override
  void didUpdateWidget(covariant MgHoldToConfirmButton oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.label != widget.label ||
        (!widget.enabled && (_hold.isAnimating || _check.isAnimating))) {
      _hold.stop();
      _check.stop();
      _reset();
    }
  }

  @override
  void dispose() {
    _hold.dispose();
    _check.dispose();
    super.dispose();
  }

  void _start() {
    if (!widget.enabled || _confirmed) return;
    _hold.forward(from: 0);
  }

  void _cancel() {
    if (!widget.enabled ||
        _confirmed ||
        _hold.status == AnimationStatus.dismissed) {
      return;
    }
    _hold.reverse();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Semantics(
      button: true,
      label: '长按确认 ${widget.label}',
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTapDown: widget.enabled ? (_) => _start() : null,
        onTapUp: widget.enabled ? (_) => _cancel() : null,
        onTapCancel: widget.enabled ? _cancel : null,
        child: AnimatedBuilder(
          animation: Listenable.merge([_hold, _check]),
          builder: (context, _) {
            final progress = _hold.value;
            final foreground = widget.enabled
                ? colors.commandConfirmForeground
                : colors.textTertiary;
            return SizedBox(
              width: widget.compact
                  ? MgMetrics.commandConfirmCompactWidth
                  : MgMetrics.commandConfirmWidth,
              height: MgMetrics.controlHeight,
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
                  boxShadow: widget.enabled
                      ? [
                          BoxShadow(
                            color: colors.shadow.withValues(alpha: 0.16),
                            blurRadius: 7,
                            offset: const Offset(0, 2),
                          ),
                        ]
                      : null,
                ),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
                  child: Stack(
                    children: [
                      Positioned.fill(
                        child: ColoredBox(
                          color: widget.enabled
                              ? colors.commandConfirmSurface
                              : colors.surfaceMuted,
                        ),
                      ),
                      if (progress > 0)
                        Positioned.fill(
                          child: Align(
                            alignment: Alignment.centerLeft,
                            child: FractionallySizedBox(
                              widthFactor: progress,
                              heightFactor: 1,
                              child: ColoredBox(color: colors.accent),
                            ),
                          ),
                        ),
                      Positioned.fill(
                        child: Padding(
                          padding: const EdgeInsets.symmetric(horizontal: 10),
                          child: Row(
                            children: [
                              Expanded(
                                child: Text(
                                  widget.label,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: MgTypography.sans(
                                    MgTypography.body.copyWith(
                                      color: foreground,
                                    ),
                                  ),
                                ),
                              ),
                              if (_confirmed || _check.isAnimating)
                                ScaleTransition(
                                  scale: Tween<double>(begin: 0.65, end: 1)
                                      .animate(
                                        CurvedAnimation(
                                          parent: _check,
                                          curve: Curves.easeOut,
                                        ),
                                      ),
                                  child: FadeTransition(
                                    opacity: _check,
                                    child: Icon(
                                      CupertinoIcons.check_mark_circled_solid,
                                      size: 14,
                                      color: colors.onAccent,
                                    ),
                                  ),
                                )
                              else if (!widget.compact) ...[
                                const SizedBox(width: 8),
                                Text(
                                  '长按确认',
                                  style: MgTypography.sans(
                                    MgTypography.caption.copyWith(
                                      color: foreground.withValues(alpha: 0.78),
                                    ),
                                  ),
                                ),
                              ],
                            ],
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            );
          },
        ),
      ),
    );
  }
}
