part of 'backstage_page.dart';

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
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _TreeRootRow(
            node: root,
            selectedPrefix: selectedPrefix,
            onSelected: onSelected,
            parameterCount: parameters.length,
          ),
          Divider(
            height: 1,
            thickness: 1,
            color: MacGlassTokens.ink.withValues(alpha: 0.08),
          ),
          Expanded(
            child: ListView(
              padding: const EdgeInsets.fromLTRB(6, 4, 6, 12),
              children: [
                for (final child in root.children)
                  _TreeItem(
                    node: child,
                    selectedPrefix: selectedPrefix,
                    onSelected: onSelected,
                    depth: 1,
                  ),
              ],
            ),
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
    required this.depth,
  });
  final ParameterTreeNode node;
  final String selectedPrefix;
  final ValueChanged<String> onSelected;
  final int depth;

  @override
  Widget build(BuildContext context) {
    final active = selectedPrefix == node.prefix;
    if (node.children.isEmpty) {
      return _TreeLeafRow(
        node: node,
        selected: active,
        depth: depth,
        onSelected: onSelected,
      );
    }
    return _TreeBranch(
      key: ValueKey('mg-parameter-tree-branch-${node.prefix}'),
      node: node,
      selectedPrefix: selectedPrefix,
      onSelected: onSelected,
      depth: depth,
    );
  }
}

class _TreeRootRow extends StatelessWidget {
  const _TreeRootRow({
    required this.node,
    required this.selectedPrefix,
    required this.onSelected,
    required this.parameterCount,
  });

  final ParameterTreeNode node;
  final String selectedPrefix;
  final ValueChanged<String> onSelected;
  final int parameterCount;

