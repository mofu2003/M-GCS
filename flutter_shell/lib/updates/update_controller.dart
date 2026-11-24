import 'dart:async';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/foundation.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:path_provider/path_provider.dart';

import 'update_backend.dart';
import 'update_models.dart';
import 'update_preferences.dart';

typedef UpdateVersionLoader = Future<AppVersion> Function();
typedef UpdateDirectoryProvider = Future<Directory> Function();

final class UpdateController extends ChangeNotifier {
  UpdateController({
    UpdateBackend? backend,
    UpdatePreferencesStore? preferences,
    UpdateVersionLoader? versionLoader,
    UpdateDirectoryProvider? downloadDirectory,
    UpdateTarget? target,
    DateTime Function()? now,
  }) : _backend = backend ?? const HttpUpdateBackend(),
       _preferences = preferences ?? FileUpdatePreferencesStore(),
       _versionLoader = versionLoader ?? _loadPackageVersion,
       _downloadDirectory = downloadDirectory ?? getTemporaryDirectory,
       _target = target ?? UpdateTarget.current(),
       _now = now ?? DateTime.now;

  static const automaticCheckInterval = kAutomaticUpdateCheckInterval;

  final UpdateBackend _backend;
  final UpdatePreferencesStore _preferences;
  final UpdateVersionLoader _versionLoader;
  final UpdateDirectoryProvider _downloadDirectory;
  final UpdateTarget _target;
  final DateTime Function() _now;

  UpdatePreferences? _settings;
  AppVersion? _currentVersion;
  bool _initialized = false;
  bool _initializing = false;
  bool _checking = false;
  bool _downloading = false;
  bool _lastCheckSucceeded = false;
  UpdateManifest? _available;
  UpdateAsset? _selectedAsset;
  String? _error;
  DateTime? _lastCheckAt;
  double? _downloadProgress;
  File? _downloadedFile;
  bool _disposed = false;

  bool get initialized => _initialized;
  bool get checking => _checking;
  bool get downloading => _downloading;
  bool get automaticChecks => _settings?.automaticChecks ?? true;
  String get manifestUrl => _settings?.manifestUrl ?? kDefaultUpdateManifestUrl;
  AppVersion get currentVersion =>
      _currentVersion ?? const AppVersion(version: '未知', build: 0);
  DateTime? get lastCheckAt => _lastCheckAt;
  bool get lastCheckSucceeded => _lastCheckSucceeded;
  UpdateManifest? get availableUpdate => _available;
  UpdateAsset? get selectedAsset => _selectedAsset;
  String? get error => _error;
  double? get downloadProgress => _downloadProgress;
  File? get downloadedFile => _downloadedFile;
  UpdateTarget get target => _target;
  bool get updateAvailable => _available != null;
  bool get hasCompatibleAsset => _selectedAsset != null;

  String get menuSubtitle {
    if (updateAvailable) {
      final version = _available!.version;
      return '发现新版本 $version';
    }
    if (manifestUrl.trim().isEmpty) {
      return '版本 ${currentVersion.version} · 未配置更新源';
    }
    return '版本 ${currentVersion.version} · 检查与安装更新';
  }

  Future<void> initialize() async {
    if (_initialized || _initializing) return;
    _initializing = true;
    try {
      final settings = await _preferences.load();
      final current = await _versionLoader();
      _settings = settings;
      _currentVersion = current;
      _lastCheckAt = settings.lastCheckAt;
      _lastCheckSucceeded = settings.lastCheckSucceeded;
      _initialized = true;
      _notifyListeners();
    } on Object catch (error) {
      _error = _messageOf(error);
      _notifyListeners();
    } finally {
      _initializing = false;
    }
  }

  Future<bool> checkForUpdates({bool automatic = false}) async {
    await initialize();
    if (_checking || _downloading) return updateAvailable;
    final rawUrl = manifestUrl.trim();
    if (rawUrl.isEmpty) {
      if (!automatic) {
        _error = '请先配置更新清单地址';
        _notifyListeners();
      }
      return false;
    }
    final validation = validateUpdateManifestUrl(rawUrl);
    if (validation != null) {
      if (!automatic) {
        _error = validation;
        _notifyListeners();
      }
      return false;
    }
    _checking = true;
    _error = null;
    _notifyListeners();
    try {
      final decoded = await _backend.fetchManifest(Uri.parse(rawUrl));
      final manifest = UpdateManifest.fromJson(decoded);
      _available = isUpdateNewer(currentVersion, manifest) ? manifest : null;
      _selectedAsset = _available?.assetFor(_target);
      _downloadedFile = null;
      _downloadProgress = null;
      _lastCheckAt = _now();
      _lastCheckSucceeded = true;
      await _saveSettings(lastCheckAt: _lastCheckAt, lastCheckSucceeded: true);
      return updateAvailable;
    } on Object catch (error) {
      _lastCheckSucceeded = false;
      if (!automatic) _error = _messageOf(error);
      _lastCheckAt = _now();
      await _saveSettings(lastCheckAt: _lastCheckAt, lastCheckSucceeded: false);
      return false;
    } finally {
      _checking = false;
      _notifyListeners();
    }
  }

  /// 启动时调用；距离上次检查不足 24 小时则不访问网络。
  Future<bool> autoCheckIfDue() async {
    await initialize();
    if (!automaticChecks || manifestUrl.trim().isEmpty) return false;
    final last = _lastCheckAt;
    if (last != null &&
        _now().difference(last) < kAutomaticUpdateCheckInterval) {
      return updateAvailable;
    }
    return checkForUpdates(automatic: true);
  }

