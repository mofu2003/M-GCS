final class ParameterTreeNode {
  ParameterTreeNode({
    required this.label,
    required this.prefix,
    this.parameterName,
    List<ParameterTreeNode>? children,
  }) : children = children ?? [];

  final String label;
  final String prefix;
  final String? parameterName;
  final List<ParameterTreeNode> children;

  bool get isLeaf => parameterName != null;

  static ParameterTreeNode build(Iterable<String> names) {
    final root = ParameterTreeNode(label: '全部参数', prefix: '');
    final groupedNames = <String, List<String>>{};
    for (final name in names.toSet()..removeWhere((name) => name.isEmpty)) {
      if (!name.contains('_')) {
        root.children.add(_leaf(name));
        continue;
      }
      final category = name.split('_').first;
      (groupedNames[category] ??= []).add(name);
    }

    for (final entry in groupedNames.entries) {
      final categoryPrefix = '${entry.key}_';
      final category = ParameterTreeNode(
        label: entry.key,
        prefix: categoryPrefix,
      );
      root.children.add(category);
      _addCompressedChildren(category, categoryPrefix, entry.value);
    }
    _sort(root);
    return root;
  }

  // Mirrors Mission Planner's raw-parameter tree: only shared prefix segments
  // create an intermediate node. Parameters themselves stay intact as leaves.
  static void _addCompressedChildren(
    ParameterTreeNode category,
    String categoryPrefix,
    List<String> names,
  ) {
    final sortedNames = [...names]..sort();
    var current = category;
    var currentPrefix = categoryPrefix;
    final ancestors = <ParameterTreeNode>[category];

    for (var index = 0; index < sortedNames.length; index++) {
      final name = sortedNames[index];
      while (!name.startsWith(currentPrefix) && ancestors.length > 1) {
        ancestors.removeLast();
        current = ancestors.last;
        currentPrefix = current.prefix;
      }

      if (index == sortedNames.length - 1) {
        current.children.add(_leaf(name));
        break;
      }

      final nextName = sortedNames[index + 1];
      var segment = '${name.substring(currentPrefix.length).split('_').first}_';
      while (segment.length > 1 &&
          name.startsWith('$currentPrefix$segment') &&
          nextName.startsWith('$currentPrefix$segment')) {
        currentPrefix = '$currentPrefix$segment';
        current = ParameterTreeNode(
          label: currentPrefix.substring(0, currentPrefix.length - 1),
          prefix: currentPrefix,
        );
        ancestors.last.children.add(current);
        ancestors.add(current);
        segment = '${name.substring(currentPrefix.length).split('_').first}_';
      }
      current.children.add(_leaf(name));
    }
  }

  static ParameterTreeNode _leaf(String name) =>
      ParameterTreeNode(label: name, prefix: name, parameterName: name);

  static void _sort(ParameterTreeNode node) {
    node.children.sort((left, right) => left.label.compareTo(right.label));
    for (final child in node.children) {
      _sort(child);
    }
  }
}
