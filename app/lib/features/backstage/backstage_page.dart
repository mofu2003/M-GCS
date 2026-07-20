import 'package:flutter/material.dart';

import '../../application/connection_controller.dart';
import '../../application/parameter_controller.dart';
import '../../application/parameter_models.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';
import 'parameter_tree.dart';

final class BackstagePage extends StatefulWidget {
  const BackstagePage({
    required this.connectionController,
    required this.parameterController,
    required this.onReturnToFrontstage,
    super.key,
  });

  final ConnectionController connectionController;
  final ParameterController? parameterController;
  final VoidCallback onReturnToFrontstage;

  @override
  State<BackstagePage> createState() => _BackstagePageState();
}

final class _BackstagePageState extends State<BackstagePage> {
  final _searchController = TextEditingController();
  String _prefix = '';
  String? _selectedName;
  _ParameterFilter _filter = _ParameterFilter.all;

  @override
  void initState() {
    super.initState();
    _searchController.addListener(() => setState(() {}));
    widget.connectionController.addListener(_refreshWhenReady);
  }

  @override
  void dispose() {
    widget.connectionController.removeListener(_refreshWhenReady);
    _searchController.dispose();
    super.dispose();
  }

  void _refreshWhenReady() {
    final controller = widget.parameterController;
    if (controller != null &&
        controller.canEdit &&
        controller.parameters.isEmpty &&
        !controller.refreshing) {
      controller.refresh();
    }
  }

  List<FlightControllerParameter> _visibleParameters(
    ParameterController controller,
  ) {
    final query = _searchController.text.trim().toUpperCase();
    final stagedNames = controller.stagedChanges
        .map((change) => change.parameter.name)
        .toSet();
    return controller.parameters
        .where((parameter) {
          if (_prefix.isNotEmpty && !parameter.name.startsWith(_prefix)) {
            return false;
          }
          if (query.isNotEmpty &&
              !parameter.name.toUpperCase().contains(query) &&
              !(controller
                      .descriptorFor(parameter.name)
                      ?.displayName
                      .toUpperCase()
                      .contains(query) ??
                  false) &&
              !(controller
                      .descriptorFor(parameter.name)
                      ?.description
                      .toUpperCase()
                      .contains(query) ??
                  false)) {
            return false;
          }
          return switch (_filter) {
            _ParameterFilter.all => true,
            _ParameterFilter.staged => stagedNames.contains(parameter.name),
          };
        })
        .toList(growable: false);
  }

  Future<void> _reviewAndWrite(ParameterController controller) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) =>
          _ParameterWriteReviewDialog(changes: controller.stagedChanges),
    );
    if (confirmed != true || !mounted) return;
    final result = await controller.writeStaged();
    if (!mounted || !result.hasFailures) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('${result.failed.length} 项参数写入失败，已保留草稿')),
    );
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.parameterController;
    return Scaffold(
      body: DecoratedBox(
        key: const ValueKey('mg-backstage-shell'),
        decoration: const BoxDecoration(color: Color(0xffedf2f5)),
        child: controller == null
            ? _UnavailableBackstage(onReturn: widget.onReturnToFrontstage)
            : ListenableBuilder(
                listenable: controller,
                builder: (context, _) {
                  final visible = _visibleParameters(controller);
                  final selected = controller.parameters
                      .where((item) => item.name == _selectedName)
                      .firstOrNull;
                  return LayoutBuilder(
                    builder: (context, constraints) => _BackstageWorkspace(
                      // Desktop windows may be narrower than 960px. Keep the
                      // navigation tree visible until the screen is genuinely
                      // phone-sized, where a horizontal workspace cannot work.
                      compact: constraints.maxWidth < 720,
                      controller: controller,
                      visibleParameters: visible,
                      selectedParameter: selected,
                      selectedPrefix: _prefix,
                      searchController: _searchController,
                      filter: _filter,
                      onReturn: widget.onReturnToFrontstage,
                      onPrefixSelected: (prefix) => setState(() {
                        _prefix = prefix;
                        _selectedName = null;
                      }),
                      onFilterChanged: (filter) =>
                          setState(() => _filter = filter),
                      onSelected: (parameter) {
                        setState(() => _selectedName = parameter.name);
                        controller.loadDescriptor(parameter.name);
                      },
                      onReviewWrite: () => _reviewAndWrite(controller),
                    ),
                  );
                },
              ),
      ),
    );
  }
}

