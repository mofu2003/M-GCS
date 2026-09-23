import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';

import '../../application/connection_session_backend.dart';
import '../../application/mission_models.dart';
import '../../application/mission_validator.dart';

enum MissionOperation {
  idle,
  downloading,
  uploading,
  clearing,
  settingCurrent,
  starting,
}

final class MissionEditorController extends ChangeNotifier {
  MissionEditorController({required this.backend, MissionValidator? validator})
    : _validator = validator ?? const MissionValidator();

  final MissionSessionBackend? backend;
  final MissionValidator _validator;
  List<MissionItemSnapshot> _items = const [];
  MissionOperation _operation = MissionOperation.idle;
  String _status = '尚未读取任务';
  int _currentIndex = 0;
  MissionProgressSnapshot _progress = const MissionProgressSnapshot();
  Timer? _progressTimer;

  List<MissionItemSnapshot> get items => _items;
  MissionOperation get operation => _operation;
  String get status => _status;
  int get currentIndex => _currentIndex;
  bool get busy => _operation != MissionOperation.idle;
  MissionProgressSnapshot get progress => _progress;
  MissionValidationResult get validation => _validator.validate(_items);

  void replacePlan(MissionPlanSnapshot plan) {
    _items = _renumber(plan.items);
    _currentIndex = plan.currentIndex.clamp(
      0,
      _items.isEmpty ? 0 : _items.length - 1,
    );
    _status = '已读取 ${_items.length} 个任务项';
    notifyListeners();
  }

  void addWaypoint() {
    final previous = _items.isEmpty ? null : _items.last;
    final item = MissionItemSnapshot(
      sequence: _items.length,
      command: 16,
      commandName: 'NAV_WAYPOINT',
      frame: 3,
      frameName: 'GLOBAL_REL_ALT',
      autoContinue: true,
      latitude: previous?.latitude ?? 0,
      longitude: previous?.longitude ?? 0,
      altitudeMeters: previous?.altitudeMeters ?? 50,
    );
    _items = [..._items, item];
    _status = '已添加本地航点，写入飞控后才会生效';
    notifyListeners();
  }

  /// Adds a waypoint from a map interaction. Coordinates are expected to be
  /// WGS-84, matching the mission domain model; map-specific conversion is
  /// handled by the map surface before this method is called.
  void addWaypointAt({
    required double latitude,
    required double longitude,
    double altitude = 50,
    double? speedMetersPerSecond,
  }) {
    final item = MissionItemSnapshot(
      sequence: _items.length,
      command: 16,
      commandName: 'NAV_WAYPOINT',
      frame: 3,
      frameName: 'GLOBAL_REL_ALT',
      autoContinue: true,
      latitude: latitude,
      longitude: longitude,
      altitudeMeters: altitude,
      speedMetersPerSecond: speedMetersPerSecond,
    );
    _items = [..._items, item];
    _currentIndex = item.sequence;
    _status = '已从地图添加航点，写入飞控后才会生效';
    notifyListeners();
  }

  void removeWaypoint(int index) {
    if (index < 0 || index >= _items.length) return;
    _items = _renumber([..._items]..removeAt(index));
    _currentIndex = _currentIndex.clamp(
      0,
      _items.isEmpty ? 0 : _items.length - 1,
    );
    _status = '已从本地任务移除航点';
    notifyListeners();
  }

  void moveWaypoint(int index, int delta) {
    final target = index + delta;
    if (index < 0 ||
        target < 0 ||
        index >= _items.length ||
        target >= _items.length) {
      return;
    }
    final next = [..._items];
    final item = next.removeAt(index);
    next.insert(target, item);
    _items = _renumber(next);
    _currentIndex = target;
    _status = '已调整航点顺序';
    notifyListeners();
  }

  void updateWaypoint(
    int index, {
    double? latitude,
    double? longitude,
    double? altitude,
    double? speedMetersPerSecond,
  }) {
    if (index < 0 || index >= _items.length) return;
    final item = _items[index];
    final next = [..._items]
      ..[index] = item.copyWith(
        latitude: latitude,
        longitude: longitude,
        altitudeMeters: altitude,
        speedMetersPerSecond: speedMetersPerSecond,
      );
    _items = next;
    notifyListeners();
  }

  /// Serializes the local mission draft. The format intentionally stays
  /// stable and human-readable so a different frontend can reuse it without
  /// depending on Flutter widgets or native bindings.
  String exportJson() {
    return const JsonEncoder.withIndent('  ').convert({
      'version': 1,
      'current_index': _currentIndex,
      'items': [
        for (final item in _items)
          {
            ...item.toTransferJson(),
            'command_name': item.commandName,
            'frame_name': item.frameName,
          },
      ],
    });
  }

  String exportCsv() {
    final rows = <String>[
      'seq,command,command_name,frame,frame_name,autocontinue,lat,lon,alt,hold_time,speed',
    ];
    for (final item in _items) {
      rows.add(
        [
          item.sequence,
          item.command,
          _csvField(item.commandName),
          item.frame,
          _csvField(item.frameName),
          item.autoContinue ? 1 : 0,
          item.latitude ?? '',
          item.longitude ?? '',
          item.altitudeMeters ?? '',
          item.holdTimeSeconds,
          item.speedMetersPerSecond ?? '',
        ].join(','),
      );
    }
    return '${rows.join('\n')}\n';
  }

