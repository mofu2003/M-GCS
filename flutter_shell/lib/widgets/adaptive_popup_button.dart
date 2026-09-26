import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

/// 一个受父级宽度约束的 macOS 弹出按钮。
///
/// macos_ui 的 [MacosPopupButton] 会按最宽的选项决定自身宽度，父级空间不足时
/// 会整行溢出并画出黄黑警告条。这里把内容宽度收敛到父级可用宽度以内，并让菜单项
/// 与按钮使用同一宽度，保证并列控件能够对齐。
class AdaptivePopupButton<T> extends StatelessWidget {
  const AdaptivePopupButton({
    super.key,
    required this.value,
    required this.options,
    required this.labelOf,
    this.onChanged,
    this.maxContentWidth = double.infinity,
  });

  /// 当前选中值；为 null 时按钮显示未选中状态。
  final T? value;

  /// 互斥选项列表。
  final List<T> options;

  /// 选项到显示文本的映射。
  final String Function(T value) labelOf;

  /// 变更回调；为 null 时按钮处于禁用状态。
  final ValueChanged<T>? onChanged;

  /// 内容区宽度上限，实际宽度不会超过父级可用空间。
  final double maxContentWidth;

  /// 弹层按钮除内容外的固定占位：箭头区 24 + 内边距 10 + 边框 1。
  static const double chromeWidth = 36;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final available = constraints.maxWidth;
      final availableContentWidth = available.isFinite
          ? (available - chromeWidth).clamp(0.0, double.infinity).toDouble()
          : (maxContentWidth.isFinite ? maxContentWidth : 180.0);
      final labels = options.map(labelOf).toList(growable: false);
      final textStyle = MacosTheme.of(context).typography.body;
      var longestTextWidth = 0.0;
      for (final label in labels) {
        final painter = TextPainter(
          text: TextSpan(text: label, style: textStyle),
          textDirection: TextDirection.ltr,
          maxLines: 1,
        )..layout();
        longestTextWidth = math.max(longestTextWidth, painter.width);
      }

      // MacosPopupButton fixes the transient menu to the trigger width. Each
      // menu row also spends width on the selection mark and padding. Keep a
      // small safety inset so even long RC function labels cannot overflow the
      // constrained menu row and paint the yellow/black warning stripe.
      final buttonWidth = availableContentWidth;
      final menuContentWidth = math.max(0.0, buttonWidth - 8.0);
      final menuWidth = math.min(longestTextWidth + 28, menuContentWidth);

      Widget optionLabel(
        T option, {
        required double width,
        bool alignLeft = false,
      }) => SizedBox(
        width: width,
        child: alignLeft
            ? Align(
                alignment: Alignment.centerLeft,
                child: Text(
                  labelOf(option),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
              )
            : Text(
                labelOf(option),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
      );

      return MacosPopupButton<T>(
        value: value,
        items: [
          for (final option in options)
            MacosPopupMenuItem<T>(
              value: option,
              child: optionLabel(option, width: menuWidth),
            ),
        ],
        selectedItemBuilder: (context) => [
          for (final option in options)
            optionLabel(option, width: buttonWidth, alignLeft: true),
        ],
        onChanged: onChanged == null
            ? null
            : (selected) {
                if (selected != null) onChanged!(selected);
              },
      );
    },
  );
}
