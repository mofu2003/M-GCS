import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/logging/tlog_backend.dart';
import 'package:mgcs_frontend/logging/tlog_controller.dart';

/// 按脚本响应的替身后端：不接触 dart:ffi，用来驱动录制/回放状态机。
class _FakeTlogBackend implements TlogBackend {
  bool connected = true;

  int statusCalls = 0;
  Map<String, Object?> statusResponse = _ok({
    'recording': false,
    'file': '',
    'bytes': 0,
  });
  final List<Map<String, Object?>> statusQueue = [];

  int startCalls = 0;
  String? lastStartPath;
  Map<String, Object?> startResponse = _ok({'recording': true});

  int stopCalls = 0;
  Map<String, Object?> stopResponse = _ok({'recording': false});

  int openCalls = 0;
  String? lastOpenPath;
  Map<String, Object?> openResponse = _openInfo(
    file: '/tmp/a.tlog',
    bytes: 4096,
    messages: 10,
  );

  int stepCalls = 0;
  int lastStepMax = 0;
  final List<Map<String, Object?>> stepQueue = [];

  int seekCalls = 0;
  double? lastSeekPercent;
  Map<String, Object?> seekResponse = _ok({'progress': 0.5});

  int closeCalls = 0;

  @override
  bool get isConnected => connected;

  @override
  Map<String, Object?> recordingStatus() {
    statusCalls++;
    if (statusQueue.isEmpty) return statusResponse;
    return statusQueue.removeAt(0);
  }

  @override
  Map<String, Object?> startRecording(String filepath) {
    startCalls++;
    lastStartPath = filepath;
    return startResponse;
  }

  @override
  Map<String, Object?> stopRecording() {
    stopCalls++;
    return stopResponse;
  }

  @override
  Map<String, Object?> replayOpen(String filepath) {
    openCalls++;
    lastOpenPath = filepath;
    return openResponse;
  }

  @override
  Map<String, Object?> replayStep(int maxMessages) {
    stepCalls++;
    lastStepMax = maxMessages;
    if (stepQueue.isEmpty) return _batch(messages: const [], eof: true);
    return stepQueue.removeAt(0);
  }

  @override
  Map<String, Object?> replaySeek(double percent) {
    seekCalls++;
    lastSeekPercent = percent;
    return seekResponse;
  }

  @override
  Map<String, Object?> replayClose() {
    closeCalls++;
    return _ok();
  }
}

Map<String, Object?> _ok([Map<String, Object?> extra = const {}]) => {
  'success': true,
  ...extra,
};

Map<String, Object?> _fail([String message = 'boom']) => {
  'success': false,
  'error': {'code': -1, 'name': 'FAILED', 'message': message},
};

Map<String, Object?> _openInfo({
  String file = '/tmp/a.tlog',
  int bytes = 0,
  int messages = 0,
  int durationUs = 0,
  int firstTimestampUs = 0,
  int lastTimestampUs = 0,
  bool truncated = false,
}) => _ok({
  'file': file,
  'bytes': bytes,
  'messages': messages,
  'duration_us': durationUs,
  'first_timestamp_us': firstTimestampUs,
  'last_timestamp_us': lastTimestampUs,
  'truncated': truncated,
});

Map<String, Object?> _msg(
  int msgid,
  String name, {
  int sysid = 1,
  int compid = 1,
  int timestampUs = 0,
}) => {
  'msgid': msgid,
  'name': name,
  'sysid': sysid,
  'compid': compid,
  'timestamp_us': timestampUs,
};

Map<String, Object?> _batch({
  List<Map<String, Object?>> messages = const [],
  int? count,
  double progress = 0,
  int lastTimestampUs = 0,
  bool eof = false,
  bool truncated = false,
}) => _ok({
  'messages': messages,
  'count': count ?? messages.length,
  'progress': progress,
  'last_timestamp_us': lastTimestampUs,
  'eof': eof,
  'truncated': truncated,
});

Future<void> _waitFor(
  bool Function() predicate, {
  Duration timeout = const Duration(seconds: 3),
  String reason = '条件未在超时内满足',
}) async {
  final deadline = DateTime.now().add(timeout);
  while (!predicate()) {
    if (DateTime.now().isAfter(deadline)) fail(reason);
    await Future<void>.delayed(const Duration(milliseconds: 4));
  }
}

