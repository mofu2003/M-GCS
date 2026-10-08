import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/widgets/parameter_load_progress.dart';

/// 页面顶部的数据状态条。
///
/// 没有数据时页面照常渲染组件骨架（数值显示「—」、控件置灰），只用这一条说明
/// 原因，避免出现"整页空白 + 中间几行字"。
class PageDataNotice extends StatelessWidget {
  const PageDataNotice({
    super.key,
    required this.controller,
    required this.message,
    this.showProgress = false,
  });

  final ConnectionController controller;
  final String message;

  /// 参数正在下载时在右侧显示进度条。
  final bool showProgress;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final error = controller.parameterError;
    final isError = error != null;
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        children: [
          MacosIcon(
            isError
                ? CupertinoIcons.exclamationmark_circle
                : CupertinoIcons.info_circle,
            size: 14,
            color: isError ? colors.danger : colors.textSecondary,
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Text(
              error ?? message,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: isError ? colors.danger : colors.textSecondary,
              ),
            ),
          ),
          if (showProgress && !isError) ...[
            const SizedBox(width: 16),
            SizedBox(
              width: 220,
              child: ParameterLoadProgress(
                received: controller.parameterReceived,
                total: controller.parameterTotal,
              ),
            ),
          ],
        ],
      ),
    );
  }
}
