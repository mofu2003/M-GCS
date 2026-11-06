import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import 'flight_message_translator.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_floating_panel.dart';

/// 飞控消息框宽度。
const double kFlightMessagePanelWidth = 360;

/// 飞控消息框。
///
/// 显示飞控来的 STATUSTEXT（MP 的 Messages 页、QGC 的 Messages 面板等价物）：
/// 时间 + 按严重级别着色的文本，自动滚到最新一条。数据由后端环形缓冲提供，
/// 前端按 seq 增量拉取。
class FlightMessageFloatingPanel extends StatefulWidget {
  const FlightMessageFloatingPanel({
    super.key,
    required this.controller,
    required this.onClose,
  });

  final ConnectionController controller;
  final VoidCallback onClose;

  @override
  State<FlightMessageFloatingPanel> createState() =>
      _FlightMessageFloatingPanelState();
}

class _FlightMessageFloatingPanelState
    extends State<FlightMessageFloatingPanel> {
  Timer? _pollTimer;
  final ScrollController _scroll = ScrollController();
  int _renderedCount = 0;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_handleControllerChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) unawaited(widget.controller.refreshFlightMessages());
    });
    // 面板只在显示时挂载，轮询随面板出现/消失自动开始和停止。
    _pollTimer = Timer.periodic(
      const Duration(milliseconds: 700),
      (_) => unawaited(widget.controller.refreshFlightMessages()),
    );
  }

  void _handleControllerChanged() {
    if (!mounted) return;
    setState(() {});
    // 有新消息时滚到底部；用户手动往上翻时不打扰。
    if (widget.controller.flightMessages.length != _renderedCount) {
      _renderedCount = widget.controller.flightMessages.length;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!_scroll.hasClients) return;
        _scroll.jumpTo(_scroll.position.maxScrollExtent);
      });
    }
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    widget.controller.removeListener(_handleControllerChanged);
    _scroll.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final typography = MacosTheme.of(context).typography;
    final messages = widget.controller.flightMessages;
    return SizedBox(
      width: kFlightMessagePanelWidth,
      child: MgFloatingPanel(
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(14, 10, 8, 10),
              child: Row(
                children: [
                  Text('飞控消息', style: typography.title2),
                  const SizedBox(width: 8),
                  if (messages.isNotEmpty)
                    Text(
                      '${messages.length}',
                      style: typography.body.copyWith(
                        color: colors.textTertiary,
                      ),
                    ),
                  const Spacer(),
                  Tooltip(
                    message: '清空消息',
                    child: MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.trash, size: 13),
                      semanticLabel: '清空消息',
                      onPressed: messages.isEmpty
                          ? null
                          : widget.controller.clearFlightMessages,
                    ),
                  ),
                  Tooltip(
                    message: '关闭飞控消息',
                    child: MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 14),
                      semanticLabel: '关闭飞控消息',
                      onPressed: widget.onClose,
                    ),
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: colors.divider),
            Expanded(
              child: messages.isEmpty
                  ? Align(
                      alignment: Alignment.topLeft,
                      child: Padding(
                        padding: const EdgeInsets.all(12),
                        child: Text(
                          widget.controller.isConnected ? '暂无飞控消息' : '未连接飞控',
                          style: typography.body.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                    )
                  : Scrollbar(
                      controller: _scroll,
                      child: ListView.builder(
                        controller: _scroll,
                        padding: const EdgeInsets.fromLTRB(12, 8, 12, 10),
                        itemCount: messages.length,
                        itemBuilder: (context, index) => _MessageLine(
                          message: messages[index],
                          colors: colors,
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

class _MessageLine extends StatelessWidget {
  const _MessageLine({required this.message, required this.colors});

  final FlightMessage message;
  final MgColors colors;

  /// 严重级别配色：error 及以上用危险色，warning 用警告色，其余用次要文本色。
  Color get _color {
    if (message.severity <= 3) return colors.danger;
    if (message.severity == 4) return colors.warning;
    if (message.severity >= 7) return colors.textTertiary;
    return colors.textPrimary;
  }

  String get _time {
    final time = message.time;
    if (time == null) return '--:--:--';
    String two(int v) => v.toString().padLeft(2, '0');
    return '${two(time.hour)}:${two(time.minute)}:${two(time.second)}';
  }

  @override
  Widget build(BuildContext context) {
    final translation = translateFlightMessage(message.text);
    final content = SelectableText(
      translation.displayText,
      style: MgTypography.sans(
        MgTypography.flightMessageBody.copyWith(color: _color),
      ),
    );
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.baseline,
        textBaseline: TextBaseline.alphabetic,
        children: [
          SizedBox(
            width: MgMetrics.flightMessageTimeWidth,
            child: Text(
              _time,
              textAlign: TextAlign.right,
              style: MgTypography.sans(
                MgTypography.flightMessageTime.copyWith(
                  color: colors.textTertiary,
                ),
              ),
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: DecoratedBox(
              decoration: BoxDecoration(
                border: Border(
                  left: BorderSide(color: colors.hairline, width: 0.5),
                ),
              ),
              child: Padding(
                padding: const EdgeInsets.only(left: 8),
                child: translation.translated
                    ? MacosTooltip(
                        message: '原文：${translation.originalText}',
                        child: content,
                      )
                    : content,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