void main() {
  Directory? tempDir;
  late _FakeTlogBackend backend;
  TlogController? controller;

  setUp(() {
    backend = _FakeTlogBackend();
    tempDir = Directory.systemTemp.createTempSync('mgcs-tlog-test');
    controller = TlogController(
      backend: backend,
      directoryProvider: () async => tempDir!,
      recordingPollInterval: const Duration(milliseconds: 4),
    );
  });

  tearDown(() {
    controller?.dispose();
    controller = null;
    if (tempDir != null && tempDir!.existsSync()) {
      tempDir!.deleteSync(recursive: true);
    }
  });

  test('未连接时拒绝开始录制并给出错误', () async {
    backend.connected = false;
    await controller!.startRecording();
    expect(controller!.isRecording, isFalse);
    expect(controller!.recordError, isNotNull);
    expect(backend.startCalls, 0);
  });

  test('录制 start/stop 驱动状态并轮询字节数', () async {
    backend.statusQueue
      ..add(_ok({'recording': true, 'file': 'x', 'bytes': 100}))
      ..add(_ok({'recording': true, 'file': 'x', 'bytes': 200}));

    await controller!.startRecording();
    expect(backend.startCalls, 1);
    expect(backend.lastStartPath, isNotNull);
    expect(backend.lastStartPath!.endsWith(kTlogExtension), isTrue);
    expect(controller!.isRecording, isTrue);
    expect(controller!.recordError, isNull);

    await _waitFor(
      () => backend.statusCalls >= 2 && controller!.recording.bytes == 200,
      reason: '轮询未刷新字节数',
    );

    backend.statusResponse = _ok({'recording': false, 'file': '', 'bytes': 0});
    await controller!.stopRecording();
    expect(backend.stopCalls, 1);
    expect(controller!.isRecording, isFalse);
  });

  test('打开回放空路径与失败都给错误而不建会话', () async {
    await controller!.openReplay('   ');
    expect(controller!.hasSession, isFalse);
    expect(controller!.replayError, isNotNull);
    expect(backend.openCalls, 0);

    backend.openResponse = _fail('文件不存在');
    await controller!.openReplay('/tmp/missing.tlog');
    expect(controller!.hasSession, isFalse);
    expect(controller!.replayError, '文件不存在');
  });

  test('step 批量累积消息并在 eof 停止', () async {
    backend.openResponse = _openInfo(file: '/tmp/a.tlog', bytes: 100);
    backend.stepQueue
      ..add(_batch(messages: [_msg(1, 'HEARTBEAT')], progress: 0.5))
      ..add(
        _batch(
          messages: [_msg(33, 'GLOBAL_POSITION_INT')],
          progress: 1,
          eof: true,
        ),
      );

    await controller!.openReplay('/tmp/a.tlog');
    // openReplay 打开后自动取一批。
    expect(controller!.hasSession, isTrue);
    expect(controller!.messages.length, 1);
    expect(controller!.eof, isFalse);
    expect(backend.lastStepMax, kTlogReplayBatch);

    await controller!.stepReplay();
    expect(controller!.messages.length, 2);
    expect(controller!.eof, isTrue);
    expect(controller!.progress, 1);

    // eof 后再调用不再请求后端。
    final calls = backend.stepCalls;
    await controller!.stepReplay();
    expect(backend.stepCalls, calls);
  });

  test('filter 按消息名与纯数字 msgid 过滤', () async {
    backend.stepQueue.add(
      _batch(
        messages: [
          _msg(1, 'HEARTBEAT'),
          _msg(33, 'GLOBAL_POSITION_INT'),
          _msg(24, 'GPS_RAW_INT'),
        ],
        eof: true,
      ),
    );
    await controller!.openReplay('/tmp/a.tlog');
    expect(controller!.visibleMessages.length, 3);

    controller!.setFilter('gps');
    expect(controller!.visibleMessages.map((m) => m.name).toList(), [
      'GPS_RAW_INT',
    ]);

    controller!.setFilter('33');
    expect(controller!.visibleMessages.map((m) => m.msgid).toList(), [33]);

    controller!.setFilter('');
    expect(controller!.visibleMessages.length, 3);
  });

  test('seek 清空已读消息并重读落点后的批次', () async {
    backend.stepQueue
      ..add(_batch(messages: [_msg(1, 'HEARTBEAT')], progress: 0.1))
      ..add(_batch(messages: [_msg(2, 'SYS_STATUS')], progress: 0.6));
    await controller!.openReplay('/tmp/a.tlog');
    expect(controller!.messages.length, 1);

    await controller!.seekReplay(0.6);
    expect(backend.seekCalls, 1);
    expect(backend.lastSeekPercent, 0.6);
    expect(controller!.messages.map((m) => m.msgid).toList(), [2]);
    expect(controller!.eof, isFalse);
  });

  test('文件尾部截断会透传到状态', () async {
    backend.openResponse = _openInfo(file: '/tmp/a.tlog', truncated: true);
    backend.stepQueue.add(
      _batch(messages: const [], eof: true, truncated: true),
    );
    await controller!.openReplay('/tmp/a.tlog');
    expect(controller!.truncated, isTrue);
  });

  test('close 关闭会话并清空状态', () async {
    backend.stepQueue.add(_batch(messages: [_msg(1, 'HEARTBEAT')], eof: true));
    await controller!.openReplay('/tmp/a.tlog');
    controller!.setFilter('heart');
    expect(controller!.hasSession, isTrue);

    await controller!.closeReplay();
    expect(backend.closeCalls, 1);
    expect(controller!.hasSession, isFalse);
    expect(controller!.messages, isEmpty);
    expect(controller!.filter, '');
    expect(controller!.progress, 0);
  });

  test('超出保留上限时丢弃较早的消息', () async {
    final many = [
      for (var i = 0; i < kTlogMaxRetainedMessages + 200; i++)
        _msg(i, 'MSG_$i'),
    ];
    backend.stepQueue.add(_batch(messages: many, eof: true));
    await controller!.openReplay('/tmp/a.tlog');
    expect(controller!.messages.length, kTlogMaxRetainedMessages);
    expect(controller!.droppedMessages, 200);
    // 丢弃的是最早的，最新一条仍在。
    expect(controller!.messages.last.msgid, kTlogMaxRetainedMessages + 199);
  });

  test('dispose 会释放打开的回放句柄', () async {
    backend.stepQueue.add(_batch(messages: const [], eof: true));
    await controller!.openReplay('/tmp/a.tlog');
    expect(backend.closeCalls, 0);
    controller!.dispose();
    controller = null;
    expect(backend.closeCalls, 1);
  });
}