  void importJson(String content) {
    final decoded = jsonDecode(content);
    final map = decoded is Map ? decoded : {'items': decoded};
    final rawItems = map['items'];
    if (rawItems is! List) throw const FormatException('JSON 中没有任务项');
    final items = rawItems
        .whereType<Map>()
        .map(_itemFromMap)
        .toList(growable: false);
    replacePlan(
      MissionPlanSnapshot(
        items: items,
        currentIndex: (map['current_index'] as num?)?.toInt() ?? 0,
        valid: true,
      ),
    );
    _status = '已导入 ${items.length} 个任务项';
    notifyListeners();
  }

  void importCsv(String content) {
    final lines = content
        .split(RegExp(r'\r?\n'))
        .map((line) => line.trim())
        .where((line) => line.isNotEmpty)
        .toList(growable: false);
    if (lines.length < 2) throw const FormatException('CSV 中没有任务项');
    final items = <MissionItemSnapshot>[];
    for (final line in lines.skip(1)) {
      final columns = _splitCsvLine(line);
      if (columns.length < 10) continue;
      items.add(
        MissionItemSnapshot(
          sequence: int.tryParse(columns[0]) ?? items.length,
          command: int.tryParse(columns[1]) ?? 16,
          commandName: columns[2],
          frame: int.tryParse(columns[3]) ?? 3,
          frameName: columns[4],
          autoContinue: columns[5] == '1' || columns[5].toLowerCase() == 'true',
          latitude: double.tryParse(columns[6]),
          longitude: double.tryParse(columns[7]),
          altitudeMeters: double.tryParse(columns[8]),
          holdTimeSeconds: double.tryParse(columns[9]) ?? 0,
          speedMetersPerSecond: columns.length > 10
              ? double.tryParse(columns[10])
              : null,
        ),
      );
    }
    if (items.isEmpty) throw const FormatException('CSV 中没有可读取的任务项');
    replacePlan(MissionPlanSnapshot(items: items, valid: true));
    _status = '已导入 ${items.length} 个任务项';
    notifyListeners();
  }

  Future<void> download() async {
    final service = backend;
    if (service == null) return _setStatus('当前后端不支持任务传输');
    await _run(MissionOperation.downloading, '正在从飞控读取航点…', () async {
      replacePlan(await service.downloadMission());
      await _waitForTransferCompletion();
    });
  }

  Future<void> upload() async {
    final service = backend;
    if (service == null) return _setStatus('当前后端不支持任务传输');
    final result = validation;
    if (_items.isEmpty) return _setStatus('请先添加至少一个航点');
    if (!result.isValid) return _setStatus(result.issues.first.message);
    await _run(MissionOperation.uploading, '正在写入飞控…', () async {
      final response = await service.uploadMission(_items);
      _setStatus(response.message);
      if (response.success) await _waitForTransferCompletion();
    });
  }

  Future<void> clear() async {
    final service = backend;
    if (service is! MissionControlSessionBackend) {
      return _setStatus('当前后端不支持清空任务');
    }
    await _run(MissionOperation.clearing, '正在清空飞控任务…', () async {
      final response = await service.clearMission();
      if (response.success) _items = const [];
      _setStatus(response.message);
      notifyListeners();
    });
  }

  Future<void> setCurrent(int index) async {
    final service = backend;
    if (service is! MissionControlSessionBackend) {
      return _setStatus('当前后端不支持设置当前航点');
    }
    if (index < 0 || index >= _items.length) return _setStatus('当前航点序号无效');
    await _run(MissionOperation.settingCurrent, '正在设置当前航点…', () async {
      final response = await service.setCurrentMission(index);
      if (response.success) _currentIndex = index;
      _setStatus(response.message);
    });
  }

  Future<void> start() async {
    final service = backend;
    if (service is! MissionControlSessionBackend) {
      return _setStatus('当前后端不支持开始任务');
    }
    if (!validation.isValid) return _setStatus(validation.issues.first.message);
    await _run(MissionOperation.starting, '正在开始任务…', () async {
      _setStatus((await service.startMission()).message);
    });
  }

  Future<void> generateSurvey(SurveyConfig config) async {
    final service = backend;
    if (service is! MissionPlanningSessionBackend) {
      return _setStatus('当前后端不支持测绘航线生成');
    }
    final planning = service as MissionPlanningSessionBackend;
    await _run(MissionOperation.uploading, '正在生成测绘航线…', () async {
      final result = await planning.generateSurvey(config);
      if (!result.valid) {
        _setStatus('测绘航线生成失败：${result.error}');
        return;
      }
      replacePlan(MissionPlanSnapshot(items: result.items, valid: true));
      _setStatus(
        '已生成 ${result.waypointCount} 个航点，${result.flightLineCount} 条航线',
      );
    });
  }

