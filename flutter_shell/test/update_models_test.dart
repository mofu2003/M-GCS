import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/updates/update_models.dart';

void main() {
  const current = AppVersion(version: '1.2.0', build: 3);
  const target = UpdateTarget(platform: 'macos', architecture: 'arm64');

  test('compares semantic version before build number', () {
    expect(
      isUpdateNewer(
        current,
        const UpdateManifest(
          version: '1.3.0',
          build: 0,
          releaseNotes: '',
          assets: [],
        ),
      ),
      isTrue,
    );
    expect(
      isUpdateNewer(
        current,
        const UpdateManifest(
          version: '1.2.0',
          build: 4,
          releaseNotes: '',
          assets: [],
        ),
      ),
      isTrue,
    );
    expect(
      isUpdateNewer(
        current,
        const UpdateManifest(
          version: '1.2.0',
          build: 3,
          releaseNotes: '',
          assets: [],
        ),
      ),
      isFalse,
    );
  });

  test('prefers the matching architecture and accepts a generic asset', () {
    final manifest = UpdateManifest.fromJson({
      'version': '1.3.0',
      'build': 4,
      'release_notes': 'fixes',
      'assets': [
        {
          'platform': 'macos',
          'architecture': 'arm64',
          'url': 'https://example.com/mac-arm64.zip',
          'sha256': List.filled(64, 'a').join(),
          'file_name': 'mac-arm64.zip',
        },
        {
          'platform': 'macos',
          'url': 'https://example.com/mac.zip',
          'sha256': List.filled(64, 'b').join(),
          'file_name': 'mac.zip',
        },
      ],
    });

    expect(manifest.assetFor(target)!.fileName, 'mac-arm64.zip');
    expect(
      manifest
          .assetFor(const UpdateTarget(platform: 'macos', architecture: 'x64'))!
          .fileName,
      'mac.zip',
    );
    expect(manifest.releaseNotes, 'fixes');
  });

  test('rejects insecure URLs and malformed checksums', () {
    expect(
      UpdateAsset.fromJson({
        'platform': 'macos',
        'url': 'http://example.com/update.zip',
        'sha256': List.filled(64, 'a').join(),
        'file_name': 'update.zip',
      }),
      isNull,
    );
    expect(
      UpdateAsset.fromJson({
        'platform': 'macos',
        'url': 'https://example.com/update.zip',
        'sha256': 'not-a-hash',
        'file_name': 'update.zip',
      }),
      isNull,
    );
    expect(
      validateUpdateManifestUrl('https://example.com/update.json'),
      isNull,
    );
    expect(
      validateUpdateManifestUrl('http://example.com/update.json'),
      isNotNull,
    );
  });

  test('defaults to the public GitHub release manifest', () {
    expect(
      kDefaultUpdateManifestUrl,
      'https://github.com/mofu2003/M-GCS/releases/latest/download/update.json',
    );
  });
}
