import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';

import '../ffi_response.dart';
import 'file_backend.dart';
import 'file_index.dart';
import 'file_models.dart';

/// 文件浏览默认下载目录：应用支持目录下的 `files/downloads/`。
///
/// 与机载日志一样不引入文件选择器插件，先把文件放到可预期位置并在界面显示完整路径。
Future<Directory> defaultDownloadDirectory() async {
  final base = await getApplicationSupportDirectory();
  return Directory(
    '${base.path}${Platform.pathSeparator}files'
    '${Platform.pathSeparator}downloads',
  );
}

/// 文件索引默认位置：应用支持目录下的 `files/`。
Future<Directory> defaultFileIndexDirectory() async {
  final base = await getApplicationSupportDirectory();
  return Directory('${base.path}${Platform.pathSeparator}files');
}

/// MAVLink FTP 文件浏览页的状态机。
///
/// 三种传输（列表/下载/上传）共用 native 的同一条 FTP 会话，所以控制器串行化：
/// 已有传输在跑时第二次发起直接拒绝，与 native 的 `MG_ERROR_BUSY` 一致。
/// 状态只来自 `mg_fs_transfer_status`，不由页面推测；会话切换后旧结果不写回。
class FileController extends ChangeNotifier {
  FileController({
    required this.backend,
    FileIndexStore? indexStore,
    Future<Directory> Function()? downloadDirectoryProvider,
    this.statusPollInterval = const Duration(milliseconds: 200),
    this.listTimeout = const Duration(seconds: 8),
  }) : _indexStore =
           indexStore ??
           FileIndexStore(defaultFileIndexDirectory),
       _downloadDirectoryProvider =
           downloadDirectoryProvider ?? defaultDownloadDirectory;

  /// MAVLink FTP 的起始目录。ArduPilot/PX4 都有 `@SYS`，比空路径更安全：
  /// FFI 明确拒绝空路径。
  static const String defaultPath = '@SYS';

  /// 方便快速跳转的常用目录。
  static const List<String> commonPaths = <String>['@SYS', '/APM/LOGS', '/'];

  final FileBackend backend;
  final FileIndexStore _indexStore;
  final Future<Directory> Function() _downloadDirectoryProvider;
  final Duration statusPollInterval;
  final Duration listTimeout;

  String _currentPath = defaultPath;
  List<RemoteFileEntry> _entries = const [];
  bool _listing = false;
  String? _listError;

  /// 当前展示的条目来自本地缓存而不是本次实时列表。
  bool _entriesFromCache = false;
  int _cacheUpdatedAtMs = 0;

  FileTransfer? _transfer;
  String? _actionError;
  String? _actionMessage;

  FileIndex? _index;
  bool _indexWriteFailed = false;

  int? _sessionId;
  bool _disposed = false;
  Timer? _statusTimer;
  int _statusPollGeneration = 0;

  String get currentPath => _currentPath;
  List<RemoteFileEntry> get entries => _entries;
  bool get listing => _listing;
  String? get listError => _listError;

  /// 条目是否来自缓存（离线/列表失败时的上次结果）。
  bool get entriesFromCache => _entriesFromCache;
  int get cacheUpdatedAtMs => _cacheUpdatedAtMs;

  FileTransfer? get transfer => _transfer;
  bool get isTransferring => _transfer?.isActive ?? false;
  String? get actionError => _actionError;
  String? get actionMessage => _actionMessage;

  /// 本地索引写入是否失败过（只提示，不阻断传输）。
  bool get indexWriteFailed => _indexWriteFailed;

  /// [remotePath] 的本地副本；没有则为 null。
  CachedDownload? localCopy(String remotePath) => _index?.download(remotePath);

  /// 当前路径的完整子路径。
  String childPath(String name) =>
      _currentPath.endsWith('/') ? '$_currentPath$name' : '$_currentPath/$name';

  /// 上一级目录；已经在根目录时返回 null。
  String? get parentPath {
    var path = _currentPath;
    if (path == '/' || path.isEmpty) return null;
    if (path.endsWith('/')) path = path.substring(0, path.length - 1);
    final slash = path.lastIndexOf('/');
    if (slash < 0) return '/';
    if (slash == 0) return '/';
    return path.substring(0, slash);
  }

  /// 绑定/切换连接会话。会话变化会丢弃列表与传输状态。
  void bindSession(int? sessionId) {
    if (_sessionId == sessionId) return;
    _sessionId = sessionId;
    _entries = const [];
    _entriesFromCache = false;
    _cacheUpdatedAtMs = 0;
    _listError = null;
    _listing = false;
    _transfer = null;
    _actionError = null;
    _actionMessage = null;
    _statusPollGeneration++;
    _statusTimer?.cancel();
    _statusTimer = null;
    notifyListeners();
    // 换会话后把缓存读出来，离线也能看到上次的目录内容。
    unawaited(_refreshIndexAndCache());
  }

