import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import 'map/amap_map_view.dart'
    show MapStyle, mapStyleAttribution, mapStyleLabel;
import 'theme/mg_colors.dart';
import 'theme/mg_typography.dart';
import 'widgets/app_workspace_shell.dart';

/// 地面站配置。
///
/// 和飞控配置同构（左侧栏目 + 右侧内容），但配置的是**地面站本地行为**，不影响
/// 飞控。第一页是「地图配置」：把原先散在地图右下角的底图选择和数据来源说明收进来，
/// MP / QGC 里属于地面站本地的地图选项也都归到这一页。
class GcsConfigurationWorkspace extends StatefulWidget {
  const GcsConfigurationWorkspace({
    super.key,
    required this.mapStyle,
    required this.onReturnToMap,
  });

  /// 地图样式的唯一真源，地图页监听它切换图层并持久化。
  final ValueNotifier<MapStyle> mapStyle;
  final VoidCallback onReturnToMap;

  @override
  State<GcsConfigurationWorkspace> createState() =>
      _GcsConfigurationWorkspaceState();
}

class _GcsConfigurationWorkspaceState extends State<GcsConfigurationWorkspace> {
  String _selectedPageId = 'map';

  @override
  Widget build(BuildContext context) => AppWorkspaceShell(
    items: const [
      AppWorkspaceItem(id: 'map', label: '地图配置', icon: CupertinoIcons.map),
    ],
    selectedId: _selectedPageId,
    onSelected: (id) => setState(() => _selectedPageId = id),
    onReturnToMap: widget.onReturnToMap,
    contentBuilder: (context, scrollController) => _MapSettingsPage(
      scrollController: scrollController,
      mapStyle: widget.mapStyle,
    ),
  );
}

class _MapSettingsPage extends StatelessWidget {
  const _MapSettingsPage({
    required this.scrollController,
    required this.mapStyle,
  });

  final ScrollController scrollController;
  final ValueNotifier<MapStyle> mapStyle;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return ListView(
      controller: scrollController,
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          '地图配置',
          style: MgTypography.sans(
            MgTypography.title.copyWith(color: colors.textPrimary),
          ),
        ),
        const SizedBox(height: 4),
        Text(
          '这些选项只影响地面站本地显示，不会写入飞控。',
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textSecondary),
          ),
        ),
        const SizedBox(height: 18),
        _SectionCard(
          title: '底图来源',
          child: ValueListenableBuilder<MapStyle>(
            valueListenable: mapStyle,
            builder: (context, selected, _) => Column(
              children: [
                for (final style in MapStyle.values)
                  _MapStyleRow(
                    style: style,
                    selected: style == selected,
                    onSelected: () => mapStyle.value = style,
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 16),
        _SectionCard(
          title: '数据来源与署名',
          child: ValueListenableBuilder<MapStyle>(
            valueListenable: mapStyle,
            builder: (context, selected, _) {
              final attribution = mapStyleAttribution(selected);
              return Padding(
                padding: const EdgeInsets.fromLTRB(14, 4, 14, 14),
                child: Text(
                  attribution.isEmpty ? '当前底图无署名要求。' : attribution,
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(color: colors.textSecondary),
                  ),
                ),
              );
            },
          ),
        ),
      ],
    );
  }
}

/// 与飞行模式列表一致的选中表现：底色高亮 + 强调色文字，不用对勾。
class _MapStyleRow extends StatelessWidget {
  const _MapStyleRow({
    required this.style,
    required this.selected,
    required this.onSelected,
  });

  final MapStyle style;
  final bool selected;
  final VoidCallback onSelected;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Material(
      color: selected ? colors.accentSurface : Colors.transparent,
      child: InkWell(
        onTap: onSelected,
        hoverColor: colors.mapControlHover,
        child: SizedBox(
          height: MgMetrics.rowHeight,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 14),
            child: Align(
              alignment: Alignment.centerLeft,
              child: Text(
                mapStyleLabel(style),
                style: MgTypography.sans(
                  MgTypography.body.copyWith(
                    color: selected ? colors.accent : colors.textPrimary,
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _SectionCard extends StatelessWidget {
  const _SectionCard({required this.title, required this.child});

  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 14, 8),
            child: Text(
              title,
              style: MgTypography.sans(
                MgTypography.section.copyWith(color: colors.textPrimary),
              ),
            ),
          ),
          Divider(height: 1, color: colors.hairline),
          child,
        ],
      ),
    );
  }
}