  Future<void> setAutomaticChecks(bool value) async {
    await initialize();
    await _saveSettings(automaticChecks: value);
  }

  Future<void> setManifestUrl(String raw) async {
    await initialize();
    final normalized = raw.trim();
    if (normalized.isNotEmpty) {
      final validation = validateUpdateManifestUrl(normalized);
      if (validation != null) {
        _error = validation;
        _notifyListeners();
        return;
      }
    }
    _error = null;
    _lastCheckSucceeded = false;
    _available = null;
    _selectedAsset = null;
    _downloadedFile = null;
    await _saveSettings(manifestUrl: normalized, clearLastCheck: true);
  }

  Future<void> restoreDefaultManifestUrl() async {
    await setManifestUrl(kDefaultUpdateManifestUrl);
  }

  Future<File> downloadAvailableUpdate() async {
    await initialize();
    final asset = _selectedAsset;
    if (asset == null) {
      throw StateError('当前平台没有可用的更新包');
    }
    if (_downloading) throw StateError('更新包正在下载');
    _downloading = true;
    _error = null;
    _downloadProgress = 0;
    _downloadedFile = null;
    _notifyListeners();
    File? target;
    try {
      final directory = await _downloadDirectory();
      final updates = Directory('${directory.path}/mgcs-updates');
      await updates.create(recursive: true);
      target = File('${updates.path}/${_safeFileName(asset.fileName)}');
      await _backend.download(
        asset.url,
        target,
        onProgress: (received, total) {
          if (!_downloading) return;
          _downloadProgress = total == null || total <= 0
              ? null
              : (received / total).clamp(0.0, 1.0);
          _notifyListeners();
        },
      );
      final size = await target.length();
      if (asset.size != null && size != asset.size) {
        await target.delete();
        target = null;
        throw FileSystemException('更新包大小不符：期望 ${asset.size} 字节，实际 $size 字节');
      }
      final actual = await _sha256OfFile(target);
      if (actual != asset.sha256) {
        await target.delete();
        target = null;
        throw const FileSystemException('更新包 SHA-256 校验失败');
      }
      _downloadedFile = target;
      _downloadProgress = 1;
      return target;
    } on Object catch (error) {
      if (target != null && await target.exists()) await target.delete();
      _error = _messageOf(error);
      rethrow;
    } finally {
      _downloading = false;
      _notifyListeners();
    }
  }

  Future<bool> openDownloadedUpdate() async {
    final file = _downloadedFile;
    if (file == null || !await file.exists()) {
      _error = '更新包尚未下载';
      _notifyListeners();
      throw const FileSystemException('更新包尚未下载');
    }
    _error = null;
    _notifyListeners();
    try {
      final opened = await _backend.openFile(file);
      if (!opened) {
        throw const FileSystemException('系统无法打开更新包');
      }
      return true;
    } on Object catch (error) {
      _error = _messageOf(error);
      _notifyListeners();
      rethrow;
    }
  }

  Future<void> _saveSettings({
    String? manifestUrl,
    bool? automaticChecks,
    DateTime? lastCheckAt,
    bool? lastCheckSucceeded,
    bool clearLastCheck = false,
  }) async {
    final current = _settings;
    final next = UpdatePreferences(
      manifestUrl:
          manifestUrl ?? current?.manifestUrl ?? kDefaultUpdateManifestUrl,
      automaticChecks: automaticChecks ?? current?.automaticChecks ?? true,
      lastCheckAt: clearLastCheck ? null : lastCheckAt ?? current?.lastCheckAt,
      lastCheckSucceeded: clearLastCheck
          ? false
          : lastCheckSucceeded ?? current?.lastCheckSucceeded ?? false,
    );
    try {
      await _preferences.save(next);
      _settings = next;
      _lastCheckAt = next.lastCheckAt;
      _lastCheckSucceeded = next.lastCheckSucceeded;
      _notifyListeners();
    } on Object catch (error) {
      _error = _messageOf(error);
      _notifyListeners();
      rethrow;
    }
  }

  static Future<AppVersion> _loadPackageVersion() async {
    final info = await PackageInfo.fromPlatform();
    return AppVersion(
      version: info.version,
      build: int.tryParse(info.buildNumber) ?? 0,
    );
  }

  static Future<String> _sha256OfFile(File file) async {
    final collector = _DigestCollector();
    final sink = sha256.startChunkedConversion(collector);
    await for (final chunk in file.openRead()) {
      sink.add(chunk);
    }
    sink.close();
    return collector.value.toString();
  }

  static String _safeFileName(String raw) {
    final base = raw.split(RegExp(r'[/\\]')).last.trim();
    final safe = base.replaceAll(RegExp(r'[<>:"|?*\x00-\x1f]'), '_');
    if (safe.isEmpty || safe == '.' || safe == '..') {
      return 'M-GCS-update';
    }
    return safe;
  }

  static String _messageOf(Object error) {
    if (error is FormatException) return error.message;
    if (error is HttpException) return error.message;
    if (error is FileSystemException) {
      return error.message.isEmpty ? error.toString() : error.message;
    }
    if (error is TimeoutException) return '网络请求超时';
    if (error is SocketException) {
      return error.message.isEmpty ? '无法连接更新服务器' : '无法连接更新服务器：${error.message}';
    }
    return error.toString().replaceFirst('Exception: ', '');
  }

  void _notifyListeners() {
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    _available = null;
    _selectedAsset = null;
    super.dispose();
  }
}

class _DigestCollector implements Sink<Digest> {
  Digest? value;

  @override
  void add(Digest data) => value = data;

  @override
  void close() {}
}
