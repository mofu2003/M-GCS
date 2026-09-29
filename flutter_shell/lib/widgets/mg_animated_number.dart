import 'package:flutter/material.dart';

import '../theme/mg_typography.dart';

/// 平滑过渡的数字读数。
///
/// 直接替换文本在低刷新率下会一格一格地跳，这里用一个短补间过渡，并强制等宽
/// 数字，位数变化时不会左右抖动。
///
/// 补间时长必须远小于取数间隔：波形页按 10Hz（100ms）取数，如果补间比这个还长，
/// 读数会永远追着真实值跑，表现为"动作结束了数值还在动"。这里与取数间隔对齐。
///
/// 非有限值（NaN/Infinity）显示占位符，不参与动画。
class MgAnimatedNumber extends StatelessWidget {
  const MgAnimatedNumber({
    super.key,
    required this.value,
    this.style,
    this.textAlign = TextAlign.right,
    this.duration = const Duration(milliseconds: 100),
    this.placeholder = '—',
    this.fractionDigits,
    this.color,
    this.maxLines = 1,
  });

  final double value;

  /// 不传时按 [MgTypography.data] 的字号字重，并套上界面字体族。
  final TextStyle? style;
  final TextAlign textAlign;
  final Duration duration;
  final String placeholder;

  /// 小数位；为 null 时整数显示、有小数才保留一位。
  final int? fractionDigits;
  final Color? color;
  final int maxLines;

  String _format(double number) {
    if (!number.isFinite) return placeholder;
    if (fractionDigits != null) {
      return number.toStringAsFixed(fractionDigits!);
    }
    final rounded = number.roundToDouble();
    final isInteger = (number - rounded).abs() < 0.005;
    return isInteger ? rounded.toStringAsFixed(0) : number.toStringAsFixed(1);
  }

  @override
  Widget build(BuildContext context) {
    // 字体族在这里统一套上，调用方传什么字号字重都不会漏掉字体统一。
    final baseStyle = MgTypography.sans(style ?? MgTypography.data);
    final effectiveStyle = color == null
        ? baseStyle
        : baseStyle.copyWith(color: color);
    if (!value.isFinite) {
      return Text(
        placeholder,
        style: effectiveStyle,
        textAlign: textAlign,
        maxLines: maxLines,
        overflow: TextOverflow.ellipsis,
      );
    }
    return TweenAnimationBuilder<double>(
      tween: Tween<double>(end: value),
      duration: duration,
      curve: Curves.easeOutCubic,
      builder: (context, animated, _) => Text(
        _format(animated),
        style: effectiveStyle,
        textAlign: textAlign,
        maxLines: maxLines,
        overflow: TextOverflow.ellipsis,
      ),
    );
  }
}
