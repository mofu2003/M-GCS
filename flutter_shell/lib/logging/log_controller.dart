import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';

import '../ffi_response.dart';
import 'log_backend.dart';
import 'log_models.dart';

/// 机载日志下载默认落盘目录：应用支持目录下的 `logs/`。
///
/// 不引入文件选择器插件，先把日志放到可预期位置，并在界面显示完整路径。
Future<Directory> defaultLogDirectory() async {
  final base = await getApplicationSupportDirectory();
  return Directory('${base.path}${Platform.pathSeparator}logs');
}

/// 机载 DataFlash 日志页的状态机。
///
/// 负责三件事：把 `LOG_ENTRY` 列表收齐（用 `expected`/`complete` 判定，而不是
/// 靠“一段时间没新条目”猜）；发起下载并轮询 `mg_onboard_log_download_status`；
/// 把每次异步结果绑定到发起时的连接会话，会话切换后旧结果不得写回。
class LogController extends ChangeNotifier {
  LogController({
    required this.backend,
    Future<Directory> Function()? directoryProvider,
    this.statusPollInterval = const Duration(milliseconds: 200),
    this.listPollInterval = const Duration(milliseconds: 300),
    this.listTimeout = const Duration(seconds: 8),
  }) : _directoryProvider = directoryProvider ?? defaultLogDirectory;

  final LogBackend backend;
  final Future<Directory> Function() _directoryProvider;
  final Duration statusPollInterval;
  final Duration listPollInterval;
  final Duration listTimeout;

  List<OnboardLogEntry> _entries = const [];
  bool _loadingList = false;
  bool _listComplete = false;
  String? _listError;
  OnboardLogDownload? _download;
  String? _actionError;
  String? _actionMessage;

  /// 绑定到本控制器的连接会话；会话切换时清空所有状态，避免旧结果串台。
  int? _sessionId;
  bool _disposed = false;
  Timer? _statusTimer;
  int _statusPollGeneration = 0;

  List<OnboardLogEntry> get entries => _entries;
  bool get loadingList => _loadingList;

  /// 飞控声明的日志是否已全部收到。
  bool get listComplete => _listComplete;
  String? get listError => _listError;
  OnboardLogDownload? get download => _download;
  String? get actionError => _actionError;

  /// 擦除成功一类的信息提示（非错误），用完由页面调 [clearActionMessage] 清掉。
  String? get actionMessage => _actionMessage;
  bool get isDownloading => _download?.isActive ?? false;

  /// 绑定/切换连接会话。会话变化会丢弃列表与下载状态。
  void bindSession(int? sessionId) {
    if (_sessionId == sessionId) return;
    _sessionId = sessionId;
    _entries = const [];
    _listComplete = false;
    _listError = null;
    _download = null;
    _actionError = null;
    _actionMessage = null;
    _loadingList = false;
    _statusPollGeneration++;
    _statusTimer?.cancel();
    _statusTimer = null;
    notifyListeners();
  }

  /// 向飞控请求日志列表并轮询收齐。
  Future<void> refreshList() async {
    if (_disposed || _loadingList) return;
    final session = _sessionId;
    if (!backend.isConnected) {
      _listError = '未连接飞控，无法读取机载日志列表';
      notifyListeners();
      return;
    }

    _loadingList = true;
    _listError = null;
    _entries = const [];
    _listComplete = false;
    notifyListeners();

    final request = backend.requestList();
    if (!ffiSucceeded(request)) {
      _loadingList = false;
      _listError = ffiErrorText(request, '请求日志列表失败');
      notifyListeners();
      return;
    }

    final deadline = DateTime.now().add(listTimeout);
    var stablePolls = 0;
    var lastTotal = -1;
    while (!_disposed &&
        _sessionId == session &&
        DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(listPollInterval);
      if (_disposed || _sessionId != session) return;

      final Map<String, Object?> response;
      try {
        response = backend.entries();
      } on Object catch (error) {
        _loadingList = false;
        _listError = '读取日志列表失败：$error';
        notifyListeners();
        return;
      }
      if (response['success'] == false) {
        _loadingList = false;
        _listError = ffiErrorText(response, '读取日志列表失败');
        notifyListeners();
        return;
      }

      final rawLogs = response['logs'];
      final logs = rawLogs is List
          ? rawLogs
                .whereType<Map<Object?, Object?>>()
                .map(Map<String, Object?>.from)
                .toList(growable: false)
          : const <Map<String, Object?>>[];
      _entries = logs.map(OnboardLogEntry.fromJson).toList(growable: false);
      _listComplete = response['complete'] == true;
      // 让列表按编号倒序（最新的在前），与 QGC/MP 的默认视图一致。
      final sorted = [..._entries]..sort((a, b) => b.id.compareTo(a.id));
      _entries = List.unmodifiable(sorted);
      notifyListeners();

      if (_listComplete) break;

      // 飞控没有提供 num_logs 时的兜底：条目数稳定几轮后认为列表到齐。
      final total = _entries.length;
      if (total > 0 && total == lastTotal) {
        stablePolls++;
        if (stablePolls >= 4) break;
      } else {
        stablePolls = 0;
        lastTotal = total;
      }
    }

    _loadingList = false;
    if (!_listComplete &&
        _entries.isEmpty &&
        _listError == null &&
        !_disposed &&
        _sessionId == session) {
      _listError = '未收到机载日志列表（飞控未响应 LOG_ENTRY）';
    }
    notifyListeners();
  }

