import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/updates/update_backend.dart';
import 'package:mgcs_frontend/updates/update_controller.dart';
import 'package:mgcs_frontend/updates/update_models.dart';
import 'package:mgcs_frontend/updates/update_preferences.dart';
import 'package:mgcs_frontend/updates/update_settings_page.dart';

class _MemoryPreferences implements UpdatePreferencesStore {
  _MemoryPreferences({
    this.value = const UpdatePreferences(
      manifestUrl: 'https://example.com/update.json',
      automaticChecks: true,
    ),
  });

  UpdatePreferences value;

  @override
  Future<UpdatePreferences> load() async => value;

  @override
  Future<void> save(UpdatePreferences preferences) async => value = preferences;
}

class _Backend implements UpdateBackend {
  final bytes = <int>[1, 2, 3];
  Object? fetchError;

  @override
  Future<Map<String, Object?>> fetchManifest(Uri uri) async {
    final error = fetchError;
    if (error != null) throw error;
    return {
      'version': '1.3.0',
      'build': 4,
      'release_notes': '修复地图定位并改进小队状态。',
      'assets': [
        {
          'platform': 'macos',
          'architecture': 'arm64',
          'url': 'https://example.com/M-GCS.zip',
          'sha256': sha256.convert(bytes).toString(),
          'file_name': 'M-GCS.zip',
        },
      ],
    };
  }

  @override
  Future<void> download(
    Uri uri,
    File target, {
    void Function(int received, int? total)? onProgress,
  }) => target.writeAsBytes(bytes);

  @override
  Future<bool> openFile(File file) async => true;
}

void main() {
  testWidgets('update page shows current version and available release', (
    tester,
  ) async {
    final controller = UpdateController(
      backend: _Backend(),
      preferences: _MemoryPreferences(),
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);
    await controller.initialize();
    await controller.checkForUpdates();

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: UpdateSettingsPage(
              scrollController: ScrollController(),
              controller: controller,
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('M-GCS 1.2.0'), findsOneWidget);
    expect(find.text('版本 1.3.0 可用'), findsOneWidget);
    expect(find.text('修复地图定位并改进小队状态。'), findsOneWidget);
    expect(find.byKey(const ValueKey('update-download')), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('failed automatic check is shown as retryable, not as an error', (
    tester,
  ) async {
    final backend = _Backend()..fetchError = const FormatException('无发布版本');
    final controller = UpdateController(
      backend: backend,
      preferences: _MemoryPreferences(),
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);
    await controller.autoCheckIfDue();

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: UpdateSettingsPage(
              scrollController: ScrollController(),
              controller: controller,
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.textContaining('上次自动检查未取得更新清单'), findsOneWidget);
    expect(find.text('上次检查失败'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('restores the default manifest source', (tester) async {
    final preferences = _MemoryPreferences(
      value: const UpdatePreferences(manifestUrl: '', automaticChecks: true),
    );
    final controller = UpdateController(
      backend: _Backend(),
      preferences: preferences,
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);
    await controller.initialize();

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: UpdateSettingsPage(
              scrollController: ScrollController(),
              controller: controller,
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const ValueKey('update-manifest-url')), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('update-restore-default')));
    await tester.pumpAndSettle();

    expect(controller.manifestUrl, kDefaultUpdateManifestUrl);
    expect(preferences.value.manifestUrl, kDefaultUpdateManifestUrl);
    expect(find.text(kDefaultUpdateManifestUrl), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
