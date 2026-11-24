import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/updates/update_backend.dart';
import 'package:mgcs_frontend/updates/update_controller.dart';
import 'package:mgcs_frontend/updates/update_models.dart';
import 'package:mgcs_frontend/updates/update_preferences.dart';

class _MemoryPreferences implements UpdatePreferencesStore {
  _MemoryPreferences(this.value);

  UpdatePreferences value;
  int saves = 0;

  @override
  Future<UpdatePreferences> load() async => value;

  @override
  Future<void> save(UpdatePreferences preferences) async {
    saves++;
    value = preferences;
  }
}

class _FakeBackend implements UpdateBackend {
  _FakeBackend({required this.manifest, required this.downloadBytes});

  Map<String, Object?> manifest;
  List<int> downloadBytes;
  int fetches = 0;
  int downloads = 0;
  int opens = 0;
  Object? fetchError;
  bool openSucceeds = true;

  @override
  Future<Map<String, Object?>> fetchManifest(Uri uri) async {
    fetches++;
    final error = fetchError;
    if (error != null) throw error;
    return manifest;
  }

  @override
  Future<void> download(
    Uri uri,
    File target, {
    void Function(int received, int? total)? onProgress,
  }) async {
    downloads++;
    await target.parent.create(recursive: true);
    await target.writeAsBytes(downloadBytes, flush: true);
    onProgress?.call(downloadBytes.length, downloadBytes.length);
  }

  @override
  Future<bool> openFile(File file) async {
    opens++;
    return openSucceeds && await file.exists();
  }
}

