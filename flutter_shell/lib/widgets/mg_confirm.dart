import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import 'mg_dialog.dart';

/// 统一确认弹窗：返回用户是否确认，取消、返回或关闭一律返回 false。
///
/// 改变飞控状态或本地配置的写入入口复用同一个实现，避免各页面自造变体（U3）。
/// 飞行状态命令（S3）走 [MgCommandConfirmation] 的长按入口，本弹窗用于上传、
/// 参数/校准入队、签名密钥下发一类的写入确认。
Future<bool> showMgConfirmDialog(
  BuildContext context, {
  required String title,
  required String message,
  required String confirmLabel,
  String cancelLabel = '取消',
}) async {
  final result = await showMgAlertDialog<bool>(
    context: context,
    builder: (dialogContext) => MacosAlertDialog(
      appIcon: MacosIcon(
        CupertinoIcons.exclamationmark_triangle,
        size: 44,
        color: dialogContext.mgColors.warning,
      ),
      title: Text(title),
      message: Text(message),
      primaryButton: PushButton(
        controlSize: ControlSize.large,
        onPressed: () => Navigator.of(dialogContext).pop(true),
        child: Text(confirmLabel),
      ),
      secondaryButton: PushButton(
        controlSize: ControlSize.large,
        secondary: true,
        onPressed: () => Navigator.of(dialogContext).pop(false),
        child: Text(cancelLabel),
      ),
    ),
  );
  return result ?? false;
}
