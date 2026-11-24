import 'dart:async';

import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../connection/connection_controller.dart';

/// 参数下载进度：进度条随下载推进，右侧显示「已加载/总数」。
class ParameterLoadProgress extends StatelessWidget {
  const ParameterLoadProgress({
    super.key,
    required this.received,
    required this.total,
    this.leading,
  });

  /// 已加载的参数数量。
  final int received;

  /// 飞控报告的参数总数；未知时为 0。
  final int total;

  /// 进度条左侧的可选说明文字。
  final String? leading;

  @override
  Widget build(BuildContext context) {
    final value = total > 0 ? (received * 100 / total).clamp(0.0, 100.0) : 0.0;
    final counter = '$received/$total';
    return Row(
      children: [
        if (leading case final text?) ...[
          Text(
            text,
            style: MacosTheme.of(context).typography.caption1.copyWith(
              color: context.mgColors.textSecondary,
            ),
          ),
          const SizedBox(width: 10),
        ],
        Expanded(
          child: TweenAnimationBuilder<double>(
            tween: Tween<double>(begin: value, end: value),
            duration: const Duration(milliseconds: 200),
            curve: Curves.linear,
            builder: (context, animated, child) =>
                ProgressBar(value: animated, semanticLabel: '已加载参数 $counter'),
          ),
        ),
        const SizedBox(width: 10),
        Text(
          counter,
          style: MacosTheme.of(context).typography.caption1.copyWith(
            color: context.mgColors.textSecondary,
            // 等宽数字，进度跳动时右侧数字不会左右晃动。
            fontFeatures: const [FontFeature.tabularFigures()],
          ),
        ),
      ],
    );
  }
}

/// 覆盖在顶部状态栏下沿，仅重绘当前飞机的参数加载进度。
class ToolbarParameterProgress extends StatefulWidget {
  const ToolbarParameterProgress({super.key, required this.controller});

  final ConnectionController controller;

  @override
  State<ToolbarParameterProgress> createState() =>
      _ToolbarParameterProgressState();
}

class _ToolbarParameterProgressState extends State<ToolbarParameterProgress>
    with SingleTickerProviderStateMixin {
  late final AnimationController _progress = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 200),
  )..addStatusListener(_handleAnimationStatus);
  Timer? _hideTimer;
  int? _sessionId;
  bool _visible = false;
  bool _complete = false;
  String _label = '';

  @override
  void initState() {
    super.initState();
    _sessionId = widget.controller.sessionId;
    widget.controller.connectionChanges.addListener(_sync);
    widget.controller.parameterChanges.addListener(_sync);
    _sync();
  }

  @override
  void didUpdateWidget(ToolbarParameterProgress oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller == widget.controller) return;
    oldWidget.controller.connectionChanges.removeListener(_sync);
    oldWidget.controller.parameterChanges.removeListener(_sync);
    _reset();
    _sessionId = widget.controller.sessionId;
    widget.controller.connectionChanges.addListener(_sync);
    widget.controller.parameterChanges.addListener(_sync);
    _sync();
  }

  void _reset() {
    _hideTimer?.cancel();
    _hideTimer = null;
    _visible = false;
    _complete = false;
    _progress.stop();
    _progress.value = 0;
  }

  void _handleAnimationStatus(AnimationStatus status) {
    if (status != AnimationStatus.completed || !_complete) return;
    _holdCompletion();
  }

  void _holdCompletion() {
    _hideTimer ??= Timer(const Duration(milliseconds: 300), () {
      if (mounted) setState(() => _visible = false);
    });
  }

  void _sync() {
    final controller = widget.controller;
    setState(() {
      if (_sessionId != controller.sessionId) {
        _reset();
        _sessionId = controller.sessionId;
      }
      if (!controller.isConnected || controller.parameterError != null) {
        _reset();
        return;
      }
      if (controller.parametersLoaded) {
        if (!_visible || _complete) return;
        _complete = true;
        _label = '参数已加载 100%';
        if (_progress.value == 1) {
          _holdCompletion();
        } else {
          _progress.animateTo(
            1,
            duration: _progress.duration,
            curve: Curves.easeOut,
          );
        }
        return;
      }
      if (_complete) _reset();
      _visible = true;
      final total = controller.parameterTotal;
      final received = controller.parameterReceived;
      final value = total > 0 ? (received / total).clamp(0.0, 1.0) : 0.0;
      _label = total > 0
          ? '正在加载参数 $received/$total · ${(value * 100).floor()}%'
          : '正在等待参数 · 0%';
      _progress.animateTo(
        value,
        duration: _progress.duration,
        curve: Curves.easeOut,
      );
    });
  }

  @override
  void dispose() {
    widget.controller.connectionChanges.removeListener(_sync);
    widget.controller.parameterChanges.removeListener(_sync);
    _hideTimer?.cancel();
    _progress.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (!_visible) return const SizedBox.shrink();
    return AnimatedBuilder(
      animation: _progress,
      builder: (context, _) => Tooltip(
        message: _label,
        child: LinearProgressIndicator(
          value: _progress.value,
          minHeight: 3,
          color: context.mgColors.accent,
          backgroundColor: context.mgColors.transparent,
          semanticsLabel: _label,
        ),
      ),
    );
  }
}