  /// 发起一条日志的下载。
  Future<void> downloadEntry(OnboardLogEntry entry) async {
    if (_disposed) return;
    if (!backend.isConnected) {
      _actionError = '未连接飞控，无法下载日志';
      notifyListeners();
      return;
    }
    if (isDownloading) {
      _actionError = '已有日志下载在进行，请先等待或取消';
      notifyListeners();
      return;
    }

    final session = _sessionId;
    final Directory directory;
    try {
      directory = await _directoryProvider();
      await directory.create(recursive: true);
    } on Object catch (error) {
      _actionError = '无法访问日志目录：$error';
      notifyListeners();
      return;
    }
    if (_disposed || _sessionId != session) return;

    final file = _uniqueFile(directory, _logFileName(entry));
    final response = backend.download(entry.id, file.path);
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '发起日志下载失败');
      notifyListeners();
      return;
    }

    _actionError = null;
    _download = OnboardLogDownload(
      logId: entry.id,
      phase: LogDownloadPhase.downloading,
      receivedBytes: 0,
      totalBytes: entry.sizeBytes,
      sizeDeclared: entry.sizeBytes > 0,
      savedBytes: 0,
      path: file.path,
      message: '',
    );
    notifyListeners();
    _startStatusPolling();
  }

  /// 取消正在进行的下载。
  Future<void> cancelDownload() async {
    if (_disposed || !isDownloading) return;
    final response = backend.cancel();
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '取消下载失败');
      notifyListeners();
      return;
    }
    _actionError = null;
    notifyListeners();
    // 终态由状态轮询确认（native 在 LOG_REQUEST_END 之后置 cancelled）。
  }

  /// 请求飞控擦除全部机载日志。
  ///
  /// 下载进行中或未连接时直接拒绝；成功后清空本地列表并给出提示，飞控擦除
  /// 是异步的，重新点「刷新列表」即可拿到新的（通常为空的）列表。
  Future<void> eraseAll() async {
    if (_disposed) return;
    if (!backend.isConnected) {
      _actionError = '未连接飞控，无法擦除日志';
      notifyListeners();
      return;
    }
    if (isDownloading) {
      _actionError = '有日志下载在进行，无法擦除';
      notifyListeners();
      return;
    }
    final response = backend.eraseAll();
    if (!ffiSucceeded(response)) {
      _actionError = ffiErrorText(response, '擦除日志失败');
      notifyListeners();
      return;
    }
    _entries = const [];
    _listComplete = false;
    _listError = null;
    _download = null;
    _actionError = null;
    _actionMessage = '已发送擦除请求。稍后点「刷新列表」查看飞控结果。';
    notifyListeners();
  }

  /// 清掉上一次动作的错误提示。
  void clearActionError() {
    if (_actionError == null) return;
    _actionError = null;
    notifyListeners();
  }

  /// 清掉擦除一类的信息提示。
  void clearActionMessage() {
    if (_actionMessage == null) return;
    _actionMessage = null;
    notifyListeners();
  }

  void _startStatusPolling() {
    final generation = ++_statusPollGeneration;
    final session = _sessionId;
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
        response = backend.status();
      } on Object catch (error) {
        timer.cancel();
        _download = _download?.copyWith(
          phase: LogDownloadPhase.failed,
          message: '读取下载状态失败：$error',
        );
        notifyListeners();
        return;
      }
      if (response['success'] == false) {
        timer.cancel();
        _download = _download?.copyWith(
          phase: LogDownloadPhase.failed,
          message: ffiErrorText(response, '读取下载状态失败'),
        );
        notifyListeners();
        return;
      }

      final status = OnboardLogDownload.fromStatus(response);
      final current = _download;
      // 状态里的 log_id 不属于本次下载时忽略，避免把别的下载结果写进来。
      if (current != null &&
          status.phase != LogDownloadPhase.idle &&
          status.logId != current.logId) {
        return;
      }

      if (status.phase == LogDownloadPhase.idle) {
        // 刚发起时可能还没落到状态里；连续多次 idle 视为下载丢失。
        idlePolls++;
        if (idlePolls >= 3) {
          timer.cancel();
          _download = current?.copyWith(
            phase: LogDownloadPhase.failed,
            message: '下载状态丢失或已被重置',
          );
          notifyListeners();
        }
        return;
      }

      idlePolls = 0;
      _download = status;
      notifyListeners();
      if (status.isTerminal) timer.cancel();
    });
  }

  String _logFileName(OnboardLogEntry entry) {
    final stamp = entry.timestamp;
    final suffix = stamp == null
        ? 'unknown'
        : '${stamp.year.toString().padLeft(4, '0')}'
              '${stamp.month.toString().padLeft(2, '0')}'
              '${stamp.day.toString().padLeft(2, '0')}-'
              '${stamp.hour.toString().padLeft(2, '0')}'
              '${stamp.minute.toString().padLeft(2, '0')}'
              '${stamp.second.toString().padLeft(2, '0')}';
    return 'log_${entry.id}_$suffix.bin';
  }

  /// 目标文件已存在时改用 `name-1.bin`、`name-2.bin`，不静默覆盖。
  File _uniqueFile(Directory directory, String fileName) {
    final separator = Platform.pathSeparator;
    var file = File('${directory.path}$separator$fileName');
    if (!file.existsSync()) return file;
    final dot = fileName.lastIndexOf('.');
    final stem = dot > 0 ? fileName.substring(0, dot) : fileName;
    final extension = dot > 0 ? fileName.substring(dot) : '';
    for (var index = 1; index < 1000; index++) {
      final candidate = File(
        '${directory.path}$separator$stem-$index$extension',
      );
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
