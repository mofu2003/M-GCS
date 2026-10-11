import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../ffi_response.dart';
import 'log_controller.dart' show defaultLogDirectory;
import 'tlog_backend.dart';
import 'tlog_models.dart';

/// 每次「读取下一批」最多搬运的条数；与 native 的 `mg_tlog_replay_step` 上限一致。
const int kTlogReplayBatch = 200;

/// 界面保留的最近消息条数上限，防止长文件把内存吃满。
const int kTlogMaxRetainedMessages = 5000;

/// 飞行记录文件扩展名。
const String kTlogExtension = '.tlog';

/// 「飞行记录」页的状态机：链路 `.tlog` 录制 + 本地 `.tlog` 离线回放。
///
/// 录制挂在活动车辆上，未连接时拒绝开始；回放只读本地文件，与连接无关。
/// 两者的 native 副作用分别是「写文件」与「占用一个文件句柄」，所以状态由
/// 页面显式控制，不做任何成功推测。
class TlogController extends ChangeNotifier {
  TlogController({
    required this.backend,
    Future<Directory> Function()? directoryProvider,
    this.recordingPollInterval = const Duration(seconds: 1),
  }) : _directoryProvider = directoryProvider ?? defaultLogDirectory;

  final TlogBackend backend;
  final Future<Directory> Function() _directoryProvider;

  /// 录制中轮询 `mg_tlog_status` 的间隔，用来刷新已写字节数。
  final Duration recordingPollInterval;

  bool _disposed = false;

  TlogRecordingStatus _recording = TlogRecordingStatus.idle;
  String? _recordError;
  Timer? _recordingTimer;

  List<File> _localFiles = const [];
  bool _loadingFiles = false;
  String? _filesError;

  TlogReplayFileInfo? _session;
  final List<TlogReplayMessage> _messages = [];
  int _droppedMessages = 0;
  double _progress = 0;
  bool _eof = false;
  bool _truncated = false;
  bool _busy = false;
  String? _replayError;
  String _filter = '';

  // ---- 录制 ----

  TlogRecordingStatus get recording => _recording;
  bool get isRecording => _recording.recording;
  String? get recordError => _recordError;

  // ---- 本地文件 ----

  List<File> get localFiles => _localFiles;
  bool get loadingFiles => _loadingFiles;
  String? get filesError => _filesError;

  // ---- 回放 ----

  TlogReplayFileInfo? get session => _session;
  bool get hasSession => _session != null;
  bool get busy => _busy;
  bool get eof => _eof;
  bool get truncated => _truncated;
  double get progress => _progress;
  String? get replayError => _replayError;
  String get filter => _filter;

  /// 因超出保留上限而被丢弃的较早消息条数。
  int get droppedMessages => _droppedMessages;

  /// 已读到的消息（受 [kTlogMaxRetainedMessages] 限制，最近的在最后）。
  List<TlogReplayMessage> get messages =>
      List.unmodifiable(_messages);

  /// 按 [filter] 过滤后的消息。filter 为消息名子串或纯数字 msgid。
  List<TlogReplayMessage> get visibleMessages {
    final query = _filter.trim().toLowerCase();
    if (query.isEmpty) return List.unmodifiable(_messages);
    final asId = int.tryParse(query);
    return List.unmodifiable([
      for (final message in _messages)
        if (message.name.toLowerCase().contains(query) ||
            (asId != null && message.msgid == asId))
          message,
    ]);
  }

  Future<void> refreshRecordingStatus() async {
    if (_disposed) return;
    _applyRecordingStatus(backend.recordingStatus());
  }

  /// 开始一次链路录制。目标文件自动落在默认日志目录，文件名带时间戳。
  Future<void> startRecording() async {
    if (_disposed || _recording.recording) return;
    if (!backend.isConnected) {
      _recordError = '未连接飞控，无法开始录制';
      notifyListeners();
      return;
    }

    final Directory directory;
    try {
      directory = await _directoryProvider();
      await directory.create(recursive: true);
    } on Object catch (error) {
      _recordError = '无法访问日志目录：$error';
      notifyListeners();
      return;
    }
    if (_disposed) return;

    final file = _uniqueRecordingFile(directory);
    final response = backend.startRecording(file.path);
    if (!ffiSucceeded(response)) {
      _recordError = ffiErrorText(response, '开始录制失败');
      notifyListeners();
      return;
    }

    _recordError = null;
    _recording = TlogRecordingStatus(
      recording: true,
      file: file.path,
      bytes: 0,
    );
    notifyListeners();
    _startRecordingPolling();
    await refreshLocalFiles();
  }

  /// 停止录制并关闭文件。
  Future<void> stopRecording() async {
    if (_disposed || !_recording.recording) return;
    final response = backend.stopRecording();
    if (!ffiSucceeded(response)) {
      _recordError = ffiErrorText(response, '停止录制失败');
      notifyListeners();
      return;
    }
    _recordError = null;
    _stopRecordingPolling();
    await refreshRecordingStatus();
    await refreshLocalFiles();
  }

  /// 列出默认日志目录下的 `.tlog` 文件，最近修改的在前。
  Future<void> refreshLocalFiles() async {
    if (_disposed) return;
    _loadingFiles = true;
    _filesError = null;
    notifyListeners();

    final List<File> files;
    try {
      final directory = await _directoryProvider();
      if (!directory.existsSync()) {
        files = const [];
      } else {
        final found = <File>[];
        for (final entity in directory.listSync()) {
          if (entity is File &&
              entity.path.toLowerCase().endsWith(kTlogExtension)) {
            found.add(entity);
          }
        }
        found.sort((a, b) {
          final am = a.statSync().modified;
          final bm = b.statSync().modified;
          return bm.compareTo(am);
        });
        files = found;
      }
    } on Object catch (error) {
      _loadingFiles = false;
      _filesError = '读取日志目录失败：$error';
      notifyListeners();
      return;
    }
    if (_disposed) return;
    _localFiles = List.unmodifiable(files);
    _loadingFiles = false;
    notifyListeners();
  }

