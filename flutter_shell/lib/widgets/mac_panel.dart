import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';

/// 标签卡与内容面板之间的标准间距，标签页与浮动面板共用。
const double kMacTabStripGap = 4;

/// macos_ui 标签视图内容面板的配色、边框与圆角。
///
/// 标签页内容和浮动面板共用同一份样式，避免各页各写一套观感。
class MacPanel extends StatelessWidget {
  const MacPanel({super.key, required this.child, this.padding});

  final Widget child;
  final EdgeInsetsGeometry? padding;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    final inner = padding == null
        ? child
        : Padding(padding: padding!, child: child);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.panel,
        border: Border.all(color: colors.panelBorder),
        borderRadius: BorderRadius.circular(5),
      ),
      child: ClipRRect(borderRadius: BorderRadius.circular(5), child: inner),
    );
  }
}