  /// 读取本地索引并把当前路径的缓存填进来（仅在没有实时列表时）。
  Future<void> _refreshIndexAndCache() async {
    try {
      _index = await _indexStore.load();
    } on Object {
      _index = FileIndex();
    }
    if (_disposed) return;
    _indexWriteFailed = _indexStore.writeFailed;
    _applyCachedDirectory();
    notifyListeners();
  }

  void _applyCachedDirectory() {
    final cached = _index?.directory(_currentPath);
    if (cached == null) {
      if (_entriesFromCache) {
        _entries = const [];
        _entriesFromCache = false;
        _cacheUpdatedAtMs = 0;
      }
      return;
    }
    if (!_entriesFromCache && _entries.isNotEmpty) return;
    _entries = List.unmodifiable(cached.entries);
    _entriesFromCache = true;
    _cacheUpdatedAtMs = cached.updatedAtMs;
  }

  /// 列出 [path]（默认当前目录）并轮询到本次列表结束。
  Future<void> refreshList({String? path}) async {
    if (_disposed || _listing) return;
    if (path != null && path.isNotEmpty) _currentPath = path;
    final session = _sessionId;
    final targetPath = _currentPath;

    if (!backend.isConnected) {
      _listError = '未连接飞控，无法读取机载目录';
      _listing = false;
      // 断链时给出缓存内容，并明确标注它可能过期。
      _entriesFromCache = false;
      _entries = const [];
      _applyCachedDirectory();
      notifyListeners();
      return;
    }

    _listing = true;
    _listError = null;
    _entriesFromCache = false;
    _entries = const [];
    _cacheUpdatedAtMs = 0;
    notifyListeners();

    if (isTransferring) {
      _listing = false;
      _listError = '已有文件传输在进行，请先等待或取消';
      notifyListeners();
      return;
    }

    final start = backend.listDirectory(targetPath);
    if (!ffiSucceeded(start)) {
      _listing = false;
      _listError = ffiErrorText(start, '请求目录列表失败');
      _applyCachedDirectory();
      notifyListeners();
      return;
    }

    _transfer = FileTransfer(
      task: FileTaskKind.list,
      phase: FileTransferPhase.running,
      remotePath: targetPath,
      localPath: '',
      targetSys: 0,
      targetComponent: 0,
      receivedBytes: 0,
      totalBytes: 0,
      errorCode: 0,
      message: '',
      items: const [],
    );
    notifyListeners();

    final deadline = DateTime.now().add(listTimeout);
    var idlePolls = 0;
    while (!_disposed &&
        _sessionId == session &&
        DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(statusPollInterval);
      if (_disposed || _sessionId != session) return;

      final Map<String, Object?> response;
      try {
        response = backend.transferStatus();
      } on Object catch (error) {
        _listing = false;
        _listError = '读取目录状态失败：$error';
        _applyCachedDirectory();
        notifyListeners();
        return;
      }

      final status = FileTransfer.fromStatus(response);
      if (status.phase == FileTransferPhase.idle) {
        idlePolls++;
        if (idlePolls >= 5) {
          _listing = false;
          _listError = '目录列表状态丢失或已被重置';
          _applyCachedDirectory();
          notifyListeners();
          return;
        }
        continue;
      }
      idlePolls = 0;
      _transfer = status;
      notifyListeners();

      if (status.phase == FileTransferPhase.completed) {
        final sorted = List<RemoteFileEntry>.of(status.items)
          ..sort(_compareEntries);
        _entries = List.unmodifiable(sorted);
        _listing = false;
        _entriesFromCache = false;
        _cacheUpdatedAtMs = DateTime.now().millisecondsSinceEpoch;
        _actionError = null;
        notifyListeners();
        await _recordDirectory(targetPath, status.items);
        return;
      }
      if (status.phase == FileTransferPhase.failed ||
          status.phase == FileTransferPhase.cancelled) {
        _listing = false;
        _listError = status.phase == FileTransferPhase.cancelled
            ? '目录列表已取消'
            : _statusErrorText(status, '读取目录失败');
        _applyCachedDirectory();
        notifyListeners();
        return;
      }
    }

    if (_disposed || _sessionId != session) return;
    _listing = false;
    _listError = '目录列表超时（飞控未在 ${listTimeout.inSeconds} 秒内响应）';
    _applyCachedDirectory();
    notifyListeners();
  }