enum _ParameterFilter { all, staged }

final class _BackstageWorkspace extends StatelessWidget {
  const _BackstageWorkspace({
    required this.compact,
    required this.controller,
    required this.visibleParameters,
    required this.selectedParameter,
    required this.selectedPrefix,
    required this.searchController,
    required this.filter,
    required this.onReturn,
    required this.onPrefixSelected,
    required this.onFilterChanged,
    required this.onSelected,
    required this.onReviewWrite,
  });

  final bool compact;
  final ParameterController controller;
  final List<FlightControllerParameter> visibleParameters;
  final FlightControllerParameter? selectedParameter;
  final String selectedPrefix;
  final TextEditingController searchController;
  final _ParameterFilter filter;
  final VoidCallback onReturn;
  final ValueChanged<String> onPrefixSelected;
  final ValueChanged<_ParameterFilter> onFilterChanged;
  final ValueChanged<FlightControllerParameter> onSelected;
  final VoidCallback onReviewWrite;

  @override
  Widget build(BuildContext context) {
    final selected = selectedParameter;
    final tree = _ParameterTree(
      parameters: controller.parameters,
      selectedPrefix: selectedPrefix,
      onSelected: onPrefixSelected,
    );
    final list = _ParameterList(
      parameters: visibleParameters,
      selectedName: selected?.name,
      staged: controller.stagedChanges,
      searchController: searchController,
      descriptorFor: controller.descriptorFor,
      onSelected: onSelected,
    );
    final inspector = _ParameterInspector(
      parameter: selected,
      descriptor: selected == null
          ? null
          : controller.descriptorFor(selected.name),
      staged: controller.stagedChanges
          .where((change) => change.parameter.name == selected?.name)
          .firstOrNull,
      enabled: controller.canEdit,
      onStage: (value) => controller.stageValue(selected!.name, value),
    );

    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          children: [
            _BackstageHeader(
              parameterCount: controller.parameters.length,
              onRead: controller.canEdit ? controller.refresh : null,
              onReviewWrite:
                  controller.stagedChanges.isEmpty || controller.writing
                  ? null
                  : onReviewWrite,
            ),
            const SizedBox(height: 14),
            Expanded(
              child: compact
                  ? Column(
                      children: [
                        Expanded(child: list),
                        const SizedBox(height: 12),
                        SizedBox(height: 230, child: inspector),
                      ],
                    )
                  : Row(
                      children: [
                        SizedBox(
                          width: 76,
                          child: _BackstageNavigationRail(onReturn: onReturn),
                        ),
                        const SizedBox(width: 12),
                        SizedBox(width: 216, child: tree),
                        const SizedBox(width: 12),
                        Expanded(child: list),
                        const SizedBox(width: 12),
                        SizedBox(width: 306, child: inspector),
                      ],
                    ),
            ),
            if (controller.stagedChanges.isNotEmpty) ...[
              const SizedBox(height: 12),
              _PendingWriteBar(
                count: controller.stagedChanges.length,
                writing: controller.writing,
                onDiscard: controller.discardAll,
                onReviewWrite: onReviewWrite,
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _GlassPanel extends StatelessWidget {
  const _GlassPanel({required this.child});
  final Widget child;
  @override
  Widget build(BuildContext context) => ClipRRect(
    borderRadius: BorderRadius.circular(20),
    child: DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.72),
        border: Border.all(color: Colors.white.withValues(alpha: 0.92)),
        borderRadius: BorderRadius.circular(20),
        boxShadow: [MacGlassTokens.floatingShadow()],
      ),
      child: Material(type: MaterialType.transparency, child: child),
    ),
  );
}

class _BackstageNavigationRail extends StatelessWidget {
  const _BackstageNavigationRail({required this.onReturn});

  final VoidCallback onReturn;

  @override
  Widget build(BuildContext context) => _GlassPanel(
    child: Column(
      children: [
        const SizedBox(height: 12),
        Tooltip(
          message: '参数配置',
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: MacGlassTokens.black,
              borderRadius: BorderRadius.circular(14),
            ),
            child: const SizedBox(
              width: 44,
              height: 44,
              child: Icon(Icons.tune_rounded, color: Colors.white),
            ),
          ),
        ),
        const Spacer(),
        IconButton(
          key: const ValueKey('mg-backstage-return-frontstage'),
          tooltip: '返回前台',
          onPressed: onReturn,
          icon: const Icon(Icons.map_outlined),
        ),
        const SizedBox(height: 8),
      ],
    ),
  );
}

