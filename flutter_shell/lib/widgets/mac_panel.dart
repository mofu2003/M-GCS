import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

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
    final brightness = MacosTheme.brightnessOf(context);
    final inner = padding == null
        ? child
        : Padding(padding: padding!, child: child);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: brightness.resolve(
          const Color(0xFFE6E9EA),
          const Color(0xFF2B2E33),
        ),
        border: Border.all(
          color: brightness.resolve(
            const Color(0xFFE1E2E4),
            const Color(0xFF3E4045),
          ),
        ),
        borderRadius: BorderRadius.circular(5),
      ),
      child: ClipRRect(borderRadius: BorderRadius.circular(5), child: inner),
    );
  }
}
