import 'dart:convert';
import 'dart:io';

import 'file_models.dart';

/// 本地文件索引：S3 的「本地索引/缓存」部分。
///
/// 机载目录列表是网络请求，断链或飞控慢时拿不到；下载过的文件也可能已经不在机载。
/// 这里把「上次成功列出的目录」与「已下载的远程→本地映射」写到应用支持目录，
/// 让文件浏览页在离线时仍能给出上次看到的内容（明确标注为缓存），
/// 并在条目上标出「本地已有副本」。
///
/// 索引只是缓存：它不参与任何飞行决策，写入失败只记录，不影响传输本身。
class CachedDirectory {
  const CachedDirectory({required this.updatedAtMs, required this.entries});

  factory CachedDirectory.fromJson(Map<String, Object?> json) => CachedDirectory(
    updatedAtMs: (json['updated_at_ms'] as num?)?.toInt() ?? 0,
    entries: <RemoteFileEntry>[
      if (json['entries'] is List)
        for (final item in json['entries'] as List)
          if (item is Map) RemoteFileEntry.fromJson(Map<String, Object?>.from(item)),
    ],
  );

  final int updatedAtMs;
  final List<RemoteFileEntry> entries;

  Map<String, Object?> toJson() => {
    'updated_at_ms': updatedAtMs,
    'entries': [
      for (final entry in entries)
        {
          'name': entry.name,
          'directory': entry.isDirectory,
          'size_bytes': entry.sizeBytes,
        },
    ],
  };
}

/// 一个已下载的远程文件的本地副本记录。
class CachedDownload {
  const CachedDownload({
    required this.localPath,
    required this.sizeBytes,
    required this.downloadedAtMs,
  });

  factory CachedDownload.fromJson(Map<String, Object?> json) => CachedDownload(
    localPath: json['local_path'] as String? ?? '',
    sizeBytes: (json['size_bytes'] as num?)?.toInt() ?? 0,
    downloadedAtMs: (json['downloaded_at_ms'] as num?)?.toInt() ?? 0,
  );

  final String localPath;
  final int sizeBytes;
  final int downloadedAtMs;

  Map<String, Object?> toJson() => {
    'local_path': localPath,
    'size_bytes': sizeBytes,
    'downloaded_at_ms': downloadedAtMs,
  };
}

/// 索引的内存表示与 JSON 编解码。磁盘读写由 [FileIndexStore] 负责。
class FileIndex {
  FileIndex({
    Map<String, CachedDirectory>? directories,
    Map<String, CachedDownload>? downloads,
  }) : directories = directories ?? <String, CachedDirectory>{},
       downloads = downloads ?? <String, CachedDownload>{};

  factory FileIndex.fromJson(Map<String, Object?> json) {
    final directories = <String, CachedDirectory>{};
    final rawDirectories = json['directories'];
    if (rawDirectories is Map) {
      for (final entry in rawDirectories.entries) {
        final value = entry.value;
        if (value is Map) {
          directories['${entry.key}'] = CachedDirectory.fromJson(
            Map<String, Object?>.from(value),
          );
        }
      }
    }
    final downloads = <String, CachedDownload>{};
    final rawDownloads = json['downloads'];
    if (rawDownloads is Map) {
      for (final entry in rawDownloads.entries) {
        final value = entry.value;
        if (value is Map) {
          downloads['${entry.key}'] = CachedDownload.fromJson(
            Map<String, Object?>.from(value),
          );
        }
      }
    }
    return FileIndex(directories: directories, downloads: downloads);
  }

  static const int schemaVersion = 1;

  final Map<String, CachedDirectory> directories;
  final Map<String, CachedDownload> downloads;

  /// 上次成功列出 [remotePath] 的结果；没有缓存时返回 null。
  CachedDirectory? directory(String remotePath) => directories[remotePath];

  /// [remotePath] 的本地副本记录；没有则返回 null。
  CachedDownload? download(String remotePath) => downloads[remotePath];

  Map<String, Object?> toJson() => {
    'version': schemaVersion,
    'directories': {
      for (final entry in directories.entries) entry.key: entry.value.toJson(),
    },
    'downloads': {
      for (final entry in downloads.entries) entry.key: entry.value.toJson(),
    },
  };
}

/// 索引的持久化：默认落在应用支持目录的 `files/file_index.json`。
class FileIndexStore {
  FileIndexStore(this._directoryProvider);

  final Future<Directory> Function() _directoryProvider;
  FileIndex? _cache;
  bool _writeFailed = false;

  /// 最近一次写入是否失败；页面据此提示「索引未能保存」，但不阻断传输。
  bool get writeFailed => _writeFailed;

  Future<File> _indexFile() async {
    final base = await _directoryProvider();
    return File('${base.path}${Platform.pathSeparator}file_index.json');
  }

  /// 读入索引；文件不存在或损坏时返回空索引，不抛异常。
  Future<FileIndex> load() async {
    if (_cache != null) return _cache!;
    try {
      final file = await _indexFile();
      if (!await file.exists()) return _cache = FileIndex();
      final decoded = jsonDecode(await file.readAsString());
      if (decoded is! Map) return _cache = FileIndex();
      return _cache = FileIndex.fromJson(Map<String, Object?>.from(decoded));
    } on Object {
      return _cache = FileIndex();
    }
  }

  /// 记录一次成功的目录列表。
  Future<void> recordDirectory(
    String remotePath,
    List<RemoteFileEntry> entries, {
    DateTime? now,
  }) async {
    final index = await load();
    index.directories[remotePath] = CachedDirectory(
      updatedAtMs: (now ?? DateTime.now()).millisecondsSinceEpoch,
      entries: entries,
    );
    await _persist(index);
  }

  /// 记录一个已完成的下载（远程路径 → 本地副本）。
  Future<void> recordDownload(
    String remotePath,
    String localPath,
    int sizeBytes, {
    DateTime? now,
  }) async {
    final index = await load();
    index.downloads[remotePath] = CachedDownload(
      localPath: localPath,
      sizeBytes: sizeBytes,
      downloadedAtMs: (now ?? DateTime.now()).millisecondsSinceEpoch,
    );
    await _persist(index);
  }

  Future<void> _persist(FileIndex index) async {
    try {
      final file = await _indexFile();
      await file.parent.create(recursive: true);
      await file.writeAsString(jsonEncode(index.toJson()));
      _writeFailed = false;
    } on Object {
      _writeFailed = true;
    }
  }
}
