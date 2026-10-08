import 'dart:io';

const _allowedFiles = {
  'demo/component_gallery_demo.dart',
  'theme/mg_colors.dart',
  'theme/mg_typography.dart',
};

final _rules = <_TokenRule>[
  _TokenRule(
    '直接使用 MacosColors',
    _regex(r'\bMacosColors\.'),
    '改用 context.mgColors 或 MgColors.of(context)。',
  ),
  _TokenRule(
    '直接使用 Flutter/Cupertino 调色板',
    _regex(r'\b(?:Colors|CupertinoColors)\.'),
    '改用 MgColors 语义色。',
  ),
  _TokenRule(
    '裸色值',
    _regex(r'\bColor\s*\(\s*0x|\bColor\.from(?:ARGB|RGBO)\s*\('),
    '色值集中定义在 mg_colors.dart。',
  ),
  _TokenRule(
    '裸 TextStyle 字号',
    _findTextStyleFontSize,
    '改用 MgTypography 中的字号 token。',
  ),
  _TokenRule(
    '硬编码字体族',
    _regex(r'''fontFamily\s*:\s*['"]'''),
    '改用 MgFonts / MgTypography 的字体辅助方法。',
  ),
  _TokenRule(
    '读取 macos_ui 亮度',
    _regex(r'MacosTheme\.brightnessOf\s*\('),
    '改用 MgColors.isDark 或 MgColors.brightness。',
  ),
  _TokenRule(
    '读取 macos_ui 分隔线色',
    _regex(r'\btheme\.dividerColor\b|MacosTheme\.of\([^)]*\)\.dividerColor'),
    '改用 MgColors.divider。',
  ),
  _TokenRule(
    '裸面板圆角',
    _regex(r'BorderRadius\.circular\(\s*10(?:\.0)?\s*\)'),
    '改用 MgMetrics.panelRadius。',
  ),
  _TokenRule(
    '裸控件圆角',
    _regex(r'BorderRadius\.circular\(\s*7(?:\.0)?\s*\)'),
    '改用 MgMetrics.controlRadius。',
  ),
  _TokenRule(
    '裸表格行高',
    _regex(r'\bheight\s*:\s*48(?:\.0)?\b'),
    '改用 MgMetrics.rowHeight。',
  ),
  _TokenRule(
    '裸工具栏高度',
    _regex(r'topInset\s*\+\s*52(?:\.0)?\b'),
    '改用 MgMetrics.toolbarHeight。',
  ),
];

void main(List<String> args) {
  if (args.contains('--help')) {
    stdout.writeln('用法：dart tools/check_ui_tokens.dart [--self-test]');
    return;
  }
  if (args.contains('--self-test')) {
    if (!_runSelfTests()) exitCode = 1;
    return;
  }
  if (args.isNotEmpty) {
    stderr.writeln('[ui-tokens] 未知参数：${args.join(' ')}');
    exitCode = 2;
    return;
  }

  if (!_runSelfTests()) {
    exitCode = 1;
    return;
  }

  final repoRoot = File.fromUri(Platform.script).parent.parent.absolute.path;
  final libDir = Directory('$repoRoot/flutter_shell/lib');
  if (!libDir.existsSync()) {
    stderr.writeln('[ui-tokens] 未找到目录：${libDir.path}');
    exitCode = 1;
    return;
  }

  final files =
      libDir
          .listSync(recursive: true)
          .whereType<File>()
          .where((file) => file.path.endsWith('.dart'))
          .toList()
        ..sort((a, b) => a.path.compareTo(b.path));

  final findings = <_Finding>[];
  for (final file in files) {
    final relative = file.path
        .substring(libDir.path.length + 1)
        .replaceAll(Platform.pathSeparator, '/');
    if (_allowedFiles.contains(relative)) continue;

    final source = _sanitizeDart(file.readAsStringSync());
    for (final rule in _rules) {
      for (final offset in rule.findMatches(source)) {
        findings.add(
          _Finding(
            file: relative,
            line: _lineNumberAt(source, offset),
            rule: rule,
          ),
        );
      }
    }
  }

  if (findings.isEmpty) {
    stdout.writeln('[ui-tokens] 通过：未发现绕过设计 token 的用法。');
    return;
  }

  stderr.writeln('[ui-tokens] 发现 ${findings.length} 处违规：');
  for (final finding in findings) {
    stderr.writeln(
      '  flutter_shell/lib/${finding.file}:${finding.line} '
      '${finding.rule.name}。${finding.rule.fix}',
    );
  }
  exitCode = 1;
}