  /// 跳转到 [name] 子目录（用于目录条目）。
  Future<void> openDirectory(String name) => refreshList(path: childPath(name));

  /// 返回上一级目录。
  Future<void> goUp() async {
    final parent = parentPath;
    if (parent == null) return;
    await refreshList(path: parent);
  }

  /// 下载一个机载文件到默认下载目录。
  Future<void> downloadEntry(RemoteFileEntry entry) async {
    if (_disposed) return;
    if (entry.isDirectory) {
      _actionError = '目录需要先进入再选择文件';
      notifyListeners();
      return;
    }
    if (!backend.isConnected) {
      _actionError = '未连接飞控，无法下载文件';
      notifyListeners();
      return;
    }
    if (isTransferring) {
      _actionError = '已有文件传输在进行，请先等待或取消';
      notifyListeners();
      return;
    }

    final session = _sessionId;
    final remotePath = childPath(entry.name);
    final Directory directory;
    try {
      directory = await _downloadDirectoryProvider();
      await directory.create(recursive: true);
    } on Object catch (error) {
      _actionError = '无法访问下载目录：$error';
      notifyListeners();
      return;
    }
    if (_disposed || _sessionId != session) return;

    final file = _uniqueFile(directory, _safeFileName(entry.name));
    final response = backend.downloadFile(remotePath, file.path);
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '发起下载失败');
      notifyListeners();
      return;
    }

    _actionError = null;
    _actionMessage = null;
    _transfer = FileTransfer(
      task: FileTaskKind.download,
      phase: FileTransferPhase.running,
      remotePath: remotePath,
      localPath: file.path,
      targetSys: 0,
      targetComponent: 0,
      receivedBytes: 0,
      totalBytes: entry.sizeBytes,
      errorCode: 0,
      message: '',
      items: const [],
    );
    notifyListeners();
    _startTransferPolling();
  }

  /// 把本地文件 [localPath] 上传到当前机载目录下的 [remoteName]。
  Future<void> uploadFile(String localPath, {String? remoteName}) async {
    if (_disposed) return;
    if (!backend.isConnected) {
      _actionError = '未连接飞控，无法上传文件';
      notifyListeners();
      return;
    }
    if (isTransferring) {
      _actionError = '已有文件传输在进行，请先等待或取消';
      notifyListeners();
      return;
    }

    final trimmed = localPath.trim();
    if (trimmed.isEmpty) {
      _actionError = '请输入要上传的本地文件路径';
      notifyListeners();
      return;
    }
    final source = File(trimmed);
    if (!source.existsSync()) {
      _actionError = '本地文件不存在：$trimmed';
      notifyListeners();
      return;
    }

    final session = _sessionId;
    final name = (remoteName == null || remoteName.trim().isEmpty)
        ? _safeFileName(source.uri.pathSegments.isEmpty
              ? trimmed
              : source.uri.pathSegments.last)
        : remoteName.trim();
    final remotePath = childPath(name);
    final response = backend.uploadFile(trimmed, remotePath);
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '发起上传失败');
      notifyListeners();
      return;
    }

    _actionError = null;
    _actionMessage = null;
    _transfer = FileTransfer(
      task: FileTaskKind.upload,
      phase: FileTransferPhase.running,
      remotePath: remotePath,
      localPath: trimmed,
      targetSys: 0,
      targetComponent: 0,
      receivedBytes: 0,
      totalBytes: source.existsSync() ? source.lengthSync() : 0,
      errorCode: 0,
      message: '',
      items: const [],
    );
    notifyListeners();
    _startTransferPolling();
    if (_sessionId != session) return;
  }

  /// 取消正在进行的传输。终态由状态轮询确认（native 置 `cancelled`）。
  Future<void> cancelTransfer() async {
    if (_disposed || !isTransferring) return;
    final response = backend.cancel();
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '取消失败');
      notifyListeners();
      return;
    }
    _actionError = null;
    notifyListeners();
  }

  void clearActionError() {
    if (_actionError == null) return;
    _actionError = null;
    notifyListeners();
  }

  void clearActionMessage() {
    if (_actionMessage == null) return;
    _actionMessage = null;
    notifyListeners();
  }

  void _startTransferPolling() {
    final generation = ++_statusPollGeneration;
    final session = _sessionId;
    final expectedTask = _transfer?.task;
    _statusTimer?.cancel();
    var idlePolls = 0;
    _statusTimer = Timer.periodic(statusPollInterval, (timer) {
      if (_disposed ||
          generation != _statusPollGeneration ||
          _sessionId != session) {
        timer.cancel();
        return;
      }

      final Map<String, Object?> response;
      try {
        response = backend.transferStatus();
      } on Object catch (error) {
        timer.cancel();
        _failTransfer('读取传输状态失败：$error');
        return;
      }

      final status = FileTransfer.fromStatus(response);
      if (status.phase == FileTransferPhase.idle) {
        // 刚发起时状态可能还没落到 native 状态里；连续多次 idle 视为丢失。
        idlePolls++;
        if (idlePolls >= 5) {
          timer.cancel();
          _failTransfer('传输状态丢失或已被重置');
        }
        return;
      }
      idlePolls = 0;

      // 任务类型不匹配时忽略这一帧，避免把别的传输结果写成当前的。
      if (expectedTask != null && status.task != expectedTask) {
        return;
      }

      _transfer = status;
      notifyListeners();
      if (!status.isTerminal) return;

      timer.cancel();
      if (status.phase == FileTransferPhase.completed) {
        _actionError = null;
        switch (status.task) {
          case FileTaskKind.download:
            _actionMessage =
                '已下载 ${status.remotePath} → ${status.localPath}'
                '（${status.receivedBytes} 字节）';
            unawaited(_recordDownload(status));
          case FileTaskKind.upload:
            _actionMessage =
                '已上传 ${status.localPath} → ${status.remotePath}'
                '（${status.receivedBytes} 字节）';
          case FileTaskKind.list:
          case FileTaskKind.idle:
            break;
        }
        notifyListeners();
      } else if (status.phase == FileTransferPhase.cancelled) {
        _actionMessage = '传输已取消';
        notifyListeners();
      } else {
        _actionError = _statusErrorText(status, '传输失败');
        notifyListeners();
      }
    });
  }

  void _failTransfer(String message) {
    final current = _transfer;
    _actionError = message;
    if (current != null) {
      _transfer = FileTransfer(
        task: current.task,
        phase: FileTransferPhase.failed,
        remotePath: current.remotePath,
        localPath: current.localPath,
        targetSys: current.targetSys,
        targetComponent: current.targetComponent,
        receivedBytes: current.receivedBytes,
        totalBytes: current.totalBytes,
        errorCode: current.errorCode,
        message: message,
        items: current.items,
      );
    }
    notifyListeners();
  }

  Future<void> _recordDirectory(String remotePath, List<RemoteFileEntry> items) async {
    try {
      await _indexStore.recordDirectory(remotePath, items);
      _indexWriteFailed = _indexStore.writeFailed;
      _index = await _indexStore.load();
    } on Object {
      _indexWriteFailed = true;
    }
    if (_disposed) return;
    notifyListeners();
  }

  Future<void> _recordDownload(FileTransfer status) async {
    try {
      await _indexStore.recordDownload(
        status.remotePath,
        status.localPath,
        status.receivedBytes,
      );
      _indexWriteFailed = _indexStore.writeFailed;
      _index = await _indexStore.load();
    } on Object {
      _indexWriteFailed = true;
    }
    if (_disposed) return;
    notifyListeners();
  }

  String _statusErrorText(FileTransfer status, String fallback) {
    if (status.message.isNotEmpty) return status.message;
    if (status.errorCode != 0) return '$fallback（错误码 ${status.errorCode}）';
    return fallback;
  }

  /// 目录在前、文件在后，同类按名称排序，与 QGC/MP 的文件列表一致。
  static int _compareEntries(RemoteFileEntry a, RemoteFileEntry b) {
    if (a.isDirectory != b.isDirectory) return a.isDirectory ? -1 : 1;
    return a.name.toLowerCase().compareTo(b.name.toLowerCase());
  }

  /// 去掉路径分隔符，避免机载条目名逃出下载目录。
  static String _safeFileName(String name) {
    final cleaned = name
        .replaceAll('/', '_')
        .replaceAll('\\', '_')
        .replaceAll('..', '_');
    return cleaned.isEmpty ? 'download.bin' : cleaned;
  }

  /// 目标文件已存在时改用 `name-1.bin`、`name-2.bin`，不静默覆盖。
  File _uniqueFile(Directory directory, String fileName) {
    final separator = Platform.pathSeparator;
    final file = File('${directory.path}$separator$fileName');
    if (!file.existsSync()) return file;
    final dot = fileName.lastIndexOf('.');
    final stem = dot > 0 ? fileName.substring(0, dot) : fileName;
    final extension = dot > 0 ? fileName.substring(dot) : '';
    for (var index = 1; index < 1000; index++) {
      final candidate = File('${directory.path}$separator$stem-$index$extension');
      if (!candidate.existsSync()) return candidate;
    }
    return file;
  }

  @override
  void dispose() {
    _disposed = true;
    _statusPollGeneration++;
    _statusTimer?.cancel();
    _statusTimer = null;
    super.dispose();
  }
}
