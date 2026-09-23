import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

void main() {
  test('application stays independent from Flutter and concrete adapters', () {
    final violations = _scan(
      Directory('lib/application'),
      forbidden: const [
        "package:flutter/",
        "package:flutter_",
        "dart:ffi",
        "/infrastructure/",
        "/presentation/",
        "/features/",
        "/ui/",
      ],
    );

    expect(
      violations,
      isEmpty,
      reason: 'Application models and ports must remain frontend-independent.',
    );
  });

  test('only infrastructure may access native FFI', () {
    final violations = <String>[];
    for (final file in _dartFiles(Directory('lib'))) {
      final relative = file.path.replaceAll('\\', '/');
      if (relative.contains('/infrastructure/')) continue;
      final content = file.readAsStringSync();
      if (content.contains('dart:ffi') || content.contains('package:ffi/')) {
        violations.add(relative);
      }
    }

    expect(
      violations,
      isEmpty,
      reason: 'Native bindings belong behind infrastructure adapters.',
    );
  });

  test('presentation controllers do not depend on concrete infrastructure', () {
    final violations = _scan(
      Directory('lib/presentation/controllers'),
      forbidden: const ['/infrastructure/', 'dart:ffi', 'package:ffi/'],
    );

    expect(
      violations,
      isEmpty,
      reason: 'Flutter state may consume application ports, not MgCore itself.',
    );
  });
}

List<String> _scan(Directory root, {required List<String> forbidden}) {
  final violations = <String>[];
  for (final file in _dartFiles(root)) {
    final content = file.readAsStringSync();
    for (final pattern in forbidden) {
      if (content.contains(pattern)) {
        violations.add('${file.path}: $pattern');
      }
    }
  }
  return violations;
}

Iterable<File> _dartFiles(Directory root) sync* {
  if (!root.existsSync()) return;
  for (final entity in root.listSync(recursive: true)) {
    if (entity is File && entity.path.endsWith('.dart')) yield entity;
  }
}
