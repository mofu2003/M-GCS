import 'dart:io';

import 'package:flutter/foundation.dart';

/// 默认从公开仓库的最新 GitHub Release 获取更新清单。
/// 发布构建可用
/// `--dart-define=MGCS_UPDATE_MANIFEST_URL=https://.../update.json` 覆盖。
const String kDefaultUpdateManifestUrl = String.fromEnvironment(
  'MGCS_UPDATE_MANIFEST_URL',
  defaultValue:
      'https://github.com/mofu2003/M-GCS/releases/latest/download/update.json',
);

const Duration kAutomaticUpdateCheckInterval = Duration(hours: 24);

/// 当前程序版本。
@immutable
class AppVersion {
  const AppVersion({required this.version, required this.build});

  final String version;
  final int build;

  @override
  String toString() => build > 0 ? '$version($build)' : version;
}

/// 更新包对应的平台和 CPU 架构。
@immutable
class UpdateTarget {
  const UpdateTarget({required this.platform, required this.architecture});

  final String platform;
  final String architecture;

  static UpdateTarget current() {
    final platform = switch (Platform.operatingSystem) {
      'macos' => 'macos',
      'windows' => 'windows',
      'linux' => 'linux',
      'android' => 'android',
      'ios' => 'ios',
      _ => Platform.operatingSystem,
    };
    final runtime = RegExp(
      r'on "([^"]+)"',
    ).firstMatch(Platform.version)?.group(1);
    final separator = runtime?.indexOf('_') ?? -1;
    final abi = separator < 0 ? '' : runtime!.substring(separator + 1);
    final architecture = switch (abi) {
      'arm64' || 'aarch64' => 'arm64',
      'x64' || 'amd64' => 'x64',
      'ia32' || 'x86' => 'ia32',
      _ => abi.isEmpty ? 'unknown' : abi,
    };
    return UpdateTarget(platform: platform, architecture: architecture);
  }

  String get label => switch (platform) {
    'macos' => 'macOS',
    'windows' => 'Windows',
    'linux' => 'Linux',
    'android' => 'Android',
    'ios' => 'iOS',
    _ => platform,
  };
}

/// 更新清单中的一个可下载安装包。
@immutable
class UpdateAsset {
  const UpdateAsset({
    required this.platform,
    required this.architecture,
    required this.url,
    required this.sha256,
    required this.fileName,
    this.size,
  });

  final String platform;
  final String? architecture;
  final Uri url;
  final String sha256;
  final String fileName;
  final int? size;

  bool matches(UpdateTarget target) =>
      platform == target.platform &&
      (architecture == null || architecture == target.architecture);

  static UpdateAsset? fromJson(Object? raw) {
    if (raw is! Map) return null;
    final platform = raw['platform'];
    final url = Uri.tryParse(raw['url'] as String? ?? '');
    final sha = (raw['sha256'] as String? ?? '').trim().toLowerCase();
    final fileName = (raw['file_name'] as String? ?? '').trim();
    if (platform is! String ||
        platform.trim().isEmpty ||
        url == null ||
        !_isSecureDownloadUrl(url) ||
        !RegExp(r'^[0-9a-f]{64}$').hasMatch(sha)) {
      return null;
    }
    final architecture = raw['architecture'];
    final size = (raw['size'] as num?)?.toInt();
    final safeName = fileName.isEmpty
        ? url.pathSegments.isEmpty
              ? 'M-GCS-update'
              : url.pathSegments.last
        : fileName;
    return UpdateAsset(
      platform: platform.trim().toLowerCase(),
      architecture: architecture is String && architecture.trim().isNotEmpty
          ? architecture.trim().toLowerCase()
          : null,
      url: url,
      sha256: sha,
      fileName: safeName,
      size: size != null && size > 0 ? size : null,
    );
  }
}

/// 更新服务器返回的版本清单。
@immutable
class UpdateManifest {
  const UpdateManifest({
    required this.version,
    required this.build,
    required this.releaseNotes,
    required this.assets,
    this.publishedAt,
  });

  final String version;
  final int build;
  final String? publishedAt;
  final String releaseNotes;
  final List<UpdateAsset> assets;

  UpdateAsset? assetFor(UpdateTarget target) {
    UpdateAsset? generic;
    for (final asset in assets) {
      if (!asset.matches(target)) continue;
      if (asset.architecture == target.architecture) return asset;
      generic ??= asset;
    }
    return generic;
  }

  static UpdateManifest fromJson(Object? raw) {
    if (raw is! Map) throw const FormatException('更新清单不是 JSON 对象');
    final version = (raw['version'] as String? ?? '').trim();
    if (!_parseVersion(version).isValid) {
      throw const FormatException('更新清单版本号无效');
    }
    final rawAssets = raw['assets'];
    if (rawAssets is! List) {
      throw const FormatException('更新清单缺少 assets');
    }
    final assets = <UpdateAsset>[];
    for (final item in rawAssets) {
      final asset = UpdateAsset.fromJson(item);
      if (asset != null) assets.add(asset);
    }
    if (assets.isEmpty) {
      throw const FormatException('更新清单没有可用的安装包');
    }
    final notes = raw['release_notes'];
    return UpdateManifest(
      version: version,
      build: (raw['build'] as num?)?.toInt() ?? 0,
      publishedAt: raw['published_at'] as String?,
      releaseNotes: notes is String ? notes.trim() : '',
      assets: List.unmodifiable(assets),
    );
  }
}

/// 校验更新清单地址。更新会下载可执行安装包，因此只接受 HTTPS。
String? validateUpdateManifestUrl(String raw) {
  final value = raw.trim();
  if (value.isEmpty) return '请填写更新清单地址';
  final uri = Uri.tryParse(value);
  if (uri == null) return '更新清单地址无法解析';
  if (uri.scheme != 'https') return '更新清单必须使用 HTTPS';
  if (uri.host.isEmpty) return '更新清单地址缺少主机名';
  if (uri.userInfo.isNotEmpty) return '更新清单地址不能包含账号信息';
  return null;
}

bool _isSecureDownloadUrl(Uri uri) =>
    uri.scheme == 'https' && uri.host.isNotEmpty && uri.userInfo.isEmpty;

/// 比较点分十进制版本；相同主版本时再比较构建号。
int compareAppVersions(AppVersion current, UpdateManifest candidate) {
  final currentParts = _parseVersion(current.version);
  final candidateParts = _parseVersion(candidate.version);
  if (!currentParts.isValid || !candidateParts.isValid) return 0;
  for (var index = 0; index < 3; index++) {
    final delta = candidateParts.parts[index] - currentParts.parts[index];
    if (delta != 0) return delta.sign;
  }
  return candidate.build.compareTo(current.build);
}

bool isUpdateNewer(AppVersion current, UpdateManifest candidate) =>
    compareAppVersions(current, candidate) > 0;

_ParsedVersion _parseVersion(String raw) {
  final match = RegExp(r'^(\d+)\.(\d+)\.(\d+)$').firstMatch(raw.trim());
  if (match == null) {
    return const _ParsedVersion(isValid: false, parts: [0, 0, 0]);
  }
  return _ParsedVersion(
    isValid: true,
    parts: [
      int.parse(match.group(1)!),
      int.parse(match.group(2)!),
      int.parse(match.group(3)!),
    ],
  );
}

class _ParsedVersion {
  const _ParsedVersion({required this.isValid, required this.parts});

  final bool isValid;
  final List<int> parts;
}
