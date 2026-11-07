import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_material_theme.dart';
import '../theme/mg_typography.dart';

/// A navigation entry in the cross-platform configuration workspace.
class AppWorkspaceItem {
  const AppWorkspaceItem({
    required this.id,
    required this.label,
    required this.icon,
  });

  final String id;
  final String label;
  final IconData icon;
}

/// The M-GCS desktop shell is drawn entirely by Flutter. Only the optional
/// macOS window chrome in main.dart uses a platform plugin.
class AppWorkspaceShell extends StatefulWidget {
  const AppWorkspaceShell({
    super.key,
    required this.items,
    required this.selectedId,
    required this.onSelected,
    required this.onReturnToMap,
    required this.contentBuilder,
  });

  final List<AppWorkspaceItem> items;
  final String selectedId;
  final ValueChanged<String> onSelected;
  final VoidCallback onReturnToMap;
  final Widget Function(BuildContext context, ScrollController scrollController)
  contentBuilder;

  @override
  State<AppWorkspaceShell> createState() => _AppWorkspaceShellState();
}

class _AppWorkspaceShellState extends State<AppWorkspaceShell> {
  /// 折叠按钮宽度，用于把它和下方导航图标对齐到同一条竖轴。
  static const double _collapseButtonWidth = 26;

  /// 展开态下按钮的左偏移：与导航项结构对齐——列表内边距 12 + 图标前留白 12
  /// + 图标半径 − 按钮半径。
  static const double _collapseButtonLeft =
      12 + 12 + MgMetrics.navIconSize / 2 - _collapseButtonWidth / 2;

  final _sidebarScroll = ScrollController();
  final _contentScrolls = <String, ScrollController>{};

  /// 手动折叠侧栏。窄窗口本来就只留图标，这里让用户在任意宽度下也能收起。
  bool _collapsed = false;

  @override
  void dispose() {
    _sidebarScroll.dispose();
    for (final scroll in _contentScrolls.values) {
      scroll.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final selected = widget.items.firstWhere(
      (item) => item.id == widget.selectedId,
      orElse: () => widget.items.first,
    );
    return MgMaterialTheme(
      child: LayoutBuilder(
        builder: (context, constraints) {
          final compact = _collapsed || constraints.maxWidth < 720;
          return Material(
            color: colors.workspace,
            child: SafeArea(
              child: Row(
                children: [
                  SizedBox(
                    width: compact
                        ? MgMetrics.sidebarCompactWidth
                        : MgMetrics.sidebarWidth,
                    child: ColoredBox(
                      color: colors.sidebar,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          const SizedBox(height: 46),
                          // 折叠按钮是这里唯一的控件：展开时也把它放在与导航图标
                          // 同一条竖轴上，收起时随侧栏整体居中。
                          Row(
                            children: [
                              SizedBox(
                                width: compact ? 0 : _collapseButtonLeft,
                              ),
                              if (compact) const Spacer(),
                              _SidebarCollapseButton(
                                collapsed: compact,
                                onPressed: () =>
                                    setState(() => _collapsed = !_collapsed),
                              ),
                              if (compact) const Spacer(),
                            ],
                          ),
                          const SizedBox(height: 12),
                          Expanded(
                            child: ListView(
                              controller: _sidebarScroll,
                              padding: const EdgeInsets.symmetric(
                                horizontal: 12,
                              ),
                              children: [
                                for (final item in widget.items)
                                  _SidebarEntry(
                                    item: item,
                                    selected: item.id == selected.id,
                                    compact: compact,
                                    onPressed: () => widget.onSelected(item.id),
                                  ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  VerticalDivider(
                    width: 1,
                    thickness: 1,
                    color: colors.hairline,
                  ),
                  Expanded(
                    child: Column(
                      children: [
                        Container(
                          height: MgMetrics.toolbarHeight,
                          padding: const EdgeInsets.fromLTRB(24, 0, 12, 0),
                          decoration: BoxDecoration(
                            color: colors.toolbar,
                            border: Border(
                              bottom: BorderSide(color: colors.hairline),
                            ),
                          ),
                          child: Row(
                            children: [
                              Expanded(
                                child: Text(
                                  selected.label,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: MgTypography.sans(
                                    MgTypography.title.copyWith(
                                      color: colors.textPrimary,
                                    ),
                                  ),
                                ),
                              ),
                              Tooltip(
                                message: '返回飞行控制页面',
                                child: TextButton(
                                  onPressed: widget.onReturnToMap,
                                  style: TextButton.styleFrom(
                                    foregroundColor: colors.textSecondary,
                                    minimumSize: const Size(108, 44),
                                    padding: const EdgeInsets.symmetric(
                                      horizontal: 8,
                                    ),
                                  ),
                                  child: Column(
                                    mainAxisAlignment: MainAxisAlignment.center,
                                    children: [
                                      Icon(
                                        Icons.map_outlined,
                                        size: 18,
                                        color: colors.accent,
                                      ),
                                      const SizedBox(height: 2),
                                      Text(
                                        '返回飞行控制页面',
                                        style: MgTypography.sans(
                                          MgTypography.caption,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ),
                            ],
                          ),
                        ),
                        Expanded(
                          child: ColoredBox(
                            color: colors.toolbar,
                            child: widget.contentBuilder(
                              context,
                              _contentScrolls.putIfAbsent(
                                widget.selectedId,
                                () => ScrollController(),
                              ),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}

/// 侧栏折叠按钮：展开时指向左（收起），收起时指向右（展开）。
///
/// 位置固定在搜索栏左侧，收起后搜索栏一并让位，只留这一枚小箭头。
class _SidebarCollapseButton extends StatelessWidget {
  const _SidebarCollapseButton({
    required this.collapsed,
    required this.onPressed,
  });

  final bool collapsed;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Tooltip(
      message: collapsed ? '展开侧栏' : '折叠侧栏',
      child: Material(
        color: colors.transparent,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        child: InkWell(
          onTap: onPressed,
          borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
          child: SizedBox(
            width: _AppWorkspaceShellState._collapseButtonWidth,
            height: MgMetrics.controlHeight,
            child: Icon(
              collapsed ? Icons.chevron_right : Icons.chevron_left,
              size: 18,
              color: colors.textSecondary,
            ),
          ),
        ),
      ),
    );
  }
}

class _SidebarEntry extends StatelessWidget {
  const _SidebarEntry({
    required this.item,
    required this.selected,
    required this.compact,
    required this.onPressed,
  });

  final AppWorkspaceItem item;
  final bool selected;
  final bool compact;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final foreground = selected ? colors.onAccent : colors.textPrimary;
    return Padding(
      padding: const EdgeInsets.only(bottom: 2),
      child: Tooltip(
        message: item.label,
        child: Semantics(
          button: true,
          selected: selected,
          label: item.label,
          child: Material(
            color: selected ? colors.accent : colors.transparent,
            borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
            child: InkWell(
              onTap: onPressed,
              borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
              child: SizedBox(
                height: MgMetrics.navItemHeight,
                child: Row(
                  mainAxisAlignment: compact
                      ? MainAxisAlignment.center
                      : MainAxisAlignment.start,
                  children: [
                    if (!compact) const SizedBox(width: 12),
                    Icon(
                      item.icon,
                      size: MgMetrics.navIconSize,
                      color: foreground,
                    ),
                    if (!compact) ...[
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          item.label,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: MgTypography.sans(
                            MgTypography.body.copyWith(
                              color: foreground,
                              // 选中态已经有强调色底，不用再加粗（非标题不加粗）。
                            ),
                          ),
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
