import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';

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
