import 'package:flutter/material.dart';
import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../widgets/page_data_notice.dart';
import '../widgets/parameter_summary_row.dart';

/// 全部参数搜索与分组列表，沿用工作区滚动控制器。
class AllParametersPage extends StatefulWidget {
  const AllParametersPage({
    super.key,
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<AllParametersPage> createState() => _AllParametersPageState();
}

class _AllParametersPageState extends State<AllParametersPage> {
  String _query = '';
  final _search = TextEditingController();
  final _searchTerms = <String, String>{};
  final _indexedGroups = <String, List<VehicleParameter>>{};
  bool _indexDirty = true;
  bool _restored = false;

  @override
  void initState() {
    super.initState();
    widget.controller.parameterChanges.addListener(_invalidateIndex);
  }

  void _invalidateIndex() => _indexDirty = true;

  @override
  void didUpdateWidget(covariant AllParametersPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.parameterChanges.removeListener(_invalidateIndex);
      widget.controller.parameterChanges.addListener(_invalidateIndex);
      _indexDirty = true;
    }
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_restored) return;
    _restored = true;
    final saved = PageStorage.maybeOf(
      context,
    )?.readState(context, identifier: 'parameter-search');
    if (saved is Map<String, Object?>) {
      _query = saved['query'] as String? ?? '';
      _search.text = _query;
      _expandedGroups.addAll(Map<String, bool>.from(saved['groups'] as Map));
    }
  }

  void _saveViewState() => PageStorage.maybeOf(context)?.writeState(context, {
    'query': _query,
    'groups': Map<String, bool>.of(_expandedGroups),
  }, identifier: 'parameter-search');

  void _ensureIndex() {
    if (!_indexDirty) return;
    _searchTerms.clear();
    final grouped = <String, List<VehicleParameter>>{};
    for (final parameter in widget.controller.parameters.values) {
      _searchTerms[parameter.name] = [
        parameter.name,
        parameter.displayName ?? '',
        parameter.description ?? '',
        parameter.group ?? '',
      ].join(' ').toLowerCase();
      grouped.putIfAbsent(_parameterGroup(parameter), () => []).add(parameter);
    }
    _indexedGroups.clear();
    final names = grouped.keys.toList()..sort();
    for (final name in names) {
      _indexedGroups[name] = grouped[name]!
        ..sort((a, b) => a.name.compareTo(b.name));
    }
    _indexDirty = false;
  }

  @override
  void dispose() {
    widget.controller.parameterChanges.removeListener(_invalidateIndex);
    _search.dispose();
    super.dispose();
  }