class _BackstageHeader extends StatelessWidget {
  const _BackstageHeader({
    required this.parameterCount,
    required this.onRead,
    required this.onReviewWrite,
  });
  final int parameterCount;
  final VoidCallback? onRead;
  final VoidCallback? onReviewWrite;
  @override
  Widget build(BuildContext context) => _GlassPanel(
    child: SizedBox(
      height: 58,
      child: Row(
        children: [
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('工程配置', style: Theme.of(context).textTheme.titleMedium),
                Text(
                  '$parameterCount 个飞控参数',
                  style: const TextStyle(
                    color: MacGlassTokens.muted,
                    fontSize: 12,
                  ),
                ),
              ],
            ),
          ),
          _CommandButton(
            icon: Icons.download_rounded,
            label: '读取',
            onPressed: onRead,
          ),
          const SizedBox(width: 6),
          _CommandButton(
            icon: Icons.save_alt_rounded,
            label: '保存',
            onPressed: null,
          ),
          const SizedBox(width: 6),
          _CommandButton(
            icon: Icons.upload_rounded,
            label: '写入',
            onPressed: onReviewWrite,
          ),
        ],
      ),
    ),
  );
}

class _CommandButton extends StatelessWidget {
  const _CommandButton({
    required this.icon,
    required this.label,
    required this.onPressed,
  });

  final IconData icon;
  final String label;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) => OutlinedButton.icon(
    onPressed: onPressed,
    icon: Icon(icon, size: 16),
    label: Text(label),
  );
}

class _ParameterTree extends StatelessWidget {
  const _ParameterTree({
    required this.parameters,
    required this.selectedPrefix,
    required this.onSelected,
  });
  final List<FlightControllerParameter> parameters;
  final String selectedPrefix;
  final ValueChanged<String> onSelected;
  @override
  Widget build(BuildContext context) {
    final root = ParameterTreeNode.build(
      parameters.map((parameter) => parameter.name),
    );
    return _GlassPanel(
      child: ListView(
        children: [
          const Padding(
            padding: EdgeInsets.fromLTRB(16, 16, 16, 8),
            child: Text('参数树', style: TextStyle(fontWeight: FontWeight.w800)),
          ),
          _TreeItem(
            node: root,
            selectedPrefix: selectedPrefix,
            onSelected: onSelected,
          ),
        ],
      ),
    );
  }
}

class _TreeItem extends StatelessWidget {
  const _TreeItem({
    required this.node,
    required this.selectedPrefix,
    required this.onSelected,
  });
  final ParameterTreeNode node;
  final String selectedPrefix;
  final ValueChanged<String> onSelected;
  @override
  Widget build(BuildContext context) {
    final active = selectedPrefix == node.prefix;
    if (node.children.isEmpty) {
      return ListTile(
        dense: true,
        selected: active,
        selectedTileColor: MacGlassTokens.blue.withValues(alpha: 0.13),
        leading: const Icon(Icons.tune_rounded, size: 17),
        title: Text(node.label, maxLines: 1, overflow: TextOverflow.ellipsis),
        onTap: () => onSelected(node.prefix),
      );
    }
    return ExpansionTile(
      initiallyExpanded:
          node.prefix.isEmpty || selectedPrefix.startsWith(node.prefix),
      leading: Icon(
        node.prefix.isEmpty
            ? Icons.folder_open_outlined
            : Icons.folder_outlined,
        size: 18,
      ),
      title: Text(node.label, maxLines: 1, overflow: TextOverflow.ellipsis),
      children: [
        ListTile(
          dense: true,
          selected: active,
          selectedTileColor: MacGlassTokens.blue.withValues(alpha: 0.13),
          title: Text(node.prefix.isEmpty ? '全部参数' : '本分类全部参数'),
          onTap: () => onSelected(node.prefix),
        ),
        for (final child in node.children)
          Padding(
            padding: const EdgeInsets.only(left: 12),
            child: _TreeItem(
              node: child,
              selectedPrefix: selectedPrefix,
              onSelected: onSelected,
            ),
          ),
      ],
    );
  }
}