  @override
  Widget build(BuildContext context) {
    final active = selectedPrefix == node.prefix;
    return Padding(
      padding: const EdgeInsets.fromLTRB(8, 7, 8, 5),
      child: Material(
        color: active
            ? MacGlassTokens.blue.withValues(alpha: 0.14)
            : MacGlassTokens.ink.withValues(alpha: 0.045),
        borderRadius: BorderRadius.circular(8),
        child: InkWell(
          borderRadius: BorderRadius.circular(8),
          onTap: () => onSelected(node.prefix),
          child: SizedBox(
            height: 38,
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 10),
              child: Row(
                children: [
                  Icon(
                    Icons.folder_open_outlined,
                    size: 17,
                    color: active ? MacGlassTokens.blue : MacGlassTokens.muted,
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      node.label,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: TextStyle(
                        color: active
                            ? MacGlassTokens.blue
                            : MacGlassTokens.ink,
                        fontSize: 12.5,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                  Text(
                    '$parameterCount',
                    style: const TextStyle(
                      color: MacGlassTokens.muted,
                      fontFamily: 'monospace',
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _TreeBranch extends StatefulWidget {
  const _TreeBranch({
    required this.node,
    required this.selectedPrefix,
    required this.onSelected,
    required this.depth,
    super.key,
  });

  final ParameterTreeNode node;
  final String selectedPrefix;
  final ValueChanged<String> onSelected;
  final int depth;

  @override
  State<_TreeBranch> createState() => _TreeBranchState();
}

class _TreeBranchState extends State<_TreeBranch> {
  late bool _expanded;

  @override
  void initState() {
    super.initState();
    _expanded = widget.selectedPrefix.startsWith(widget.node.prefix);
  }

  @override
  void didUpdateWidget(covariant _TreeBranch oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.selectedPrefix != widget.selectedPrefix &&
        widget.selectedPrefix.startsWith(widget.node.prefix)) {
      _expanded = true;
    }
  }

  @override
  Widget build(BuildContext context) {
    final selected = widget.selectedPrefix == widget.node.prefix;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        _TreeBranchRow(
          node: widget.node,
          selected: selected,
          expanded: _expanded,
          depth: widget.depth,
          onToggle: () => setState(() => _expanded = !_expanded),
          onSelected: widget.onSelected,
        ),
        ClipRect(
          child: AnimatedSize(
            duration: const Duration(milliseconds: 150),
            curve: Curves.easeOutCubic,
            alignment: Alignment.topCenter,
            child: _expanded
                ? Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      for (final child in widget.node.children)
                        _TreeItem(
                          node: child,
                          selectedPrefix: widget.selectedPrefix,
                          onSelected: widget.onSelected,
                          depth: widget.depth + 1,
                        ),
                    ],
                  )
                : const SizedBox.shrink(),
          ),
        ),
      ],
    );
  }
}

class _TreeBranchRow extends StatelessWidget {
  const _TreeBranchRow({
    required this.node,
    required this.selected,
    required this.expanded,
    required this.depth,
    required this.onToggle,
    required this.onSelected,
  });

  final ParameterTreeNode node;
  final bool selected;
  final bool expanded;
  final int depth;
  final VoidCallback onToggle;
  final ValueChanged<String> onSelected;

  @override
  Widget build(BuildContext context) {
    final color = selected ? MacGlassTokens.blue : MacGlassTokens.ink;
    return Padding(
      padding: const EdgeInsets.fromLTRB(4, 2, 4, 1),
      child: Material(
        color: selected
            ? MacGlassTokens.blue.withValues(alpha: 0.14)
            : Colors.transparent,
        borderRadius: BorderRadius.circular(8),
        child: SizedBox(
          height: 36,
          child: Row(
            children: [
              _TreeIndentGuides(depth: depth, height: 36),
              SizedBox(
                width: 30,
                height: 30,
                child: IconButton(
                  key: ValueKey('mg-parameter-tree-toggle-${node.prefix}'),
                  tooltip: expanded ? '收起 ${node.label}' : '展开 ${node.label}',
                  padding: EdgeInsets.zero,
                  constraints: const BoxConstraints.tightFor(
                    width: 30,
                    height: 30,
                  ),
                  onPressed: onToggle,
                  icon: AnimatedRotation(
                    turns: expanded ? 0.25 : 0,
                    duration: const Duration(milliseconds: 120),
                    child: Icon(
                      Icons.chevron_right_rounded,
                      size: 18,
                      color: color.withValues(alpha: 0.72),
                    ),
                  ),
                ),
              ),
              Expanded(
                child: InkWell(
                  key: ValueKey('mg-parameter-tree-select-${node.prefix}'),
                  borderRadius: BorderRadius.circular(8),
                  onTap: () => onSelected(node.prefix),
                  child: Padding(
                    padding: const EdgeInsets.only(right: 10),
                    child: Row(
                      children: [
                        Icon(
                          Icons.folder_outlined,
                          size: depth == 1 ? 17 : 15,
                          color: color,
                        ),
                        const SizedBox(width: 7),
                        Expanded(
                          child: Text(
                            node.label,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: _treeTitleStyle(depth, active: selected),
                          ),
                        ),
                        Text(
                          '${_parameterCountForNode(node)}',
                          style: TextStyle(
                            color: selected
                                ? MacGlassTokens.blue
                                : MacGlassTokens.muted,
                            fontFamily: 'monospace',
                            fontSize: 10.5,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _TreeLeafRow extends StatelessWidget {
  const _TreeLeafRow({
    required this.node,
    required this.selected,
    required this.depth,
    required this.onSelected,
  });

  final ParameterTreeNode node;
  final bool selected;
  final int depth;
  final ValueChanged<String> onSelected;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(4, 0, 4, 1),
    child: Material(
      color: selected
          ? MacGlassTokens.blue.withValues(alpha: 0.13)
          : Colors.transparent,
      borderRadius: BorderRadius.circular(8),
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: () => onSelected(node.prefix),
        child: SizedBox(
          height: 34,
          child: Row(
            children: [
              _TreeIndentGuides(depth: depth, height: 34),
              const SizedBox(width: 30),
              Icon(
                Icons.tune_rounded,
                size: 14,
                color: selected ? MacGlassTokens.blue : MacGlassTokens.muted,
              ),
              const SizedBox(width: 7),
              Expanded(
                child: Text(
                  key: ValueKey('mg-parameter-tree-leaf-${node.prefix}'),
                  node.label,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    color: selected ? MacGlassTokens.blue : MacGlassTokens.ink,
                    fontFamily: 'monospace',
                    fontSize: 11.5,
                    fontWeight: selected ? FontWeight.w800 : FontWeight.w600,
                  ),
                ),
              ),
              const SizedBox(width: 10),
            ],
          ),
        ),
      ),
    ),
  );
}

class _TreeIndentGuides extends StatelessWidget {
  const _TreeIndentGuides({required this.depth, required this.height});

  final int depth;
  final double height;

  @override
  Widget build(BuildContext context) {
    final guideCount = depth <= 1 ? 0 : depth - 1;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        for (var index = 0; index < guideCount; index++)
          SizedBox(
            width: 12,
            child: Center(
              child: Container(
                width: 1,
                height: height,
                color: MacGlassTokens.blue.withValues(alpha: 0.16),
              ),
            ),
          ),
      ],
    );
  }
}

TextStyle _treeTitleStyle(int depth, {required bool active}) {
  final color = active ? MacGlassTokens.blue : MacGlassTokens.ink;
  return TextStyle(color: color, fontSize: 13, fontWeight: FontWeight.w700);
}

int _parameterCountForNode(ParameterTreeNode node) {
  if (node.isLeaf) return 1;
  return node.children.fold<int>(
    0,
    (count, child) => count + _parameterCountForNode(child),
  );
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
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 12),
            child: TextField(
              controller: searchController,
              decoration: const InputDecoration(
                hintText: '查询参数、中文名称或描述',
                prefixIcon: Icon(Icons.search_rounded),
              ),
            ),
          ),
          const _ParameterListHeader(),
          Divider(
            height: 1,
            color: MacGlassTokens.ink.withValues(alpha: 0.075),
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
                final selected = selectedName == parameter.name;
                final value = isStaged
                    ? staged
                          .firstWhere(
                            (item) => item.parameter.name == parameter.name,
                          )
                          .stagedValue
                    : parameter.value;
                return _ParameterListRow(
                  parameter: parameter,
                  displayName: displayName,
                  value: _parameterValueLabel(value, descriptor),
                  selected: selected,
                  staged: isStaged,
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

class _ParameterListHeader extends StatelessWidget {
  const _ParameterListHeader();

  @override
  Widget build(BuildContext context) => const SizedBox(
    height: 30,
    child: Padding(
      padding: EdgeInsets.symmetric(horizontal: 16),
      child: Row(
        children: [
          Expanded(flex: 11, child: _ParameterColumnLabel('参数')),
          Expanded(flex: 10, child: _ParameterColumnLabel('名称')),
          SizedBox(width: 136, child: _ParameterColumnLabel('当前值', end: true)),
        ],
      ),
    ),
  );
}

class _ParameterColumnLabel extends StatelessWidget {
  const _ParameterColumnLabel(this.label, {this.end = false});

  final String label;
  final bool end;

  @override
  Widget build(BuildContext context) => Text(
    label,
    textAlign: end ? TextAlign.end : TextAlign.start,
    style: const TextStyle(
      color: MacGlassTokens.muted,
      fontSize: 10.5,
      fontWeight: FontWeight.w700,
    ),
  );
}

class _ParameterListRow extends StatelessWidget {
  const _ParameterListRow({
    required this.parameter,
    required this.displayName,
    required this.value,
    required this.selected,
    required this.staged,
    required this.onTap,
  });

  final FlightControllerParameter parameter;
  final String? displayName;
  final String value;
  final bool selected;
  final bool staged;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => Material(
    color: selected
        ? MacGlassTokens.blue.withValues(alpha: 0.11)
        : Colors.transparent,
    child: InkWell(
      onTap: onTap,
      child: Container(
        constraints: const BoxConstraints(minHeight: 48),
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 7),
        decoration: BoxDecoration(
          border: Border(
            bottom: BorderSide(
              color: MacGlassTokens.ink.withValues(alpha: 0.045),
            ),
          ),
        ),
        child: Row(
          children: [
            Expanded(
              flex: 11,
              child: Row(
                children: [
                  SizedBox(
                    width: 7,
                    child: staged
                        ? const DecoratedBox(
                            decoration: BoxDecoration(
                              color: MacGlassTokens.amber,
                              shape: BoxShape.circle,
                            ),
                            child: SizedBox.square(dimension: 5),
                          )
                        : null,
                  ),
                  const SizedBox(width: 5),
                  Expanded(
                    child: Text(
                      parameter.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: TextStyle(
                        color: selected
                            ? MacGlassTokens.blue
                            : MacGlassTokens.ink,
                        fontFamily: 'monospace',
                        fontSize: 12,
                        fontWeight: FontWeight.w700,
                        fontFeatures: const [FontFeature.tabularFigures()],
                      ),
                    ),
                  ),
                ],
              ),
            ),
            Expanded(
              flex: 10,
              child: Text(
                displayName ?? '—',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(
                  color: displayName == null
                      ? MacGlassTokens.muted
                      : MacGlassTokens.blue,
                  fontSize: 11.5,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            SizedBox(
              width: 136,
              child: Text(
                value,
                maxLines: 1,
                textAlign: TextAlign.end,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  fontFamily: 'monospace',
                  fontSize: 11.5,
                  fontWeight: FontWeight.w600,
                  fontFeatures: [FontFeature.tabularFigures()],
                ),
              ),
            ),
          ],
        ),
      ),
    ),
  );
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
      child: ListView(
        key: const ValueKey('mg-parameter-inspector'),
        padding: const EdgeInsets.all(16),
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
          Text(
            '当前值 ${_parameterValueLabel(selected.value, descriptor)}',
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
          ),
          const SizedBox(height: 18),
          _ParameterValueEditor(
            parameter: selected,
            descriptor: descriptor,
            value: staged?.stagedValue ?? selected.value,
            enabled: enabled && !(descriptor?.readOnly ?? false),
            onChanged: onStage,
          ),
          if (descriptor?.readOnly ?? false)
            const Padding(
              padding: EdgeInsets.only(top: 8),
              child: Text(
                '此参数为只读参数',
                style: TextStyle(color: MacGlassTokens.muted, fontSize: 11),
              ),
            ),
          if (descriptor?.requiresReboot ?? false)
            const Padding(
              padding: EdgeInsets.only(top: 8),
              child: Text(
                '修改后需要重启飞控生效',
                style: TextStyle(color: MacGlassTokens.amber, fontSize: 11),
              ),
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
          if (_descriptionFor(selected, descriptor) case final description?)
            SelectableText(
              description,
              style: const TextStyle(
                color: MacGlassTokens.muted,
                fontSize: 12,
                height: 1.45,
              ),
            )
          else
            const Padding(
              padding: EdgeInsets.symmetric(vertical: 24),
              child: Center(
                child: Text(
                  '暂无中文说明',
                  style: TextStyle(color: MacGlassTokens.muted, fontSize: 12),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _ParameterValueEditor extends StatelessWidget {
  const _ParameterValueEditor({
    required this.parameter,
    required this.descriptor,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final FlightControllerParameter parameter;
  final ParameterDescriptor? descriptor;
  final double value;
  final bool enabled;
  final ValueChanged<double> onChanged;

  @override
  Widget build(BuildContext context) {
    final type = descriptor?.type.toLowerCase() ?? '';
    final options = _distinctParameterOptions(descriptor);
    final bitmaskOptions = options
        .where((option) => option.value != 0)
        .toList(growable: false);
    if (type == 'bitmask' && bitmaskOptions.isNotEmpty) {
      return _BitmaskParameterEditor(
        parameterName: parameter.name,
        options: bitmaskOptions,
        value: value.round(),
        enabled: enabled,
        onChanged: (updated) => onChanged(updated.toDouble()),
      );
    }
    if (_isBinaryParameter(type, options)) {
      final falseOption = options
          .where((option) => option.value == 0)
          .firstOrNull;
      final trueOption = options
          .where((option) => option.value == 1)
          .firstOrNull;
      return CheckboxListTile(
        key: ValueKey('mg-parameter-editor-bool-${parameter.name}'),
        contentPadding: EdgeInsets.zero,
        dense: true,
        controlAffinity: ListTileControlAffinity.leading,
        value: value != 0,
        onChanged: enabled
            ? (checked) => onChanged(checked == true ? 1 : 0)
            : null,
        title: Text(trueOption?.label ?? '启用'),
        subtitle: Text('未勾选：${falseOption?.label ?? '禁用'}'),
      );
    }
    if (type == 'enum' && options.length > 1) {
      final selectedValue = value == value.roundToDouble()
          ? value.round()
          : null;
      if (selectedValue != null &&
          !options.any((option) => option.value == selectedValue)) {
        options.add(
          ParameterOption(value: selectedValue, label: '当前值（元数据未收录）'),
        );
      }
      if (selectedValue != null) {
        return _MacParameterPicker(
          key: ValueKey('mg-parameter-editor-enum-${parameter.name}'),
          value: selectedValue,
          options: options,
          enabled: enabled,
          onChanged: (updated) => onChanged(updated.toDouble()),
        );
      }
    }

    final range = descriptor?.minValue != null && descriptor?.maxValue != null
        ? '范围 ${_formatParameterNumber(descriptor!.minValue!)} ~ '
              '${_formatParameterNumber(descriptor!.maxValue!)}'
        : null;
    final increment = descriptor?.increment;
    final helper = [
      ?range,
      if (increment != null) '步进 ${_formatParameterNumber(increment)}',
    ].join('，');
    return TextFormField(
      key: ValueKey('mg-parameter-editor-number-${parameter.name}-$value'),
      initialValue: _formatParameterNumber(value),
      enabled: enabled,
      keyboardType: const TextInputType.numberWithOptions(
        decimal: true,
        signed: true,
      ),
      decoration: InputDecoration(
        labelText: '新值',
        suffixText: descriptor?.unit.isEmpty ?? true ? null : descriptor!.unit,
        helperText: helper.isEmpty ? null : helper,
      ),
      onFieldSubmitted: (text) {
        final updated = double.tryParse(text);
        if (updated != null) onChanged(updated);
      },
    );
  }
}

class _MacParameterPicker extends StatelessWidget {
  const _MacParameterPicker({
    required this.value,
    required this.options,
    required this.enabled,
    required this.onChanged,
    this.label = '新值',
    this.compact = false,
    this.inset = false,
    this.centered = false,
    this.showValue = true,
    super.key,
  });

  final int value;
  final List<ParameterOption> options;
  final bool enabled;
  final ValueChanged<int> onChanged;
  final String label;
  final bool compact;
  final bool inset;
  final bool centered;
  final bool showValue;

  String _optionText(ParameterOption option) =>
      showValue ? '${option.value} · ${option.label}' : option.label;

  @override
  Widget build(BuildContext context) {
    final selected = options
        .where((option) => option.value == value)
        .firstOrNull;
    final menuHeight = (options.length * 40.0).clamp(40.0, 360.0);
    final radius = inset ? 10.0 : 14.0;
    final height = inset ? 38.0 : (compact ? 44.0 : 52.0);
    return MenuAnchor(
      style: MacGlassMenu.style(minimumWidth: 268),
      alignmentOffset: Offset(0, inset ? 4 : 6),
      menuChildren: [
        SizedBox(
          width: 268,
          height: menuHeight,
          child: ListView(
            primary: false,
            padding: EdgeInsets.zero,
            children: [
              for (final option in options)
                SizedBox(
                  height: 40,
                  child: MenuItemButton(
                    key: ValueKey(
                      'mg-parameter-editor-enum-option-${option.value}',
                    ),
                    onPressed: enabled ? () => onChanged(option.value) : null,
                    style: MacGlassMenu.itemStyle(
                      selected: option.value == value,
                    ),
                    child: Row(
                      children: [
                        Icon(
                          option.value == value
                              ? Icons.check_rounded
                              : Icons.circle_outlined,
                          size: 16,
                          color: option.value == value
                              ? MacGlassTokens.green
                              : MacGlassTokens.muted.withValues(alpha: 0.42),
                        ),
                        const SizedBox(width: 9),
                        Expanded(
                          child: Text(
                            _optionText(option),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                              fontSize: 12.5,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
            ],
          ),
        ),
      ],
      builder: (context, controller, child) => DecoratedBox(
        decoration: BoxDecoration(
          color: inset
              ? MacGlassTokens.ink.withValues(alpha: enabled ? 0.045 : 0.025)
              : Colors.white.withValues(alpha: enabled ? 0.64 : 0.34),
          border: Border.all(
            color: inset
                ? MacGlassTokens.ink.withValues(alpha: enabled ? 0.075 : 0.04)
                : enabled
                ? Colors.white.withValues(alpha: 0.94)
                : MacGlassTokens.ink.withValues(alpha: 0.05),
          ),
          borderRadius: BorderRadius.circular(radius),
          boxShadow: inset
              ? const []
              : [
                  BoxShadow(
                    color: MacGlassTokens.ink.withValues(alpha: 0.055),
                    blurRadius: 18,
                    offset: const Offset(0, 6),
                  ),
                ],
        ),
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            borderRadius: BorderRadius.circular(radius),
            onTap: !enabled
                ? null
                : () {
                    if (controller.isOpen) {
                      controller.close();
                    } else {
                      controller.open();
                    }
                  },
            child: SizedBox(
              height: height,
              child: Padding(
                padding: EdgeInsets.fromLTRB(
                  inset ? 10 : 12,
                  inset ? 3 : 6,
                  inset ? 5 : 8,
                  inset ? 3 : 6,
                ),
                child: Row(
                  children: [
                    Expanded(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        crossAxisAlignment: centered
                            ? CrossAxisAlignment.center
                            : CrossAxisAlignment.start,
                        children: [
                          if (label.isNotEmpty) ...[
                            Text(
                              label,
                              style: const TextStyle(
                                color: MacGlassTokens.muted,
                                fontSize: 10.5,
                              ),
                            ),
                            const SizedBox(height: 1),
                          ],
                          Text(
                            selected == null
                                ? value.toString()
                                : _optionText(selected),
                            maxLines: 1,
                            textAlign: centered
                                ? TextAlign.center
                                : TextAlign.start,
                            overflow: TextOverflow.ellipsis,
                            style: TextStyle(
                              color: enabled
                                  ? MacGlassTokens.ink
                                  : MacGlassTokens.muted,
                              fontSize: 13,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ],
                      ),
                    ),
                    inset
                        ? const SizedBox.square(
                            dimension: 22,
                            child: Icon(Icons.expand_more_rounded, size: 18),
                          )
                        : DecoratedBox(
                            decoration: BoxDecoration(
                              color: MacGlassTokens.ink.withValues(
                                alpha: 0.055,
                              ),
                              borderRadius: BorderRadius.circular(9),
                            ),
                            child: const SizedBox.square(
                              dimension: 28,
                              child: Icon(Icons.unfold_more_rounded, size: 16),
                            ),
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
}

class _BitmaskParameterEditor extends StatelessWidget {
  const _BitmaskParameterEditor({
    required this.parameterName,
    required this.options,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final String parameterName;
  final List<ParameterOption> options;
  final int value;
  final bool enabled;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      const Text(
        '选项',
        style: TextStyle(fontSize: 11, color: MacGlassTokens.muted),
      ),
      for (final option in options)
        CheckboxListTile(
          key: ValueKey(
            'mg-parameter-editor-bitmask-$parameterName-${option.value}',
          ),
          contentPadding: EdgeInsets.zero,
          dense: true,
          controlAffinity: ListTileControlAffinity.leading,
          value: (value & option.value) != 0,
          onChanged: enabled
              ? (checked) => onChanged(
                  checked == true
                      ? value | option.value
                      : value & ~option.value,
                )
              : null,
          title: Text(option.label, maxLines: 2),
          subtitle: option.description.isEmpty
              ? null
              : Text(option.description, maxLines: 2),
        ),
    ],
  );
}

List<ParameterOption> _distinctParameterOptions(
  ParameterDescriptor? descriptor,
) {
  final distinct = <int, ParameterOption>{};
  for (final option in descriptor?.options ?? const <ParameterOption>[]) {
    distinct[option.value] = option;
  }
  return distinct.values.toList(growable: true);
}

bool _isBinaryParameter(String type, List<ParameterOption> options) {
  if (type == 'bool') return true;
  if (type != 'enum' || options.length != 2) return false;
  final byValue = {for (final option in options) option.value: option.label};
  if (!byValue.keys.toSet().containsAll({0, 1})) return false;
  final falseLabel = byValue[0]?.toLowerCase() ?? '';
  final trueLabel = byValue[1]?.toLowerCase() ?? '';
  const falseHints = ['disable', 'off', 'false', 'no', '禁用', '关闭', '否'];
  const trueHints = ['enable', 'on', 'true', 'yes', '启用', '开启', '是'];
  return falseHints.any(falseLabel.contains) &&
      trueHints.any(trueLabel.contains);
}

String _parameterValueLabel(double value, ParameterDescriptor? descriptor) {
  final number = _formatParameterNumber(value);
  if (value != value.roundToDouble()) return number;
  final option = _distinctParameterOptions(
    descriptor,
  ).where((item) => item.value == value.round()).firstOrNull;
  return option == null ? number : '$number · ${option.label}';
}

String _formatParameterNumber(double value) {
  return value == value.roundToDouble() ? value.round().toString() : '$value';
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