bool _runSelfTests() {
  const cases = [
    _SelfTestCase('注释不参与扫描', '// MacosColors.red\n', 0),
    _SelfTestCase('字符串内容不参与扫描', "final x = 'MacosColors.red';\n", 0),
    _SelfTestCase(
      '字符串后的违规仍被发现',
      "final u = 'https://example.com';\nfinal c = MacosColors.red;\n",
      1,
    ),
    _SelfTestCase('嵌套参数不隐藏裸字号', 'TextStyle(foo(), fontSize: 12)\n', 1),
    _SelfTestCase('TextStyle.new 也被发现', 'TextStyle.new(fontSize: 12)\n', 1),
    _SelfTestCase(
      '合法 TextStyle 不误报',
      'TextStyle(fontWeight: FontWeight.w600)\n',
      0,
    ),
    _SelfTestCase('裸面板圆角被发现', 'BorderRadius.circular(10)\n', 1),
    _SelfTestCase('裸控件圆角被发现', 'BorderRadius.circular(7)\n', 1),
    _SelfTestCase('裸表格行高被发现', 'SizedBox(height: 48)\n', 1),
    _SelfTestCase('MgColors 不误报', 'MgColors.of(context).accent\n', 0),
  ];

  for (final test in cases) {
    final source = _sanitizeDart(test.source);
    final count = _rules.fold<int>(
      0,
      (total, rule) => total + rule.findMatches(source).length,
    );
    if (count != test.expected) {
      stderr.writeln(
        '[ui-tokens] 自检失败：${test.name}，期望 ${test.expected}，实际 $count',
      );
      return false;
    }
  }
  stdout.writeln('[ui-tokens] 自检通过：${cases.length} 项。');
  return true;
}

List<int> Function(String source) _regex(String pattern) {
  final regex = RegExp(pattern);
  return (source) => [
    for (final match in regex.allMatches(source)) match.start,
  ];
}

List<int> _findTextStyleFontSize(String source) {
  final constructor = RegExp(r'\bTextStyle(?:\.new)?\s*\(');
  final findings = <int>[];
  for (final match in constructor.allMatches(source)) {
    final open = source.indexOf('(', match.start);
    final close = _matchingParenthesis(source, open);
    if (close < 0) continue;
    final body = source.substring(open + 1, close);
    final argumentOffset = _topLevelArgumentOffset(body, 'fontSize');
    if (argumentOffset != null) {
      findings.add(open + 1 + argumentOffset);
    }
  }
  return findings;
}

int _matchingParenthesis(String source, int open) {
  if (open < 0 || source[open] != '(') return -1;
  var depth = 0;
  for (var index = open; index < source.length; index++) {
    if (source[index] == '(') depth++;
    if (source[index] == ')') {
      depth--;
      if (depth == 0) return index;
    }
  }
  return -1;
}

int? _topLevelArgumentOffset(String body, String argument) {
  var depth = 0;
  for (var index = 0; index < body.length; index++) {
    final char = body[index];
    if (char == '(' || char == '[' || char == '{') depth++;
    if (char == ')' || char == ']' || char == '}') depth--;
    if (depth != 0 || !_identifierAt(body, index, argument)) continue;

    var cursor = index + argument.length;
    while (cursor < body.length && body[cursor].trim().isEmpty) {
      cursor++;
    }
    if (cursor < body.length && body[cursor] == ':') return index;
  }
  return null;
}

bool _identifierAt(String source, int index, String identifier) {
  if (!source.startsWith(identifier, index)) return false;
  final before = index == 0 ? null : source[index - 1];
  final afterIndex = index + identifier.length;
  final after = afterIndex >= source.length ? null : source[afterIndex];
  return !_isIdentifierPart(before) && !_isIdentifierPart(after);
}

bool _isIdentifierPart(String? char) {
  if (char == null) return false;
  final code = char.codeUnitAt(0);
  return (code >= 48 && code <= 57) ||
      (code >= 65 && code <= 90) ||
      (code >= 97 && code <= 122) ||
      char == '_' ||
      char == r'$';
}

String _sanitizeDart(String source) {
  final output = StringBuffer();
  var index = 0;

  while (index < source.length) {
    if (source.startsWith('//', index)) {
      while (index < source.length && source[index] != '\n') {
        output.write(' ');
        index++;
      }
      continue;
    }
    if (source.startsWith('/*', index)) {
      output.write('  ');
      index += 2;
      while (index < source.length && !source.startsWith('*/', index)) {
        output.write(source[index] == '\n' ? '\n' : ' ');
        index++;
      }
      if (index < source.length) {
        output.write('  ');
        index += 2;
      }
      continue;
    }

    final char = source[index];
    if (char == "'" || char == '"') {
      final quote = char;
      output.write(quote);
      index++;
      while (index < source.length) {
        final current = source[index];
        if (current == r'\' && index + 1 < source.length) {
          output.write('  ');
          index += 2;
          continue;
        }
        output.write(current == '\n' ? '\n' : ' ');
        index++;
        if (current == quote) break;
      }
      continue;
    }

    output.write(char);
    index++;
  }

  return output.toString();
}

int _lineNumberAt(String source, int offset) =>
    '\n'.allMatches(source.substring(0, offset)).length + 1;

class _TokenRule {
  const _TokenRule(this.name, this.findMatches, this.fix);

  final String name;
  final List<int> Function(String source) findMatches;
  final String fix;
}

class _Finding {
  const _Finding({required this.file, required this.line, required this.rule});

  final String file;
  final int line;
  final _TokenRule rule;
}

class _SelfTestCase {
  const _SelfTestCase(this.name, this.source, this.expected);

  final String name;
  final String source;
  final int expected;
}