  final Map<String, bool> _expandedGroups = {};

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.parameterChanges,
    builder: (context, _) {
      final controller = widget.controller;
      // 没数据时也保留搜索框与分组骨架，只用状态条说明原因。
      final notice = !controller.isConnected
          ? '连接飞控后读取参数'
          : !controller.parametersLoaded
          ? '正在读取飞控参数…'
          : controller.parameters.isEmpty
          ? '飞控没有返回参数'
          : null;

      final query = _query.trim().toLowerCase();
      _ensureIndex();
      final grouped = <String, List<VehicleParameter>>{
        for (final group in _indexedGroups.entries)
          group.key: query.isEmpty
              ? group.value
              : group.value
                    .where(
                      (parameter) =>
                          _searchTerms[parameter.name]!.contains(query),
                    )
                    .toList(),
      }..removeWhere((_, values) => values.isEmpty);
      final groups = grouped.keys;
      final entries = <_ParameterTreeEntry>[];
      for (final group in groups) {
        entries.add(_ParameterTreeEntry.group(group, grouped[group]!.length));
        final isExpanded = _expandedGroups[group] ?? query.isNotEmpty;
        if (isExpanded) {
          entries.addAll(grouped[group]!.map(_ParameterTreeEntry.parameter));
        }
      }

      return ListView.builder(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(
          MgMetrics.configPageMargin,
          18,
          MgMetrics.configPageMargin,
          32,
        ),
        itemCount: entries.length + 1,
        itemBuilder: (context, index) {
          if (index == 0) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                CupertinoSearchTextField(
                  controller: _search,
                  placeholder: '搜索参数名称或说明',
                  onChanged: (value) => setState(() {
                    _query = value;
                    _expandedGroups.clear();
                    _saveViewState();
                  }),
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(
                      color: context.mgColors.textPrimary,
                    ),
                  ),
                ),
                const SizedBox(height: 12),
                if (notice != null)
                  PageDataNotice(
                    controller: controller,
                    message: notice,
                    showProgress:
                        controller.isConnected && !controller.parametersLoaded,
                  )
                else if (entries.isEmpty)
                  Text(
                    '没有找到匹配的参数',
                    style: MacosTheme.of(context).typography.caption1.copyWith(
                      color: context.mgColors.textSecondary,
                    ),
                  ),
                const SizedBox(height: 4),
              ],
            );
          }
          final entry = entries[index - 1];
          if (entry.parameter == null) {
            final group = entry.group!;
            final isExpanded = _expandedGroups[group] ?? query.isNotEmpty;
            void toggleGroup() {
              setState(() {
                _expandedGroups[group] = !isExpanded;
                _saveViewState();
              });
            }

            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Padding(
                  padding: const EdgeInsets.only(top: 8, bottom: 2),
                  child: Semantics(
                    button: true,
                    expanded: isExpanded,
                    label: '$group，${entry.parameterCount} 个参数',
                    child: Material(
                      color: context.mgColors.transparent,
                      child: InkWell(
                        onTap: toggleGroup,
                        borderRadius: BorderRadius.circular(
                          MgMetrics.controlRadius,
                        ),
                        child: SizedBox(
                          height: 34,
                          child: Row(
                            children: [
                              MacosDisclosureButton(
                                isPressed: isExpanded,
                                semanticLabel: isExpanded
                                    ? '收起$group'
                                    : '展开$group',
                                onPressed: toggleGroup,
                              ),
                              const SizedBox(width: 7),
                              Expanded(
                                child: Text(
                                  group,
                                  style: MacosTheme.of(
                                    context,
                                  ).typography.callout,
                                ),
                              ),
                              Text(
                                '${entry.parameterCount}',
                                style: MacosTheme.of(context)
                                    .typography
                                    .caption1
                                    .copyWith(
                                      color: context.mgColors.textSecondary,
                                    ),
                              ),
                              const SizedBox(width: 8),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
                // 分组标题下也画横线，展开后首行参数与标题不会糊在一起。
                Divider(
                  height: 1,
                  thickness: 1,
                  color: context.mgColors.divider,
                ),
              ],
            );
          }
          final parameter = entry.parameter!;
          return ParameterSummaryRow(
            parameter: parameter,
            controller: controller,
          );
        },
      );
    },
  );

  String _parameterGroup(VehicleParameter parameter) {
    final descriptorGroup = parameter.group;
    if (descriptorGroup != null &&
        descriptorGroup.isNotEmpty &&
        descriptorGroup != 'Standard' &&
        descriptorGroup != 'Advanced') {
      return descriptorGroup;
    }
    final prefix = parameter.name.split('_').first;
    return switch (prefix) {
      'SERIAL' => '串口配置',
      'SERVO' || 'MOT' => '输出与执行器',
      'RC' || 'FLTMODE' => '遥控与飞行模式',
      'ATC' || 'PSC' || 'INS' => '飞行控制',
      _ => prefix,
    };
  }
}

class _ParameterTreeEntry {
  const _ParameterTreeEntry.group(this.group, this.parameterCount)
    : parameter = null;
  const _ParameterTreeEntry.parameter(VehicleParameter this.parameter)
    : group = null,
      parameterCount = 0;

  final String? group;
  final VehicleParameter? parameter;
  final int parameterCount;
}
