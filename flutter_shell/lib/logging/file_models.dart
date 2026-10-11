/// MAVLink FTP 文件访问的领域模型。
///
/// 对应 FFI `mg_fs_transfer_status` 的 JSON 结构。这里只做解析，不解释语义；
/// 状态机在 [FileController]。
library;

/// 远程目录里的一条条目（`mg_fs_transfer_status` 的 `items[]`）。
class RemoteFileEntry {
  const RemoteFileEntry({
    required this.name,
    required this.isDirectory,
    required this.sizeBytes,
  });

  factory RemoteFileEntry.fromJson(Map<String, Object?> json) =>
      RemoteFileEntry(
        name: json['name'] as String? ?? '',
        isDirectory: json['directory'] == true,
        sizeBytes: (json['size_bytes'] as num?)?.toInt() ?? 0,
      );

  /// 条目名（不含目录前缀）。
  final String name;

  /// 是否为目录。
  final bool isDirectory;

  /// 文件字节数；目录一般为 0。
  final int sizeBytes;
}

/// 传输任务类型，取值对齐 native 的 `task` 字段。
enum FileTaskKind { idle, list, download, upload }

FileTaskKind fileTaskFromName(String? name) => switch (name) {
  'list' => FileTaskKind.list,
  'download' => FileTaskKind.download,
  'upload' => FileTaskKind.upload,
  _ => FileTaskKind.idle,
};

/// 单次传输的阶段，取值对齐 native 的 `state` 字段。
enum FileTransferPhase {
  idle,
  running,
  cancelling,
  completed,
  cancelled,
  failed,
}

FileTransferPhase fileTransferPhaseFromState(String? state) => switch (state) {
  'running' => FileTransferPhase.running,
  'cancelling' => FileTransferPhase.cancelling,
  'completed' => FileTransferPhase.completed,
  'cancelled' => FileTransferPhase.cancelled,
  'failed' => FileTransferPhase.failed,
  _ => FileTransferPhase.idle,
};

/// 一次 FTP 传输的状态快照。
class FileTransfer {
  const FileTransfer({
    required this.task,
    required this.phase,
    required this.remotePath,
    required this.localPath,
    required this.targetSys,
    required this.targetComponent,
    required this.receivedBytes,
    required this.totalBytes,
    required this.errorCode,
    required this.message,
    required this.items,
  });

  factory FileTransfer.fromStatus(Map<String, Object?> json) {
    final rawItems = json['items'];
    final items = <RemoteFileEntry>[
      if (rawItems is List)
        for (final item in rawItems)
          if (item is Map) RemoteFileEntry.fromJson(Map<String, Object?>.from(item)),
    ];
    return FileTransfer(
      task: fileTaskFromName(json['task'] as String?),
      phase: fileTransferPhaseFromState(json['state'] as String?),
      remotePath: json['remote_path'] as String? ?? '',
      localPath: json['local_path'] as String? ?? '',
      targetSys: (json['target_sys'] as num?)?.toInt() ?? 0,
      targetComponent: (json['target_component'] as num?)?.toInt() ?? 0,
      receivedBytes: (json['received_bytes'] as num?)?.toInt() ?? 0,
      totalBytes: (json['total_bytes'] as num?)?.toInt() ?? 0,
      errorCode: (json['error_code'] as num?)?.toInt() ?? 0,
      message: json['message'] as String? ?? '',
      items: items,
    );
  }

  /// 没有传输活动时的空快照。
  static const FileTransfer idle = FileTransfer(
    task: FileTaskKind.idle,
    phase: FileTransferPhase.idle,
    remotePath: '',
    localPath: '',
    targetSys: 0,
    targetComponent: 0,
    receivedBytes: 0,
    totalBytes: 0,
    errorCode: 0,
    message: '',
    items: [],
  );

  final FileTaskKind task;
  final FileTransferPhase phase;

  /// 目标机载路径（下载/列表的源、上传的目标）。
  final String remotePath;

  /// 本地路径（下载的目标、上传的源）。
  final String localPath;

  /// 传输绑定的目标 sysid/compid，用于确认结果属于当前焦点。
  final int targetSys;
  final int targetComponent;

  final int receivedBytes;
  final int totalBytes;
  final int errorCode;
  final String message;

  /// 目录列表结果；只有 `task == list` 时非空。
  final List<RemoteFileEntry> items;

  bool get isActive =>
      phase == FileTransferPhase.running ||
      phase == FileTransferPhase.cancelling;

  bool get isTerminal =>
      phase == FileTransferPhase.completed ||
      phase == FileTransferPhase.cancelled ||
      phase == FileTransferPhase.failed;

  double get progress {
    if (phase == FileTransferPhase.completed) return 1;
    if (totalBytes <= 0) return 0;
    return (receivedBytes / totalBytes).clamp(0.0, 1.0);
  }
}
