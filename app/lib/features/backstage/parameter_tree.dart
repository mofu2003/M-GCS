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
      if (_flightModeParameter.hasMatch(name)) {
        (groupedNames['FLTMODE'] ??= []).add(name);
        continue;
      }
      if (!name.contains('_')) {
        root.children.add(_leaf(name));
        continue;
      }
      final category = name.split('_').first;
      (groupedNames[category] ??= []).add(name);
    }

    for (final entry in groupedNames.entries) {
      final categoryPrefix = entry.key == 'FLTMODE'
          ? 'FLTMODE'
          : '${entry.key}_';
      final category = ParameterTreeNode(
        label: entry.key,
        prefix: categoryPrefix,
      );
      root.children.add(category);
      for (final name in entry.value) {
        category.children.add(_leaf(name));
      }
    }
    _sort(root);
    return root;
  }

  static final RegExp _flightModeParameter = RegExp(r'^FLTMODE[1-6]$');

  static ParameterTreeNode _leaf(String name) =>
      ParameterTreeNode(label: name, prefix: name, parameterName: name);

  static void _sort(ParameterTreeNode node) {
    node.children.sort((left, right) => left.label.compareTo(right.label));
    for (final child in node.children) {
      _sort(child);
    }
  }
}