void main() {
  late Directory temp;
  late _MemoryPreferences preferences;
  late _FakeBackend backend;
  late UpdateController controller;
  final bytes = <int>[1, 2, 3, 4, 5, 6];

  setUp(() async {
    temp = await Directory.systemTemp.createTemp('mgcs-update-test-');
    preferences = _MemoryPreferences(
      const UpdatePreferences(
        manifestUrl: 'https://example.com/update.json',
        automaticChecks: true,
      ),
    );
    backend = _FakeBackend(
      manifest: {
        'version': '1.3.0',
        'build': 4,
        'release_notes': '更新说明',
        'assets': [
          {
            'platform': 'macos',
            'architecture': 'arm64',
            'url': 'https://example.com/M-GCS.zip',
            'sha256': sha256.convert(bytes).toString(),
            'file_name': 'M-GCS.zip',
            'size': bytes.length,
          },
        ],
      },
      downloadBytes: bytes,
    );
    controller = UpdateController(
      backend: backend,
      preferences: preferences,
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      downloadDirectory: () async => temp,
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);
    addTearDown(() => temp.delete(recursive: true));
  });

  test('finds, downloads, verifies and opens a compatible update', () async {
    await controller.initialize();

    expect(await controller.checkForUpdates(), isTrue);
    expect(controller.availableUpdate!.version, '1.3.0');
    expect(controller.selectedAsset!.fileName, 'M-GCS.zip');
    expect(preferences.value.lastCheckAt, DateTime.utc(2026, 11, 15, 12));

    final file = await controller.downloadAvailableUpdate();
    expect(await file.readAsBytes(), bytes);
    expect(controller.downloadProgress, 1);
    expect(await controller.openDownloadedUpdate(), isTrue);
    expect(backend.fetches, 1);
    expect(backend.downloads, 1);
    expect(backend.opens, 1);
  });

  test('rejects a downloaded file whose checksum differs', () async {
    backend.downloadBytes = <int>[9, 9, 9];
    final assets = backend.manifest['assets']! as List<Object?>;
    final asset = assets.single! as Map<String, Object?>;
    asset.remove('size');
    await controller.initialize();
    expect(await controller.checkForUpdates(), isTrue);

    await expectLater(
      controller.downloadAvailableUpdate(),
      throwsA(isA<FileSystemException>()),
    );
    expect(controller.downloadedFile, isNull);
    expect(controller.error, contains('SHA-256'));
  });

  test('rejects a downloaded file whose declared size differs', () async {
    final assets = backend.manifest['assets']! as List<Object?>;
    final asset = assets.single! as Map<String, Object?>;
    asset['size'] = bytes.length + 1;
    await controller.initialize();
    expect(await controller.checkForUpdates(), isTrue);

    await expectLater(
      controller.downloadAvailableUpdate(),
      throwsA(isA<FileSystemException>()),
    );
    expect(controller.downloadedFile, isNull);
    expect(controller.error, contains('大小不符'));
  });

  test('automatic check runs at most once per interval', () async {
    preferences.value = UpdatePreferences(
      manifestUrl: preferences.value.manifestUrl,
      automaticChecks: true,
      lastCheckAt: DateTime.utc(2026, 11, 15, 11),
    );

    expect(await controller.autoCheckIfDue(), isFalse);
    expect(backend.fetches, 0);

    preferences.value = UpdatePreferences(
      manifestUrl: preferences.value.manifestUrl,
      automaticChecks: true,
      lastCheckAt: DateTime.utc(2026, 11, 14, 10),
    );
    controller = UpdateController(
      backend: backend,
      preferences: preferences,
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      downloadDirectory: () async => temp,
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);

    expect(await controller.autoCheckIfDue(), isTrue);
    expect(backend.fetches, 1);
  });

  test('automatic check failure stays quiet and records the attempt', () async {
    backend.fetchError = const HttpException('找不到更新清单');

    expect(await controller.autoCheckIfDue(), isFalse);
    expect(controller.error, isNull);
    expect(controller.lastCheckSucceeded, isFalse);
    expect(controller.lastCheckAt, DateTime.utc(2026, 11, 15, 12));
    expect(preferences.value.lastCheckAt, DateTime.utc(2026, 11, 15, 12));
  });

  test('manual check requires a configured source', () async {
    preferences.value = const UpdatePreferences(
      manifestUrl: '',
      automaticChecks: true,
    );
    controller = UpdateController(
      backend: backend,
      preferences: preferences,
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      downloadDirectory: () async => temp,
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);

    expect(await controller.checkForUpdates(), isFalse);
    expect(controller.error, '请先配置更新清单地址');
    expect(backend.fetches, 0);
  });

  test('restores the public release source after it was cleared', () async {
    preferences.value = const UpdatePreferences(
      manifestUrl: '',
      automaticChecks: true,
    );
    controller = UpdateController(
      backend: backend,
      preferences: preferences,
      versionLoader: () async => const AppVersion(version: '1.2.0', build: 3),
      downloadDirectory: () async => temp,
      target: const UpdateTarget(platform: 'macos', architecture: 'arm64'),
      now: () => DateTime.utc(2026, 11, 15, 12),
    );
    addTearDown(controller.dispose);

    await controller.initialize();
    await controller.restoreDefaultManifestUrl();

    expect(controller.manifestUrl, kDefaultUpdateManifestUrl);
    expect(preferences.value.manifestUrl, kDefaultUpdateManifestUrl);
    expect(preferences.value.lastCheckAt, isNull);
  });

  test('failed refresh keeps the last known update available', () async {
    await controller.initialize();
    expect(await controller.checkForUpdates(), isTrue);

    backend.fetchError = const HttpException('更新服务器不可用');
    expect(await controller.checkForUpdates(), isFalse);
    expect(controller.availableUpdate?.version, '1.3.0');
    expect(controller.error, contains('更新服务器不可用'));
  });

  test('reports when the system cannot open the downloaded package', () async {
    backend.openSucceeds = false;
    await controller.initialize();
    expect(await controller.checkForUpdates(), isTrue);
    await controller.downloadAvailableUpdate();

    await expectLater(
      controller.openDownloadedUpdate(),
      throwsA(isA<FileSystemException>()),
    );
    expect(controller.error, '系统无法打开更新包');
  });
}
