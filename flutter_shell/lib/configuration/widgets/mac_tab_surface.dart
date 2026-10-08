import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/mac_panel.dart';

/// 标签页外壳：标签卡用 macos_ui 的 [MacosSegmentedControl]，
/// 面板沿用 macos_ui 标签视图的配色与圆角。
///
/// 不用 [MacosTabView]：它在 didChangeDependencies 中重复 addListener，
/// 而 dispose 只移除一次，点击标签会给已销毁的 State 发通知。
class MacTabSurface extends StatefulWidget {
  const MacTabSurface({
    super.key,
    required this.labels,
    required this.children,
    this.controller,
  });

  final List<String> labels;
  final List<Widget> children;

  /// 外部控制器。需要知道当前选中哪一页时传入，
  /// 不传则由本组件自己创建（默认行为）。
  final MacosTabController? controller;

  @override
  State<MacTabSurface> createState() => _MacTabSurfaceState();
}

class _MacTabSurfaceState extends State<MacTabSurface> {
  late final MacosTabController _tabs =
      widget.controller ??
      MacosTabController(initialIndex: 0, length: widget.labels.length);
  late final bool _ownsTabs = widget.controller == null;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
  }

  void _handleTabChanged() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    if (_ownsTabs) _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(
      MgMetrics.configTabBodyInset,
      kMacTabStripGap,
      MgMetrics.configTabBodyInset,
      MgMetrics.configTabBodyInset,
    ),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Center(
          child: MacosSegmentedControl(
            controller: _tabs,
            tabs: [for (final label in widget.labels) MacosTab(label: label)],
          ),
        ),
        const SizedBox(height: kMacTabStripGap),
        Expanded(
          child: MacPanel(
            child: IndexedStack(index: _tabs.index, children: widget.children),
          ),
        ),
      ],
    ),
  );
}
