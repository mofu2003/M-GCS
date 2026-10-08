import 'package:flutter/material.dart';

/// 电池配置占位页；本次拆分不新增配置功能。
class BatteryConfigurationPage extends StatelessWidget {
  const BatteryConfigurationPage({super.key});

  @override
  Widget build(BuildContext context) => const _BlankConfigurationPage();
}

class _BlankConfigurationPage extends StatelessWidget {
  const _BlankConfigurationPage();

  @override
  Widget build(BuildContext context) => const SizedBox.expand();
}
