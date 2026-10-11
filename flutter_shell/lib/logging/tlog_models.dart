/// 飞行记录（`.tlog`）录制与离线回放的领域模型。
///
/// 对应 FFI `mg_tlog_status`（录制）与 `mg_tlog_replay_*`（回放）的 JSON 结构。
/// 这里只做解析，不解释语义；状态机在 [TlogController]。
library;

/// `mg_tlog_status` 的链路录制状态。
class TlogRecordingStatus {
  const TlogRecordingStatus({
    required this.recording,
    required this.file,
    required this.bytes,
  });

  factory TlogRecordingStatus.fromJson(Map<String, Object?> json) =>
      TlogRecordingStatus(
        recording: json['recording'] == true,
        file: json['file'] as String? ?? '',
        bytes: (json['bytes'] as num?)?.toInt() ?? 0,
      );

  /// 未在录制。
  static const TlogRecordingStatus idle = TlogRecordingStatus(
    recording: false,
    file: '',
    bytes: 0,
  );

  final bool recording;

  /// 当前录制目标文件；未录制时为空串。
  final String file;

  /// 已写入磁盘的字节数。
  final int bytes;
}

/// `mg_tlog_replay_open` 返回的文件统计。
class TlogReplayFileInfo {
  const TlogReplayFileInfo({
    required this.file,
    required this.bytes,
    required this.messages,
    required this.durationUs,
    required this.firstTimestampUs,
    required this.lastTimestampUs,
    required this.truncated,
  });

  factory TlogReplayFileInfo.fromJson(Map<String, Object?> json) =>
      TlogReplayFileInfo(
        file: json['file'] as String? ?? '',
        bytes: (json['bytes'] as num?)?.toInt() ?? 0,
        messages: (json['messages'] as num?)?.toInt() ?? 0,
        durationUs: (json['duration_us'] as num?)?.toInt() ?? 0,
        firstTimestampUs: (json['first_timestamp_us'] as num?)?.toInt() ?? 0,
        lastTimestampUs: (json['last_timestamp_us'] as num?)?.toInt() ?? 0,
        truncated: json['truncated'] == true,
      );

  final String file;
  final int bytes;

  /// `open()` 扫描到的完整 MAVLink 帧数。
  final int messages;

  /// 首末记录时间差，微秒。
  final int durationUs;
  final int firstTimestampUs;
  final int lastTimestampUs;

  /// 文件尾部是否是半条记录（录制中途被杀/断电）。
  final bool truncated;

  /// 记录时长；负数或 0 一律按 0 处理。
  Duration get duration => Duration(
    microseconds: durationUs > 0 ? durationUs : 0,
  );
}

/// 回放读到的一条 MAVLink 消息。
class TlogReplayMessage {
  const TlogReplayMessage({
    required this.msgid,
    required this.name,
    required this.sysid,
    required this.compid,
    required this.timestampUs,
  });

  factory TlogReplayMessage.fromJson(Map<String, Object?> json) =>
      TlogReplayMessage(
        msgid: (json['msgid'] as num?)?.toInt() ?? 0,
        name: json['name'] as String? ?? 'UNKNOWN',
        sysid: (json['sysid'] as num?)?.toInt() ?? 0,
        compid: (json['compid'] as num?)?.toInt() ?? 0,
        timestampUs: (json['timestamp_us'] as num?)?.toInt() ?? 0,
      );

  final int msgid;

  /// MAVLink 消息名（`HEARTBEAT`/`GLOBAL_POSITION_INT`…）；未知消息为 `UNKNOWN`。
  final String name;
  final int sysid;
  final int compid;
  final int timestampUs;

  /// 记录时刻；0 表示该记录没有有效时间戳。
  DateTime? get timestamp => timestampUs > 0
      ? DateTime.fromMicrosecondsSinceEpoch(timestampUs, isUtc: true).toLocal()
      : null;
}

/// 一次 `mg_tlog_replay_step` 的结果。
class TlogReplayBatch {
  const TlogReplayBatch({
    required this.messages,
    required this.count,
    required this.progress,
    required this.lastTimestampUs,
    required this.eof,
    required this.truncated,
  });

  factory TlogReplayBatch.fromJson(Map<String, Object?> json) {
    final raw = json['messages'];
    final messages = raw is List
        ? raw
              .whereType<Map<Object?, Object?>>()
              .map(Map<String, Object?>.from)
              .map(TlogReplayMessage.fromJson)
              .toList(growable: false)
        : const <TlogReplayMessage>[];
    return TlogReplayBatch(
      messages: messages,
      count: (json['count'] as num?)?.toInt() ?? messages.length,
      progress: (json['progress'] as num?)?.toDouble() ?? 0,
      lastTimestampUs: (json['last_timestamp_us'] as num?)?.toInt() ?? 0,
      eof: json['eof'] == true,
      truncated: json['truncated'] == true,
    );
  }

  final List<TlogReplayMessage> messages;
  final int count;

  /// 已读字节占文件的比例，0..1。
  final double progress;
  final int lastTimestampUs;

  /// 已读到文件末尾。
  final bool eof;

  /// 读到末尾且末尾是半条记录（文件不完整）。
  final bool truncated;
}
