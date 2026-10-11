/// 飞行记录（`.tlog`）录制与回放的原生边界。
///
/// 只暴露 `mg_tlog_*` 导出：链路录制（start/stop/status）与本地回放
/// （replay_open/replay_step/replay_seek/replay_close）。回放只读本地文件，
/// 与当前连接/车辆无关，断开状态下也能用。
abstract interface class TlogBackend {
  /// 当前连接是否可用于开始录制（native 侧录制挂在活动车辆上）。
  bool get isConnected;

  /// 读取链路录制状态（`mg_tlog_status`）。纯读取，没有副作用。
  Map<String, Object?> recordingStatus();

  /// 开始把链路上的 MAVLink 帧写入 [filepath]（`mg_tlog_start`）。
  Map<String, Object?> startRecording(String filepath);

  /// 停止录制并关闭文件（`mg_tlog_stop`）。
  Map<String, Object?> stopRecording();

  /// 打开一个本地 `.tlog` 做离线回放（`mg_tlog_replay_open`）。
  ///
  /// 会完整扫描文件以统计数据与时间范围，耗时与文件大小成正比。
  /// 每个 core 同时只有一个回放会话，再次打开会替换上一个。
  Map<String, Object?> replayOpen(String filepath);

  /// 从当前位置向后读至多 [maxMessages] 条记录（`mg_tlog_replay_step`）。
  Map<String, Object?> replayStep(int maxMessages);

  /// 按文件比例 [percent]（0..1）跳转（`mg_tlog_replay_seek`）。
  Map<String, Object?> replaySeek(double percent);

  /// 关闭回放会话并释放文件句柄（`mg_tlog_replay_close`）。
  Map<String, Object?> replayClose();
}

/// 原生库不可用时的空实现。
///
/// 让「飞行记录」页在 `initializationError != null`（例如 flutter_tester
/// 没有原生库）时仍能渲染骨架并给出确定的错误，而不是在构建期抛异常。
final class TlogUnavailableBackend implements TlogBackend {
  const TlogUnavailableBackend();

  @override
  bool get isConnected => false;

  Map<String, Object?> _fail() => const {
    'success': false,
    'error': {'code': -1, 'name': 'UNAVAILABLE', 'message': '原生飞行记录后端不可用'},
  };

  @override
  Map<String, Object?> recordingStatus() => _fail();

  @override
  Map<String, Object?> startRecording(String filepath) => _fail();

  @override
  Map<String, Object?> stopRecording() => _fail();

  @override
  Map<String, Object?> replayOpen(String filepath) => _fail();

  @override
  Map<String, Object?> replayStep(int maxMessages) => _fail();

  @override
  Map<String, Object?> replaySeek(double percent) => _fail();

  @override
  Map<String, Object?> replayClose() => _fail();
}