  Future<void> generateCorridor(CorridorConfig config) async {
    final service = backend;
    if (service is! MissionPlanningSessionBackend) {
      return _setStatus('当前后端不支持走廊航线生成');
    }
    final planning = service as MissionPlanningSessionBackend;
    await _run(MissionOperation.uploading, '正在生成走廊航线…', () async {
      final result = await planning.generateCorridor(config);
      if (!result.valid) {
        _setStatus('走廊航线生成失败：${result.error}');
        return;
      }
      replacePlan(MissionPlanSnapshot(items: result.items, valid: true));
      _setStatus('已生成 ${result.waypointCount} 个走廊航点');
    });
  }

  Future<void> refreshProgress() async {
    final service = backend;
    if (service == null || service is! MissionProgressSessionBackend) return;
    final progressBackend = service as MissionProgressSessionBackend;
    try {
      _progress = await progressBackend.loadMissionProgress();
      notifyListeners();
    } on Object {
      // Progress is auxiliary telemetry. A transient read failure must not
      // turn a completed mission operation into a failed operation.
    }
  }

  Future<void> _waitForTransferCompletion() async {
    final service = backend;
    if (service == null || service is! MissionProgressSessionBackend) return;
    var observed = false;
    var idleReads = 0;
    final deadline = DateTime.now().add(const Duration(seconds: 30));
    while (DateTime.now().isBefore(deadline)) {
      await refreshProgress();
      if (_progress.active) {
        observed = true;
        idleReads = 0;
      } else if (observed) {
        if (_progress.error.isNotEmpty) {
          _setStatus('任务传输失败：${_progress.error}');
        }
        return;
      } else if (++idleReads >= 3) {
        // Some adapters complete synchronously and only expose an idle state.
        return;
      }
      await Future<void>.delayed(const Duration(milliseconds: 150));
    }
    _setStatus('任务传输超时，请检查飞控链路后重试');
  }

  void _startProgressPolling() {
    if (backend is! MissionProgressSessionBackend) return;
    _progressTimer?.cancel();
    _progressTimer = Timer.periodic(const Duration(milliseconds: 180), (_) {
      unawaited(refreshProgress());
    });
  }

  Future<void> _stopProgressPolling() async {
    _progressTimer?.cancel();
    _progressTimer = null;
    await refreshProgress();
  }

  Future<void> _run(
    MissionOperation operation,
    String initialStatus,
    Future<void> Function() action,
  ) async {
    if (busy) return;
    _operation = operation;
    _status = initialStatus;
    _progress = const MissionProgressSnapshot();
    _startProgressPolling();
    notifyListeners();
    try {
      await action();
    } on Object catch (error) {
      _status = '操作失败：$error';
    } finally {
      await _stopProgressPolling();
      _operation = MissionOperation.idle;
      notifyListeners();
    }
  }

  void _setStatus(String value) {
    _status = value;
    notifyListeners();
  }

  static List<MissionItemSnapshot> _renumber(
    Iterable<MissionItemSnapshot> items,
  ) => [
    for (var i = 0; i < items.length; i++)
      items.elementAt(i).copyWith(sequence: i),
  ];

  static String _csvField(String value) =>
      value.contains(',') ? '"${value.replaceAll('"', '""')}"' : value;

  static List<String> _splitCsvLine(String line) {
    final values = <String>[];
    var buffer = StringBuffer();
    var quoted = false;
    for (var i = 0; i < line.length; i++) {
      final char = line[i];
      if (char == '"') {
        if (quoted && i + 1 < line.length && line[i + 1] == '"') {
          buffer.write('"');
          i++;
        } else {
          quoted = !quoted;
        }
      } else if (char == ',' && !quoted) {
        values.add(buffer.toString().trim());
        buffer = StringBuffer();
      } else {
        buffer.write(char);
      }
    }
    values.add(buffer.toString().trim());
    return values;
  }

  static MissionItemSnapshot _itemFromMap(Map<dynamic, dynamic> raw) {
    final command = (raw['command'] as num?)?.toInt() ?? 16;
    return MissionItemSnapshot(
      sequence: (raw['seq'] as num?)?.toInt() ?? 0,
      command: command,
      commandName: raw['command_name'] as String? ?? 'NAV_WAYPOINT',
      frame: (raw['frame'] as num?)?.toInt() ?? 3,
      frameName: raw['frame_name'] as String? ?? 'GLOBAL_REL_ALT',
      autoContinue: raw['autocontinue'] == true || raw['autocontinue'] == 1,
      latitude: (raw['lat'] as num?)?.toDouble(),
      longitude: (raw['lon'] as num?)?.toDouble(),
      altitudeMeters: (raw['alt'] as num?)?.toDouble(),
      holdTimeSeconds: (raw['hold_time'] as num?)?.toDouble() ?? 0,
      speedMetersPerSecond: (raw['speed'] as num?)?.toDouble(),
    );
  }

  @override
  void dispose() {
    _progressTimer?.cancel();
    _progressTimer = null;
    super.dispose();
  }
}