class _ParameterList extends StatelessWidget {
  const _ParameterList({
    required this.parameters,
    required this.selectedName,
    required this.staged,
    required this.searchController,
    required this.descriptorFor,
    required this.onSelected,
  });
  final List<FlightControllerParameter> parameters;
  final String? selectedName;
  final List<StagedParameterChange> staged;
  final TextEditingController searchController;
  final ParameterDescriptor? Function(String name) descriptorFor;
  final ValueChanged<FlightControllerParameter> onSelected;
  @override
  Widget build(BuildContext context) {
    final stagedNames = staged.map((item) => item.parameter.name).toSet();
    return _GlassPanel(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: TextField(
              controller: searchController,
              decoration: const InputDecoration(
                hintText: '查询参数、中文名称或描述',
                prefixIcon: Icon(Icons.search_rounded),
              ),
            ),
          ),
          Expanded(
            child: ListView.builder(
              key: const ValueKey('mg-parameter-list'),
              itemCount: parameters.length,
              itemBuilder: (context, index) {
                final parameter = parameters[index];
                final isStaged = stagedNames.contains(parameter.name);
                final descriptor = descriptorFor(parameter.name);
                final displayName = _displayNameFor(parameter, descriptor);
                return ListTile(
                  dense: true,
                  minVerticalPadding: 7,
                  selected: selectedName == parameter.name,
                  title: Text(
                    parameter.name,
                    style: const TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 13,
                      fontWeight: FontWeight.w700,
                      fontFeatures: [FontFeature.tabularFigures()],
                    ),
                  ),
                  subtitle: displayName == null
                      ? null
                      : Padding(
                          padding: const EdgeInsets.only(top: 2),
                          child: Text(
                            displayName,
                            maxLines: 1,
                            softWrap: false,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                              color: MacGlassTokens.blue,
                              fontSize: 11,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                  trailing: Text(
                    (isStaged
                            ? staged
                                  .firstWhere(
                                    (item) =>
                                        item.parameter.name == parameter.name,
                                  )
                                  .stagedValue
                            : parameter.value)
                        .toString(),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 12,
                      fontFeatures: [FontFeature.tabularFigures()],
                    ),
                  ),
                  onTap: () => onSelected(parameter),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _ParameterInspector extends StatelessWidget {
  const _ParameterInspector({
    required this.parameter,
    required this.descriptor,
    required this.staged,
    required this.enabled,
    required this.onStage,
  });
  final FlightControllerParameter? parameter;
  final ParameterDescriptor? descriptor;
  final StagedParameterChange? staged;
  final bool enabled;
  final ValueChanged<double> onStage;
  @override
  Widget build(BuildContext context) {
    final selected = parameter;
    if (selected == null) {
      return _GlassPanel(
        child: const Center(
          child: Text(
            '选择一个参数查看详情',
            style: TextStyle(color: MacGlassTokens.muted),
          ),
        ),
      );
    }
    return _GlassPanel(
      child: Padding(
        key: const ValueKey('mg-parameter-inspector'),
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              selected.name,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(
                fontFamily: 'monospace',
                fontSize: 14,
                fontWeight: FontWeight.w700,
              ),
            ),
            if (_displayNameFor(selected, descriptor)
                case final displayName?) ...[
              const SizedBox(height: 4),
              Text(
                displayName,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  color: MacGlassTokens.blue,
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
            const Divider(height: 24),
            Text('当前值 ${selected.value}', maxLines: 1),
            const SizedBox(height: 18),
            TextFormField(
              initialValue: (staged?.stagedValue ?? selected.value).toString(),
              enabled: enabled,
              keyboardType: const TextInputType.numberWithOptions(
                decimal: true,
                signed: true,
              ),
              decoration: const InputDecoration(labelText: '新值'),
              onFieldSubmitted: (text) {
                final value = double.tryParse(text);
                if (value != null) onStage(value);
              },
            ),
            if (staged?.errorMessage case final error?)
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(
                  error,
                  style: const TextStyle(color: MacGlassTokens.red),
                ),
              ),
            const Divider(height: 32),
            const Text(
              '参数说明',
              style: TextStyle(
                color: MacGlassTokens.muted,
                fontSize: 10,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 4),
            if (_descriptionFor(selected, descriptor)
                case final description?)
              Expanded(
                child: Scrollbar(
                  child: SingleChildScrollView(
                    child: SelectableText(
                      description,
                      style: const TextStyle(
                        color: MacGlassTokens.muted,
                        fontSize: 12,
                        height: 1.45,
                      ),
                    ),
                  ),
                ),
              )
            else
              const Expanded(
                child: Center(
                  child: Text(
                    '暂无中文说明',
                    style: TextStyle(color: MacGlassTokens.muted, fontSize: 12),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}

String? _displayNameFor(
  FlightControllerParameter parameter,
  ParameterDescriptor? descriptor,
) {
  final displayName = descriptor?.displayName.trim() ?? '';
  return displayName.isEmpty || displayName == parameter.name
      ? null
      : displayName;
}

String? _descriptionFor(
  FlightControllerParameter parameter,
  ParameterDescriptor? descriptor,
) {
  final description = descriptor?.description.trim() ?? '';
  return description.isEmpty || description == parameter.name
      ? null
      : description;
}

class _PendingWriteBar extends StatelessWidget {
  const _PendingWriteBar({
    required this.count,
    required this.writing,
    required this.onDiscard,
    required this.onReviewWrite,
  });
  final int count;
  final bool writing;
  final VoidCallback onDiscard;
  final VoidCallback onReviewWrite;
  @override
  Widget build(BuildContext context) => _GlassPanel(
    child: SizedBox(
      height: 58,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16),
        child: Row(
          children: [
            Text(
              '待写入 $count 项参数',
              style: const TextStyle(fontWeight: FontWeight.w800),
            ),
            const Spacer(),
            TextButton(
              onPressed: writing ? null : onDiscard,
              child: const Text('丢弃'),
            ),
            const SizedBox(width: 8),
            FilledButton(
              key: const ValueKey('mg-parameter-review-write'),
              onPressed: writing ? null : onReviewWrite,
              child: Text(writing ? '正在写入' : '复核并写入'),
            ),
          ],
        ),
      ),
    ),
  );
}

class _ParameterWriteReviewDialog extends StatelessWidget {
  const _ParameterWriteReviewDialog({required this.changes});
  final List<StagedParameterChange> changes;
  @override
  Widget build(BuildContext context) => Dialog(
    backgroundColor: Colors.transparent,
    insetPadding: const EdgeInsets.all(24),
    child: ClipRRect(
      borderRadius: BorderRadius.circular(24),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.82),
            border: Border.all(color: Colors.white.withValues(alpha: 0.94)),
            borderRadius: BorderRadius.circular(24),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 520, maxHeight: 540),
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const Text(
                    '确认参数变更',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.w800),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    '将写入 ${changes.length} 项参数',
                    style: const TextStyle(color: MacGlassTokens.muted),
                  ),
                  const SizedBox(height: 16),
                  Expanded(
                    child: ListView.separated(
                      itemCount: changes.length,
                      separatorBuilder: (_, _) => const Divider(height: 1),
                      itemBuilder: (context, index) {
                        final change = changes[index];
                        return ListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(change.parameter.name),
                          subtitle: Text(
                            '${change.parameter.value} -> ${change.stagedValue}',
                          ),
                        );
                      },
                    ),
                  ),
                  const SizedBox(height: 16),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.end,
                    children: [
                      TextButton(
                        onPressed: () => Navigator.pop(context, false),
                        child: const Text('取消'),
                      ),
                      const SizedBox(width: 8),
                      FilledButton(
                        key: const ValueKey('mg-parameter-confirm-write'),
                        onPressed: () => Navigator.pop(context, true),
                        child: const Text('确认写入'),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    ),
  );
}

class _UnavailableBackstage extends StatelessWidget {
  const _UnavailableBackstage({required this.onReturn});
  final VoidCallback onReturn;
  @override
  Widget build(BuildContext context) => Center(
    child: _GlassPanel(
      child: SizedBox(
        width: 320,
        height: 180,
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.link_off_rounded, color: MacGlassTokens.muted),
            const SizedBox(height: 12),
            const Text('参数服务不可用'),
            const SizedBox(height: 12),
            TextButton(onPressed: onReturn, child: const Text('返回前台')),
          ],
        ),
      ),
    ),
  );
}
