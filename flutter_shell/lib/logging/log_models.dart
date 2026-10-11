/// 机载 DataFlash 日志的领域模型。
///
/// 对应 FFI `mg_onboard_log_get_entries` / `mg_onboard_log_download_status` 的
/// JSON 结构。这里只做解析，不解释语义；状态机在 [LogController]。
library;

/// 一条机载日志条目（`LOG_ENTRY`）。
class OnboardLogEntry {
  const OnboardLogEntry({
    required this.id,
    required this.timeUtc,
    required this.sizeBytes,
  });

  factory OnboardLogEntry.fromJson(Map<String, Object?> json) =>
      OnboardLogEntry(
        id: (json['id'] as num?)?.toInt() ?? 0,
        timeUtc: (json['time_utc'] as num?)?.toInt() ?? 0,
        sizeBytes: (json['size_bytes'] as num?)?.toInt() ?? 0,
      );

  /// 飞控分配的日志编号（`LOG_ENTRY.id`）。
  final int id;

  /// 日志记录时刻，UNIX 秒；0 表示飞控没给时间。
  final int timeUtc;

  /// 日志字节数（`LOG_ENTRY.size`）。
  final int sizeBytes;

  /// 本地时间的记录时刻；[timeUtc] 为 0 时返回 null。
  DateTime? get timestamp => timeUtc > 0
      ? DateTime.fromMillisecondsSinceEpoch(
          timeUtc * 1000,
          isUtc: true,
        ).toLocal()
      : null;

  @override
  String toString() => 'OnboardLogEntry(id: $id, size: $sizeBytes)';
}

/// 单次日志下载的阶段，取值对齐 native 的 `state` 字段。
enum LogDownloadPhase {
  idle,
  downloading,
  saving,
  completed,
  cancelled,
  failed,
}

LogDownloadPhase logDownloadPhaseFromState(String? state) => switch (state) {
  'downloading' => LogDownloadPhase.downloading,
  'saving' => LogDownloadPhase.saving,
  'completed' => LogDownloadPhase.completed,
  'cancelled' => LogDownloadPhase.cancelled,
  'failed' => LogDownloadPhase.failed,
  _ => LogDownloadPhase.idle,
};

/// 一次日志下载的状态快照。
class OnboardLogDownload {
  const OnboardLogDownload({
    required this.logId,
    required this.phase,
    required this.receivedBytes,
    required this.totalBytes,
    required this.sizeDeclared,
    required this.savedBytes,
    required this.path,
    required this.message,
  });

  factory OnboardLogDownload.fromStatus(Map<String, Object?> json) =>
      OnboardLogDownload(
        logId: (json['log_id'] as num?)?.toInt() ?? 0,
        phase: logDownloadPhaseFromState(json['state'] as String?),
        receivedBytes: (json['received_bytes'] as num?)?.toInt() ?? 0,
        totalBytes: (json['total_bytes'] as num?)?.toInt() ?? 0,
        sizeDeclared: json['size_declared'] == true,
        savedBytes: (json['saved_bytes'] as num?)?.toInt() ?? 0,
        path: json['path'] as String? ?? '',
        message: json['message'] as String? ?? '',
      );

  final int logId;
  final LogDownloadPhase phase;

  /// 已从飞控收到的字节数。
  final int receivedBytes;

  /// 目标总字节数（飞控 `LOG_ENTRY.size`），0 表示未知。
  final int totalBytes;

  /// 飞控是否在 `LOG_ENTRY` 里提供了非 0 大小。为 false 时下载仍能完成，
  /// 但完整性只能靠数据流推断，无法与飞控声明核对。
  final bool sizeDeclared;

  /// 实际写入本地文件的字节数（只有成功写盘后才非 0）。
  final int savedBytes;

  /// 目标文件路径。
  final String path;

  /// 失败原因或附加说明，正常为空。
  final String message;

  bool get isActive =>
      phase == LogDownloadPhase.downloading || phase == LogDownloadPhase.saving;

  bool get isTerminal =>
      phase == LogDownloadPhase.completed ||
      phase == LogDownloadPhase.cancelled ||
      phase == LogDownloadPhase.failed;

  double get progress {
    if (phase == LogDownloadPhase.completed) return 1;
    if (totalBytes <= 0) return 0;
    return (receivedBytes / totalBytes).clamp(0.0, 1.0);
  }

  /// 完成且飞控提供了声明大小时，落盘字节数已与 `LOG_ENTRY.size` 核对。
  bool get sizeVerified => phase == LogDownloadPhase.completed && sizeDeclared;

  OnboardLogDownload copyWith({LogDownloadPhase? phase, String? message}) =>
      OnboardLogDownload(
        logId: logId,
        phase: phase ?? this.phase,
        receivedBytes: receivedBytes,
        totalBytes: totalBytes,
        sizeDeclared: sizeDeclared,
        savedBytes: savedBytes,
        path: path,
        message: message ?? this.message,
      );
}
