import 'package:flutter/widgets.dart';
import 'package:macos_ui/macos_ui.dart';

/// 桌面表单的单行网格：标签和控件共用同一条垂直中心线。
class AlignedFieldRow extends StatelessWidget {
  const AlignedFieldRow({
    super.key,
    required this.label,
    required this.child,
    this.labelWidth = 64,
    this.controlHeight = 30,
    this.gap = 10,
  });

  final String label;
  final Widget child;
  final double labelWidth;
  final double controlHeight;
  final double gap;

  @override
  Widget build(BuildContext context) => Row(
    crossAxisAlignment: CrossAxisAlignment.center,
    children: [
      SizedBox(
        width: labelWidth,
        child: Text(
          label,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MacosTheme.of(context).typography.body,
        ),
      ),
      SizedBox(width: gap),
      Expanded(
        child: SizedBox(
          height: controlHeight,
          child: Center(child: child),
        ),
      ),
    ],
  );
}
