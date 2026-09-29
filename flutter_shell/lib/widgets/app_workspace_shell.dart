import 'dart:io';

import 'package:flutter/material.dart';

import '../theme/mg_colors.dart';
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
  final _search = TextEditingController();
  final _sidebarScroll = ScrollController();
  final _contentScroll = ScrollController();
  String _query = '';

  @override
  void initState() {
    super.initState();
    _search.addListener(() => setState(() => _query = _search.text.trim()));
  }

  @override
  void dispose() {
    _search.dispose();
    _sidebarScroll.dispose();
    _contentScroll.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final selected = widget.items.firstWhere(
      (item) => item.id == widget.selectedId,
      orElse: () => widget.items.first,
    );
    final shown = widget.items.where((item) => item.label.contains(_query));

    return LayoutBuilder(
      builder: (context, constraints) {
        final compact = constraints.maxWidth < 720;
        final topInset = Platform.isMacOS ? 28.0 : 0.0;
        return Material(
          color: colors.workspace,
          child: SafeArea(
            top: !Platform.isMacOS,
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
                        SizedBox(height: topInset + 50),
                        if (compact)
                          const Padding(
                            padding: EdgeInsets.only(bottom: 12),
                            child: Icon(Icons.search, size: 18),
                          )
                        else
                          Padding(
                            padding: const EdgeInsets.fromLTRB(12, 0, 12, 18),
                            child: SizedBox(
                              height: MgMetrics.controlHeight,
                              child: TextField(
                                controller: _search,
                                style: MgTypography.sans(
                                  MgTypography.body.copyWith(
                                    color: colors.textPrimary,
                                  ),
                                ),
                                decoration: InputDecoration(
                                  hintText: '搜索配置',
                                  hintStyle: MgTypography.sans(
                                    MgTypography.body.copyWith(
                                      color: colors.textSecondary,
                                    ),
                                  ),
                                  prefixIcon: Icon(
                                    Icons.search,
                                    size: 18,
                                    color: colors.textSecondary,
                                  ),
                                  prefixIconConstraints: const BoxConstraints(
                                    minWidth: 34,
                                  ),
                                  filled: true,
                                  fillColor: colors.surface,
                                  isDense: true,
                                  contentPadding: const EdgeInsets.symmetric(
                                    vertical: 8,
                                  ),
                                  border: OutlineInputBorder(
                                    borderRadius: BorderRadius.circular(8),
                                    borderSide: BorderSide.none,
                                  ),
                                ),
                              ),
                            ),
                          ),
                        Expanded(
                          child: ListView(
                            controller: _sidebarScroll,
                            padding: const EdgeInsets.symmetric(horizontal: 12),
                            children: [
                              for (final item in shown)
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
                VerticalDivider(width: 1, thickness: 1, color: colors.hairline),
                Expanded(
                  child: Column(
                    children: [
                      Container(
                        height: topInset + MgMetrics.toolbarHeight,
                        padding: EdgeInsets.fromLTRB(24, topInset, 12, 0),
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
                              message: '返回前台地图',
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
                                      '返回前台地图',
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
                          child: widget.contentBuilder(context, _contentScroll),
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
                              fontWeight: selected
                                  ? FontWeight.w600
                                  : FontWeight.w400,
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
