import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

/// PID 参数区块标题。
class PidSectionHeader extends StatelessWidget {
  const PidSectionHeader({super.key, required this.title});

  final String title;

  @override
  Widget build(BuildContext context) =>
      Text(title, style: MacosTheme.of(context).typography.title3);
}
