import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';

/// 共享参数写入失败提示。
class ParameterWriteStatus extends StatelessWidget {
  const ParameterWriteStatus({super.key, required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final error = controller.parameterWriteError;
    final message = error ?? '参数写入失败';
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2, vertical: 3),
      child: Row(
        children: [
          MacosIcon(
            CupertinoIcons.exclamationmark_circle,
            size: 14,
            color: colors.danger,
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Text(
              message,
              style: MacosTheme.of(
                context,
              ).typography.caption1.copyWith(color: colors.danger),
            ),
          ),
        ],
      ),
    );
  }
}
