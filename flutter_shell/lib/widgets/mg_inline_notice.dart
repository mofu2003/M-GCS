import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';

/// 页面内的单行提示：图标 + 文本 + 可选右侧动作。
///
/// 机载日志、飞行记录、文件浏览三个页面都用它显示列表错误、动作错误和成功提示。
/// 提取成共享组件是为了避免同一类提示在多个页面里各写一份（`ui.md` 的 U5）。
class MgInlineNotice extends StatelessWidget {
  const MgInlineNotice({
    super.key,
    required this.icon,
    required this.color,
    required this.text,
    this.trailing,
  });

  final IconData icon;
  final Color color;
  final String text;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      MacosIcon(icon, size: 14, color: color),
      const SizedBox(width: 7),
      Expanded(
        child: Text(
          text,
          style: MacosTheme.of(
            context,
          ).typography.caption1.copyWith(color: color),
        ),
      ),
      ?trailing,
    ],
  );
}

/// 提示右侧的「知道了」按钮：清除一次性的错误/成功提示。
class MgDismissButton extends StatelessWidget {
  const MgDismissButton({super.key, required this.onPressed, this.label = '知道了'});

  final VoidCallback onPressed;
  final String label;

  @override
  Widget build(BuildContext context) => CupertinoButton(
    padding: EdgeInsets.zero,
    minimumSize: const Size(0, 0),
    onPressed: onPressed,
    child: Text(
      label,
      style: MacosTheme.of(
        context,
      ).typography.caption1.copyWith(color: context.mgColors.accent),
    ),
  );
}
