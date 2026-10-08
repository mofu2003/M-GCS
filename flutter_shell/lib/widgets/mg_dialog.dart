import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';

/// M-GCS 统一的对话框入场动画：只淡入，不使用 macos_ui 的弹簧缩放。
///
/// macos_ui 的 `showMacosAlertDialog` / `showMacosSheet` 入场会经过
/// `_SubtleBounceCurve`，窗口内容会先缩小再回弹。这里保留 `MacosAlertDialog`
/// 和 `MacosSheet` 的外观，只替换路由过渡，避免所有弹窗出现跳动。
const Duration _mgDialogFadeDuration = Duration(milliseconds: 120);

Future<T?> showMgAlertDialog<T>({
  required BuildContext context,
  required WidgetBuilder builder,
  bool barrierDismissible = false,
  Color? barrierColor,
  String barrierLabel = '关闭对话框',
  bool useRootNavigator = true,
  RouteSettings? routeSettings,
}) => _showMgDialog<T>(
  context: context,
  builder: builder,
  barrierDismissible: barrierDismissible,
  barrierColor: barrierColor,
  barrierLabel: barrierLabel,
  useRootNavigator: useRootNavigator,
  routeSettings: routeSettings,
);

/// M-GCS 统一的 Sheet 入场动画：与 Alert 一样只淡入。
Future<T?> showMgSheet<T>({
  required BuildContext context,
  required WidgetBuilder builder,
  bool barrierDismissible = false,
  Color? barrierColor,
  String barrierLabel = '关闭面板',
  bool useRootNavigator = true,
  RouteSettings? routeSettings,
}) => _showMgDialog<T>(
  context: context,
  builder: builder,
  barrierDismissible: barrierDismissible,
  barrierColor: barrierColor,
  barrierLabel: barrierLabel,
  useRootNavigator: useRootNavigator,
  routeSettings: routeSettings,
);

Future<T?> _showMgDialog<T>({
  required BuildContext context,
  required WidgetBuilder builder,
  required bool barrierDismissible,
  required String barrierLabel,
  Color? barrierColor,
  bool useRootNavigator = true,
  RouteSettings? routeSettings,
}) {
  return showGeneralDialog<T>(
    context: context,
    useRootNavigator: useRootNavigator,
    routeSettings: routeSettings,
    barrierDismissible: barrierDismissible,
    barrierLabel: barrierLabel,
    barrierColor: barrierColor ?? MgColors.of(context).shadow,
    transitionDuration: _mgDialogFadeDuration,
    pageBuilder: (dialogContext, animation, secondaryAnimation) => Semantics(
      scopesRoute: true,
      explicitChildNodes: true,
      child: builder(dialogContext),
    ),
    transitionBuilder: (dialogContext, animation, secondaryAnimation, child) =>
        FadeTransition(
          opacity: CurvedAnimation(parent: animation, curve: Curves.easeOut),
          child: child,
        ),
  );
}
