import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

/// 录制目录容量上限的默认值（MB），对齐 QGroundControl 的 `maxVideoSize`。
const kDefaultVideoLimitMegabytes = 10240;

/// 上限的下界：低于 100 MB 连一次短录制都放不下，没有实用意义。
const kMinVideoLimitMegabytes = 100;

/// 录制与截图的落盘设置。
///
/// 与 [MapPreferences] 同一套路：存成应用支持目录下的一个 JSON 文件，
/// 不依赖后端，也不碰飞控。`limitEnabled` 默认关闭，与 QGC 一致；
/// 打开后每次开始录制前会按「越旧越先删」把目录压回 [maxMegabytes] 以内。
class VideoStorageSettings {
  const VideoStorageSettings({
    required this.directory,
    this.limitEnabled = false,
    this.maxMegabytes = kDefaultVideoLimitMegabytes,
  });

  /// 录制与截图保存目录。由页面负责创建，控制器只使用它。
  final String directory;

  /// 是否启用容量上限清理。
  final bool limitEnabled;

  /// 目录内录制文件的总大小上限（MB）。
  final int maxMegabytes;

  static const _fileName = 'video_storage.json';

  VideoStorageSettings copyWith({
    String? directory,
    bool? limitEnabled,
    int? maxMegabytes,
  }) => VideoStorageSettings(
    directory: directory ?? this.directory,
    limitEnabled: limitEnabled ?? this.limitEnabled,
    maxMegabytes: maxMegabytes ?? this.maxMegabytes,
  );

  /// 读取落盘设置。缺失或损坏时退回「默认目录 + 不限制容量」。
  ///
  /// 平台通道不可用（例如纯 Dart/Widget 测试）时也退回临时目录，而不是抛异常，
  /// 让调用方永远拿到一份可用设置。
  static Future<VideoStorageSettings> load() async {
    final fallback = VideoStorageSettings(
      directory: await defaultRecordingDirectory(),
    );
    try {
      final file = await _file();
      if (!file.existsSync()) return fallback;
      final json = jsonDecode(await file.readAsString());
      if (json is! Map<String, Object?>) return fallback;
      final stored = (json['directory'] as String?)?.trim();
      return VideoStorageSettings(
        directory: (stored == null || stored.isEmpty)
            ? fallback.directory
            : stored,
        limitEnabled: json['limit_enabled'] == true,
        maxMegabytes: clampVideoLimit(
          (json['max_megabytes'] as num?)?.toInt() ?? fallback.maxMegabytes,
        ),
      );
    } on Object {
      // 读失败就用默认值，不打扰使用者。
      return fallback;
    }
  }

  Future<void> save() async {
    try {
      final file = await _file();
      await file.writeAsString(
        jsonEncode({
          'directory': directory,
          'limit_enabled': limitEnabled,
          'max_megabytes': maxMegabytes,
        }),
      );
    } on Object {
      // 写失败不影响本次使用。
    }
  }

  static Future<File> _file() async {
    final directory = await getApplicationSupportDirectory();
    return File('${directory.path}/$_fileName');
  }
}

/// 把上限收进合法区间；设置页与清理逻辑共用这一条规则。
int clampVideoLimit(int megabytes) =>
    megabytes.clamp(kMinVideoLimitMegabytes, 1 << 30);

/// 默认录制目录：应用支持目录下的 `recordings/`。
///
/// 平台通道不可用时退回系统临时目录（测试环境），避免把「读不到目录」当成异常。
Future<String> defaultRecordingDirectory() async {
  try {
    final directory = await getApplicationSupportDirectory();
    return '${directory.path}/recordings';
  } on Object {
    return '${Directory.systemTemp.path}/mgcs-recordings';
  }
}

/// 保证录制目录存在。返回 null 表示可用，否则是给使用者看的原因。
Future<String?> ensureDirectory(String path) async {
  try {
    final directory = Directory(path);
    if (!directory.existsSync()) directory.createSync(recursive: true);
    return null;
  } on Object catch (error) {
    return '无法创建录制目录：$error';
  }
}

/// 录制文件名：`mgcs-<本地时间>.mkv`。码流复制到 Matroska 容器，兼容性最好。
String recordingFileName(DateTime at) => 'mgcs-${_stamp(at)}.mkv';

/// 截图文件名：`mgcs-<本地时间>.png`。
String snapshotFileName(DateTime at) => 'mgcs-${_stamp(at)}.png';

String _stamp(DateTime at) {
  String two(int value) => value.toString().padLeft(2, '0');
  return '${at.year}${two(at.month)}${two(at.day)}'
      '-${two(at.hour)}${two(at.minute)}${two(at.second)}';
}

/// 会被容量清理统计/删除的录制文件后缀（小写）。
const _recordingExtensions = {'.mkv', '.mov', '.mp4', '.avi', '.ts', '.flv'};

/// 目录内所有录制文件，按修改时间从旧到新排序。
List<File> listRecordings(String directory) {
  final dir = Directory(directory);
  if (!dir.existsSync()) return const [];
  final files = <File>[];
  for (final entity in dir.listSync(followLinks: false)) {
    if (entity is! File) continue;
    final name = entity.path.toLowerCase();
    if (!_recordingExtensions.any(name.endsWith)) continue;
    files.add(entity);
  }
  files.sort((a, b) {
    final at = a.statSync().modified;
    final bt = b.statSync().modified;
    final byTime = at.compareTo(bt);
    return byTime != 0 ? byTime : a.path.compareTo(b.path);
  });
  return files;
}

/// 按「越旧越先删」清理录制目录，直到总大小不超过 [maxMegabytes]。
///
/// [keep] 是当前正在写入的文件，永不删除。返回删除的文件数；目录不存在、
/// 未设上限（`maxMegabytes <= 0`）或总量未超限时返回 0。删除失败会停止清理，
/// 避免把错误当成「已经腾出空间」。调用方应传入 [clampVideoLimit] 规范化后的值。
Future<int> pruneOldRecordings({
  required String directory,
  required int maxMegabytes,
  String? keep,
}) async {
  if (maxMegabytes <= 0) return 0;
  final files = listRecordings(directory);
  var total = 0;
  for (final file in files) {
    total += file.lengthSync();
  }
  final limitBytes = maxMegabytes * 1024 * 1024;
  var deleted = 0;
  for (final file in files) {
    if (total <= limitBytes) break;
    if (keep != null && file.path == keep) continue;
    final size = file.lengthSync();
    try {
      file.deleteSync();
    } on Object {
      break;
    }
    total -= size;
    deleted++;
  }
  return deleted;
}