  /// 打开本地 `.tlog` 并读入第一批消息。
  Future<void> openReplay(String path) async {
    if (_disposed) return;
    final trimmed = path.trim();
    if (trimmed.isEmpty) {
      _replayError = '请输入或选择一个 .tlog 文件';
      notifyListeners();
      return;
    }

    final response = backend.replayOpen(trimmed);
    if (!ffiSucceeded(response)) {
      _replayError = ffiErrorText(response, '打开日志失败');
      notifyListeners();
      return;
    }

    final info = TlogReplayFileInfo.fromJson(response);
    _session = info;
    _messages.clear();
    _droppedMessages = 0;
    _progress = 0;
    _eof = false;
    _truncated = info.truncated;
    _filter = '';
    _replayError = null;
    notifyListeners();
    // 打开后先取一批，页面立刻有内容；空文件/损坏文件停在确定状态。
    await stepReplay();
  }

  /// 读取下一批消息；已到末尾时不再请求。
  Future<void> stepReplay() async {
    if (_disposed || _session == null || _busy || _eof) return;
    _busy = true;
    _replayError = null;
    notifyListeners();

    final response = backend.replayStep(kTlogReplayBatch);
    _busy = false;
    if (_disposed) return;
    if (!ffiSucceeded(response)) {
      _replayError = ffiErrorText(response, '读取日志失败');
      notifyListeners();
      return;
    }

    final batch = TlogReplayBatch.fromJson(response);
    _messages.addAll(batch.messages);
    if (_messages.length > kTlogMaxRetainedMessages) {
      final overflow = _messages.length - kTlogMaxRetainedMessages;
      _messages.removeRange(0, overflow);
      _droppedMessages += overflow;
    }
    _progress = batch.progress.clamp(0.0, 1.0);
    _eof = batch.eof;
    _truncated = _truncated || batch.truncated;
    notifyListeners();
  }

  /// 按文件比例 [percent]（0..1）跳转，并读入落点后的第一批。
  Future<void> seekReplay(double percent) async {
    if (_disposed || _session == null) return;
    final response = backend.replaySeek(percent);
    if (!ffiSucceeded(response)) {
      _replayError = ffiErrorText(response, '跳转失败');
      notifyListeners();
      return;
    }
    _messages.clear();
    _droppedMessages = 0;
    _eof = false;
    _replayError = null;
    _progress = (response['progress'] as num?)?.toDouble() ?? percent;
    notifyListeners();
    await stepReplay();
  }

  /// 关闭回放会话，释放 native 文件句柄。
  Future<void> closeReplay() async {
    if (_disposed || _session == null) return;
    backend.replayClose();
    _session = null;
    _messages.clear();
    _droppedMessages = 0;
    _progress = 0;
    _eof = false;
    _truncated = false;
    _replayError = null;
    _filter = '';
    notifyListeners();
  }

  /// 设置消息过滤条件。空串表示不过滤。
  void setFilter(String value) {
    if (_filter == value) return;
    _filter = value;
    notifyListeners();
  }

  void clearRecordError() {
    if (_recordError == null) return;
    _recordError = null;
    notifyListeners();
  }

  void clearReplayError() {
    if (_replayError == null) return;
    _replayError = null;
    notifyListeners();
  }

  void _applyRecordingStatus(Map<String, Object?> response) {
    if (response['success'] == false) {
      _recordError = ffiErrorText(response, '读取录制状态失败');
      _recording = TlogRecordingStatus.idle;
      _stopRecordingPolling();
      notifyListeners();
      return;
    }
    final status = TlogRecordingStatus.fromJson(response);
    _recording = status;
    notifyListeners();
    if (status.recording) {
      _startRecordingPolling();
    } else {
      _stopRecordingPolling();
    }
  }

  void _startRecordingPolling() {
    _recordingTimer ??= Timer.periodic(recordingPollInterval, (_) {
      if (_disposed) return;
      _applyRecordingStatus(backend.recordingStatus());
    });
  }

  void _stopRecordingPolling() {
    _recordingTimer?.cancel();
    _recordingTimer = null;
  }

  /// 录制文件名：`tlog_YYYYMMDD-HHMMSS.tlog`，已存在时追加 `-N`。
  File _uniqueRecordingFile(Directory directory) {
    final now = DateTime.now();
    String two(int value) => value.toString().padLeft(2, '0');
    final stamp =
        '${now.year.toString().padLeft(4, '0')}'
        '${two(now.month)}${two(now.day)}-'
        '${two(now.hour)}${two(now.minute)}${two(now.second)}';
    final separator = Platform.pathSeparator;
    final stem = 'tlog_$stamp';
    var candidate = File('${directory.path}$separator$stem$kTlogExtension');
    for (var index = 1; candidate.existsSync() && index < 1000; index++) {
      candidate = File(
        '${directory.path}$separator$stem-$index$kTlogExtension',
      );
    }
    return candidate;
  }

  @override
  void dispose() {
    _disposed = true;
    _stopRecordingPolling();
    // 释放 native 回放句柄，避免留下打开的文件。
    if (_session != null) {
      try {
        backend.replayClose();
      } on Object {
        // 原生库已卸载时忽略；dispose 不能再抛异常。
      }
    }
    super.dispose();
  }
}
