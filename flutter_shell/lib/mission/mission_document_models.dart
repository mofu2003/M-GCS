typedef JsonMap = Map<String, Object?>;

const int kMavCmdNavTakeoff = 22;
const int kMavCmdNavVtolTakeoff = 84;
const int kMavCmdNavVtolLand = 85;

/// MAV_FRAME_GLOBAL_RELATIVE_ALT：高度相对 Home。任务项的默认高度基准。
const int kMissionFrameRelativeAlt = 3;

/// MAV_FRAME_GLOBAL_TERRAIN_ALT：高度相对地形（AGL），由飞控做地形跟随。
///
/// 与 QGC 的 `AltitudeFrameTerrain` 对应（`SimpleMissionItem.cc` 把该枚举映射到
/// `MAV_FRAME_GLOBAL_TERRAIN_ALT`）。写入这个帧时高度数值是离地高度；飞控需要有
/// 对应区域的地形数据才能按地形跟随执行。
const int kMissionFrameTerrainAlt = 10;

/// 高度基准的展示标签，供任务编辑与任务设置共用。
String missionFrameLabel(int frame) => switch (frame) {
  kMissionFrameTerrainAlt => '地形跟随 (AGL)',
  _ => '相对高度 (Home)',
};

/// 高度基准选择项（帧值 + 标签），顺序即界面顺序。
const List<(int, String)> kMissionFrameChoices = [
  (kMissionFrameRelativeAlt, '相对高度 (Home)'),
  (kMissionFrameTerrainAlt, '地形跟随 (AGL)'),
];

final class MissionDocumentMetadata {
  const MissionDocumentMetadata({
    this.id = '',
    this.name = 'Untitled Mission',
    this.description = '',
    this.firmwareProfile = 'ardupilot',
    this.vehicleType = 'copter',
  });

  final String id;
  final String name;
  final String description;
  final String firmwareProfile;
  final String vehicleType;

  factory MissionDocumentMetadata.fromJson(JsonMap json) =>
      MissionDocumentMetadata(
        id: json['id'] as String? ?? '',
        name: json['name'] as String? ?? 'Untitled Mission',
        description: json['description'] as String? ?? '',
        firmwareProfile: json['firmware_profile'] as String? ?? 'ardupilot',
        vehicleType: json['vehicle_type'] as String? ?? 'copter',
      );

  JsonMap toJson() => {
    'id': id,
    'name': name,
    'description': description,
    'firmware_profile': firmwareProfile,
    'vehicle_type': vehicleType,
  };
}

final class MissionHome {
  const MissionHome({
    this.set = false,
    this.latitude,
    this.longitude,
    this.altitudeMeters = 0,
  });

  final bool set;
  final double? latitude;
  final double? longitude;
  final double altitudeMeters;

  factory MissionHome.fromJson(JsonMap? json) => MissionHome(
    set: json?['set'] == true,
    latitude: (json?['lat'] as num?)?.toDouble(),
    longitude: (json?['lon'] as num?)?.toDouble(),
    altitudeMeters: (json?['alt_m'] as num?)?.toDouble() ?? 0,
  );

  JsonMap toJson() => {
    'set': set,
    if (latitude != null) 'lat': latitude,
    if (longitude != null) 'lon': longitude,
    'alt_m': altitudeMeters,
  };
}

final class MissionRallyPoint {
  const MissionRallyPoint({
    this.latitude,
    this.longitude,
    this.altitudeMeters = 0,
  });

  final double? latitude;
  final double? longitude;
  final double altitudeMeters;

  factory MissionRallyPoint.fromJson(JsonMap json) => MissionRallyPoint(
    latitude: (json['lat'] as num?)?.toDouble(),
    longitude: (json['lon'] as num?)?.toDouble(),
    altitudeMeters: (json['alt_m'] as num?)?.toDouble() ?? 0,
  );

  JsonMap toJson() => {
    if (latitude != null) 'lat': latitude,
    if (longitude != null) 'lon': longitude,
    'alt_m': altitudeMeters,
  };
}

final class MissionGeoPoint {
  const MissionGeoPoint({this.latitude, this.longitude});

  final double? latitude;
  final double? longitude;

  factory MissionGeoPoint.fromJson(JsonMap json) => MissionGeoPoint(
    latitude: (json['lat'] as num?)?.toDouble(),
    longitude: (json['lon'] as num?)?.toDouble(),
  );

  JsonMap toJson() => {
    if (latitude != null) 'lat': latitude,
    if (longitude != null) 'lon': longitude,
  };
}

final class MissionFencePolygon {
  const MissionFencePolygon({this.inclusion = true, this.vertices = const []});

  final bool inclusion;
  final List<MissionGeoPoint> vertices;

  factory MissionFencePolygon.fromJson(JsonMap json) => MissionFencePolygon(
    inclusion: json['inclusion'] != false,
    vertices: (json['vertices'] as List<Object?>? ?? const [])
        .map(_missionGeoPointFromJson)
        .whereType<MissionGeoPoint>()
        .toList(growable: false),
  );

  JsonMap toJson() => {
    'inclusion': inclusion,
    'vertices': [
      for (final vertex in vertices) [vertex.latitude, vertex.longitude],
    ],
  };
}

MissionGeoPoint? _missionGeoPointFromJson(Object? raw) {
  if (raw is List && raw.length >= 2) {
    return MissionGeoPoint(
      latitude: (raw[0] as num?)?.toDouble(),
      longitude: (raw[1] as num?)?.toDouble(),
    );
  }
  if (raw is JsonMap) return MissionGeoPoint.fromJson(raw);
  return null;
}

final class MissionFenceCircle {
  const MissionFenceCircle({
    this.inclusion = true,
    this.latitude,
    this.longitude,
    this.radiusMeters = 0,
  });

  final bool inclusion;
  final double? latitude;
  final double? longitude;
  final double radiusMeters;

  factory MissionFenceCircle.fromJson(JsonMap json) => MissionFenceCircle(
    inclusion: json['inclusion'] != false,
    latitude: (json['lat'] as num?)?.toDouble(),
    longitude: (json['lon'] as num?)?.toDouble(),
    radiusMeters: (json['radius_m'] as num?)?.toDouble() ?? 0,
  );

  JsonMap toJson() => {
    'inclusion': inclusion,
    if (latitude != null) 'lat': latitude,
    if (longitude != null) 'lon': longitude,
    'radius_m': radiusMeters,
  };
}

final class MissionGeoFence {
  const MissionGeoFence({
    this.polygons = const [],
    this.circles = const [],
    this.breachReturn,
    this.breachReturnAltitudeMeters = 0,
  });

  final List<MissionFencePolygon> polygons;
  final List<MissionFenceCircle> circles;
  final MissionGeoPoint? breachReturn;
  final double breachReturnAltitudeMeters;

  bool get isEmpty =>
      polygons.isEmpty && circles.isEmpty && breachReturn == null;

  factory MissionGeoFence.fromJson(JsonMap? json) {
    final breach = _mapOrNull(json?['breach_return']);
    return MissionGeoFence(
      polygons: (json?['polygons'] as List<Object?>? ?? const [])
          .whereType<JsonMap>()
          .map(MissionFencePolygon.fromJson)
          .toList(growable: false),
      circles: (json?['circles'] as List<Object?>? ?? const [])
          .whereType<JsonMap>()
          .map(MissionFenceCircle.fromJson)
          .toList(growable: false),
      breachReturn: breach == null ? null : MissionGeoPoint.fromJson(breach),
      breachReturnAltitudeMeters: (breach?['alt_m'] as num?)?.toDouble() ?? 0,
    );
  }

  JsonMap toJson() => {
    'polygons': [for (final polygon in polygons) polygon.toJson()],
    'circles': [for (final circle in circles) circle.toJson()],
    if (breachReturn != null)
      'breach_return': {
        ...breachReturn!.toJson(),
        'alt_m': breachReturnAltitudeMeters,
      },
  };
}

final class MissionDefaults {
  const MissionDefaults({
    this.frame = 3,
    this.altitudeMeters = 50,
    this.speedMetersPerSecond = 12,
  });

  final int frame;
  final double altitudeMeters;
  final double speedMetersPerSecond;

  factory MissionDefaults.fromJson(JsonMap? json) => MissionDefaults(
    frame: json?['frame'] as int? ?? 3,
    altitudeMeters: (json?['altitude_m'] as num?)?.toDouble() ?? 50,
    speedMetersPerSecond: (json?['speed_ms'] as num?)?.toDouble() ?? 12,
  );

  JsonMap toJson() => {
    'frame': frame,
    'altitude_m': altitudeMeters,
    'speed_ms': speedMetersPerSecond,
  };
}

final class MissionEntity {
  const MissionEntity({
    this.id = '',
    this.type = 'waypoint',
    this.label = '',
    this.enabled = true,
    this.order = 0,
    this.command = 16,
    this.frame = 3,
    this.autocontinue = 1,
    this.latitude,
    this.longitude,
    this.altitudeMeters,
    this.params = const [0, 0, 0, 0],
    this.notes = '',
    this.extensions = const {},
  });

  final String id;
  final String type;
  final String label;
  final bool enabled;
  final int order;
  final int command;
  final int frame;
  final int autocontinue;
  final double? latitude;
  final double? longitude;
  final double? altitudeMeters;
  final List<double> params;
  final String notes;

  /// 原生格式不认识的源字段（例如 QGC 的 `altitudeMode` /
  /// `AMSLAltAboveTerrain`），按原样透传，编辑后导出不丢用户数据。
  final JsonMap extensions;

  bool get isTakeoff =>
      command == kMavCmdNavTakeoff || command == kMavCmdNavVtolTakeoff;
  bool get isVtolTakeoff => command == kMavCmdNavVtolTakeoff;
  bool get isVtolLand => command == kMavCmdNavVtolLand;
  bool get isReturnToLaunch => command == 20;

  factory MissionEntity.fromJson(JsonMap json) => MissionEntity(
    id: json['id'] as String? ?? '',
    type: json['type'] as String? ?? 'waypoint',
    label: json['label'] as String? ?? '',
    enabled: json['enabled'] as bool? ?? true,
    order: json['order'] as int? ?? 0,
    command: json['command'] as int? ?? 16,
    frame: json['frame'] as int? ?? 3,
    autocontinue: json['autocontinue'] as int? ?? 1,
    latitude: (json['lat'] as num?)?.toDouble(),
    longitude: (json['lon'] as num?)?.toDouble(),
    altitudeMeters: (json['alt_m'] as num?)?.toDouble(),
    params: _doubleList(json['params'], fallback: const [0, 0, 0, 0]),
    notes: json['notes'] as String? ?? '',
    extensions: _map(json['extensions']),
  );

  MissionEntity copyWith({
    String? id,
    String? type,
    String? label,
    bool? enabled,
    int? order,
    int? command,
    int? frame,
    int? autocontinue,
    double? latitude,
    double? longitude,
    double? altitudeMeters,
    List<double>? params,
    String? notes,
    JsonMap? extensions,
  }) => MissionEntity(
    id: id ?? this.id,
    type: type ?? this.type,
    label: label ?? this.label,
    enabled: enabled ?? this.enabled,
    order: order ?? this.order,
    command: command ?? this.command,
    frame: frame ?? this.frame,
    autocontinue: autocontinue ?? this.autocontinue,
    latitude: latitude ?? this.latitude,
    longitude: longitude ?? this.longitude,
    altitudeMeters: altitudeMeters ?? this.altitudeMeters,
    params: params ?? this.params,
    notes: notes ?? this.notes,
    extensions: extensions ?? this.extensions,
  );

  JsonMap toJson() => {
    if (id.isNotEmpty) 'id': id,
    'type': type,
    if (label.isNotEmpty) 'label': label,
    'enabled': enabled,
    'order': order,
    'command': command,
    'frame': frame,
    'autocontinue': autocontinue,
    if (latitude != null) 'lat': latitude,
    if (longitude != null) 'lon': longitude,
    if (altitudeMeters != null) 'alt_m': altitudeMeters,
    'params': params,
    if (notes.isNotEmpty) 'notes': notes,
    if (extensions.isNotEmpty) 'extensions': extensions,
  };
}

/// QGC ComplexItem（Survey / CorridorScan 等）原样保留。
///
/// M-GCS 不解码其内部结构，只按原文透传：`json` 是源任务项的紧凑 JSON
/// 字符串，`entityInsertIndex` 记录它在简单实体之间的位置。
final class MissionComplexItem {
  const MissionComplexItem({
    this.id = '',
    this.itemType = '',
    this.json = '',
    this.entityInsertIndex = kComplexItemAppendAtEnd,
  });

  final String id;
  final String itemType;
  final String json;
  final int entityInsertIndex;

  factory MissionComplexItem.fromJson(JsonMap json) => MissionComplexItem(
    id: json['id'] as String? ?? '',
    itemType: json['item_type'] as String? ?? '',
    json: json['json'] as String? ?? '',
    entityInsertIndex:
        json['entity_insert_index'] as int? ?? kComplexItemAppendAtEnd,
  );

  JsonMap toJson() => {
    'id': id,
    'item_type': itemType,
    'json': json,
    'entity_insert_index': entityInsertIndex,
  };
}

/// 与 core 的 `kComplexItemAppendAtEnd` 哨兵一致：放在所有简单实体之后。
const int kComplexItemAppendAtEnd = 0xFFFFFFFF;

final class MissionDocument {
  const MissionDocument({
    this.schemaVersion = 1,
    this.metadata = const MissionDocumentMetadata(),
    this.home = const MissionHome(),
    this.rallyPoints = const [],
    this.geoFence = const MissionGeoFence(),
    this.defaults = const MissionDefaults(),
    this.revision = 1,
    this.updatedAtMs = 0,
    this.entities = const [],
    this.complexItems = const [],
    this.extensions = const {},
  });

  final int schemaVersion;
  final MissionDocumentMetadata metadata;
  final MissionHome home;
  final List<MissionRallyPoint> rallyPoints;
  final MissionGeoFence geoFence;
  final MissionDefaults defaults;
  final int revision;
  final int updatedAtMs;
  final List<MissionEntity> entities;
  final List<MissionComplexItem> complexItems;

  /// 原生格式保留的整体 JSON 片段（例如 QGC `.plan` 的未知顶层 / mission 段），
  /// 按键原样透传，避免 `replace` 时丢数据。
  final JsonMap extensions;

  factory MissionDocument.fromJson(JsonMap json) => MissionDocument(
    schemaVersion: json['schema_version'] as int? ?? 1,
    metadata: MissionDocumentMetadata.fromJson(_map(json['metadata'])),
    home: MissionHome.fromJson(_mapOrNull(json['home'])),
    rallyPoints: (json['rally_points'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map(MissionRallyPoint.fromJson)
        .toList(growable: false),
    geoFence: MissionGeoFence.fromJson(_mapOrNull(json['geo_fence'])),
    defaults: MissionDefaults.fromJson(_mapOrNull(json['defaults'])),
    revision: json['revision'] as int? ?? 1,
    updatedAtMs: json['updated_at_ms'] as int? ?? 0,
    entities: (json['entities'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map(MissionEntity.fromJson)
        .toList(growable: false),
    complexItems: (json['complex_items'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map(MissionComplexItem.fromJson)
        .toList(growable: false),
    extensions: _map(json['extensions']),
  );

  JsonMap toJson() => {
    'schema_version': schemaVersion,
    'metadata': metadata.toJson(),
    'home': home.toJson(),
    if (rallyPoints.isNotEmpty)
      'rally_points': [for (final point in rallyPoints) point.toJson()],
    if (!geoFence.isEmpty) 'geo_fence': geoFence.toJson(),
    'defaults': defaults.toJson(),
    'revision': revision,
    'updated_at_ms': updatedAtMs,
    'entities': [for (final entity in entities) entity.toJson()],
    if (complexItems.isNotEmpty)
      'complex_items': [for (final item in complexItems) item.toJson()],
    if (extensions.isNotEmpty) 'extensions': extensions,
  };

  /// 只替换需要改的字段；`replace` 操作会用整份文档覆盖后端，
  /// 所以 revision / updatedAtMs 必须原样带过去。
  MissionDocument copyWith({
    MissionDocumentMetadata? metadata,
    MissionHome? home,
    List<MissionRallyPoint>? rallyPoints,
    MissionGeoFence? geoFence,
    MissionDefaults? defaults,
    int? revision,
    int? updatedAtMs,
    List<MissionEntity>? entities,
    List<MissionComplexItem>? complexItems,
    JsonMap? extensions,
  }) => MissionDocument(
    schemaVersion: schemaVersion,
    metadata: metadata ?? this.metadata,
    home: home ?? this.home,
    rallyPoints: rallyPoints ?? this.rallyPoints,
    geoFence: geoFence ?? this.geoFence,
    defaults: defaults ?? this.defaults,
    revision: revision ?? this.revision,
    updatedAtMs: updatedAtMs ?? this.updatedAtMs,
    entities: entities ?? this.entities,
    complexItems: complexItems ?? this.complexItems,
    extensions: extensions ?? this.extensions,
  );
}

enum MissionDiagnosticSeverity { info, warning, error }

final class MissionDiagnostic {
  const MissionDiagnostic({
    required this.code,
    required this.severity,
    required this.message,
    this.entityId = '',
    this.field = '',
  });

  final String code;
  final MissionDiagnosticSeverity severity;
  final String message;
  final String entityId;
  final String field;

  factory MissionDiagnostic.fromJson(JsonMap json) => MissionDiagnostic(
    code: json['code'] as String? ?? '',
    severity: switch (json['severity']) {
      'info' => MissionDiagnosticSeverity.info,
      'warning' => MissionDiagnosticSeverity.warning,
      _ => MissionDiagnosticSeverity.error,
    },
    message: json['message'] as String? ?? '',
    entityId: json['entity_id'] as String? ?? '',
    field: json['field'] as String? ?? '',
  );
}

final class MissionValidationReport {
  const MissionValidationReport({
    this.valid = false,
    this.diagnostics = const [],
  });

  final bool valid;
  final List<MissionDiagnostic> diagnostics;

  factory MissionValidationReport.fromJson(JsonMap json) =>
      MissionValidationReport(
        valid: json['valid'] == true,
        diagnostics: (json['diagnostics'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(MissionDiagnostic.fromJson)
            .toList(growable: false),
      );
}

final class MissionEstimate {
  const MissionEstimate({
    this.totalDistanceMeters = 0,
    this.estimatedTimeSeconds = 0,
    this.minAltitudeMeters = 0,
    this.maxAltitudeMeters = 0,
    this.photoCount = 0,
    this.waypointCount = 0,
    this.commandCount = 0,
    this.enabledEntityCount = 0,
  });

  final double totalDistanceMeters;
  final double estimatedTimeSeconds;
  final double minAltitudeMeters;
  final double maxAltitudeMeters;
  final int photoCount;
  final int waypointCount;
  final int commandCount;
  final int enabledEntityCount;

  factory MissionEstimate.fromJson(JsonMap json) => MissionEstimate(
    totalDistanceMeters: (json['total_distance_m'] as num?)?.toDouble() ?? 0,
    estimatedTimeSeconds: (json['estimated_time_s'] as num?)?.toDouble() ?? 0,
    minAltitudeMeters: (json['min_altitude_m'] as num?)?.toDouble() ?? 0,
    maxAltitudeMeters: (json['max_altitude_m'] as num?)?.toDouble() ?? 0,
    photoCount: json['photo_count'] as int? ?? 0,
    waypointCount: json['waypoint_count'] as int? ?? 0,
    commandCount: json['command_count'] as int? ?? 0,
    enabledEntityCount: json['enabled_entity_count'] as int? ?? 0,
  );
}

final class MissionCompileResult {
  const MissionCompileResult({
    this.valid = false,
    this.documentRevision = 0,
    this.plan = const {},
    this.sourceEntityIds = const [],
    this.report = const MissionValidationReport(),
  });

  final bool valid;
  final int documentRevision;
  final JsonMap plan;
  final List<String> sourceEntityIds;
  final MissionValidationReport report;

  factory MissionCompileResult.fromJson(JsonMap json) => MissionCompileResult(
    valid: json['valid'] == true,
    documentRevision: json['document_revision'] as int? ?? 0,
    plan: _map(json['plan']),
    sourceEntityIds: (json['source_entity_ids'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map((item) => item['id'] as String? ?? '')
        .where((id) => id.isNotEmpty)
        .toList(growable: false),
    report: MissionValidationReport.fromJson({
      'valid': json['valid'],
      'diagnostics': json['diagnostics'],
    }),
  );
}

final class MissionDocumentOperationResult {
  const MissionDocumentOperationResult({
    required this.document,
    required this.revision,
  });

  final MissionDocument document;
  final int revision;
}

enum MissionTransferState { idle, downloading, uploading, waitingAck }

/// 任务执行进度：`MISSION_CURRENT` 给出当前目标项，`MISSION_ITEM_REACHED` 统计已到达项。
/// `currentIndex` 是去掉 ArduPilot home 偏移后的客户端可见序号（0 起）。
final class MissionExecutionProgress {
  const MissionExecutionProgress({
    this.valid = false,
    this.currentIndex = 0,
    this.currentSeq = 0,
    this.total = 0,
    this.reachedCount = 0,
    this.lastReachedSeq = 0,
    this.reachedEnd = false,
    this.aborted = false,
    this.finished = false,
    this.ended = false,
  });

  final bool valid;
  final int currentIndex;
  final int currentSeq;
  final int total;
  final int reachedCount;
  final int lastReachedSeq;

  /// 航段飞完：最后一项已到达。固定翼此时还在返航/降落，不等于任务完成。
  final bool reachedEnd;

  /// 末项已到达，且本轮参与任务的飞行已经上锁结束。
  final bool finished;

  /// 飞行已结束，可能尚未完成任务。
  final bool ended;

  /// 任务当前中断：离开任务模式后尚未恢复，且最后一项未到达。
  final bool aborted;

  /// 有可判断的飞控任务时才展示进度。
  bool get active => valid && total > 0;

  /// 当前目标推进也更新显示，不依赖可能丢失的单次到达通知。
  double get fraction {
    if (!active) return 0;
    if (finished || reachedEnd) return 1;
    return (currentIndex / total).clamp(0.0, 1.0);
  }

  factory MissionExecutionProgress.fromJson(JsonMap json) =>
      MissionExecutionProgress(
        valid: json['valid'] == true,
        currentIndex: (json['current_index'] as num?)?.toInt() ?? 0,
        currentSeq: (json['current_seq'] as num?)?.toInt() ?? 0,
        total: (json['total'] as num?)?.toInt() ?? 0,
        reachedCount: (json['reached_count'] as num?)?.toInt() ?? 0,
        lastReachedSeq: (json['last_reached_seq'] as num?)?.toInt() ?? 0,
        reachedEnd: json['reached_end'] == true,
        aborted: json['aborted'] == true,
        finished: json['finished'] == true,
        ended: json['ended'] == true,
      );
}

final class MissionTransferProgress {
  const MissionTransferProgress({
    this.state = MissionTransferState.idle,
    this.progressPercent = 0,
    this.received = 0,
    this.sent = 0,
    this.total = 0,
    this.cancellable = false,
    this.retryCount = 0,
    this.result = 0,
    this.cancelled = false,
    this.error = '',
  });

  final MissionTransferState state;
  final int progressPercent;
  final int received;
  final int sent;
  final int total;
  final bool cancellable;
  final int retryCount;
  final int result;
  final bool cancelled;
  final String error;

  bool get active => switch (state) {
    MissionTransferState.downloading ||
    MissionTransferState.uploading ||
    MissionTransferState.waitingAck => true,
    MissionTransferState.idle => false,
  };

  factory MissionTransferProgress.fromJson(JsonMap json) =>
      MissionTransferProgress(
        state: switch (json['state']) {
          'downloading' => MissionTransferState.downloading,
          'uploading' => MissionTransferState.uploading,
          'waiting_ack' => MissionTransferState.waitingAck,
          _ => MissionTransferState.idle,
        },
        progressPercent: (json['progress_pct'] as num?)?.toInt() ?? 0,
        received: (json['received'] as num?)?.toInt() ?? 0,
        sent: (json['sent'] as num?)?.toInt() ?? 0,
        total: (json['total'] as num?)?.toInt() ?? 0,
        cancellable: json['cancellable'] == true,
        retryCount: (json['retry_count'] as num?)?.toInt() ?? 0,
        result: (json['result'] as num?)?.toInt() ?? 0,
        cancelled: json['cancelled'] == true,
        error: json['error'] as String? ?? '',
      );
}

final class MissionVerificationDifference {
  const MissionVerificationDifference({
    required this.index,
    required this.field,
    required this.expected,
    required this.actual,
    this.reason = '',
    this.area = '',
  });

  final int index;
  final String field;
  final String expected;
  final String actual;
  final String reason;

  /// 围栏/Rally 核对时后端给出的区段名（如 `polygon[0]`、`rally[1]`）。
  final String area;

  factory MissionVerificationDifference.fromJson(JsonMap json) =>
      MissionVerificationDifference(
        index: (json['index'] as num?)?.toInt() ?? -1,
        field: json['field'] as String? ?? '',
        expected: json['expected']?.toString() ?? '',
        actual: json['actual']?.toString() ?? '',
        reason: json['reason']?.toString() ?? '',
        area: json['area']?.toString() ?? '',
      );
}

final class MissionVerificationResult {
  const MissionVerificationResult({
    this.matches = false,
    this.differenceCount = 0,
    this.expectedCount = 0,
    this.vehicleCount = 0,
    this.differences = const [],
    this.normalizationCount = 0,
    this.normalizations = const [],
    this.homeSyncCount = 0,
    this.homeSync = const [],
  });

  final bool matches;
  final int differenceCount;
  final int expectedCount;
  final int vehicleCount;
  final List<MissionVerificationDifference> differences;
  final int normalizationCount;
  final List<MissionVerificationDifference> normalizations;

  /// 飞控动态 Home 与规划 Home 的差异，不参与任务一致性判定。
  final int homeSyncCount;
  final List<MissionVerificationDifference> homeSync;

  factory MissionVerificationResult.fromJson(JsonMap json) =>
      MissionVerificationResult(
        matches: json['matches'] == true,
        differenceCount: (json['difference_count'] as num?)?.toInt() ?? 0,
        expectedCount: (json['expected_count'] as num?)?.toInt() ?? 0,
        vehicleCount: (json['vehicle_count'] as num?)?.toInt() ?? 0,
        differences: (json['differences'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(MissionVerificationDifference.fromJson)
            .toList(growable: false),
        normalizationCount: (json['normalization_count'] as num?)?.toInt() ?? 0,
        normalizations: (json['normalizations'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(MissionVerificationDifference.fromJson)
            .toList(growable: false),
        homeSyncCount: (json['home_sync_count'] as num?)?.toInt() ?? 0,
        homeSync: (json['home_sync'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(MissionVerificationDifference.fromJson)
            .toList(growable: false),
      );
}

enum MissionParameterType { float, integer, boolean, enumeration }

final class MissionCommandOption {
  const MissionCommandOption({required this.value, required this.label});

  final double value;
  final String label;
}

final class MissionCommandParameter {
  const MissionCommandParameter({
    required this.index,
    required this.name,
    required this.label,
    required this.unit,
    required this.type,
    required this.defaultValue,
    required this.required,
    this.minimum,
    this.maximum,
    this.options = const [],
  });

  final int index;
  final String name;
  final String label;
  final String unit;
  final MissionParameterType type;
  final double defaultValue;
  final bool required;
  final double? minimum;
  final double? maximum;
  final List<MissionCommandOption> options;

  factory MissionCommandParameter.fromJson(JsonMap json) =>
      MissionCommandParameter(
        index: json['index'] as int? ?? 1,
        name: json['name'] as String? ?? '',
        label: json['label'] as String? ?? '',
        unit: json['unit'] as String? ?? '',
        type: switch (json['type']) {
          'integer' => MissionParameterType.integer,
          'boolean' => MissionParameterType.boolean,
          'enum' => MissionParameterType.enumeration,
          _ => MissionParameterType.float,
        },
        defaultValue: (json['default'] as num?)?.toDouble() ?? 0,
        required: json['required'] == true,
        minimum: (json['minimum'] as num?)?.toDouble(),
        maximum: (json['maximum'] as num?)?.toDouble(),
        options: (json['options'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(
              (option) => MissionCommandOption(
                value: (option['value'] as num?)?.toDouble() ?? 0,
                label: option['label'] as String? ?? '',
              ),
            )
            .toList(growable: false),
      );
}

final class MissionCommandDescriptor {
  const MissionCommandDescriptor({
    required this.command,
    required this.name,
    required this.label,
    required this.entityType,
    required this.category,
    required this.description,
    required this.requiresPosition,
    required this.requiresAltitude,
    required this.parameters,
    required this.vehicles,
    this.supported = true,
  });

  final int command;
  final String name;

  /// 中文显示名（后端命令目录提供，前端不要硬编码命令列表）。
  final String label;

  /// 该命令对应的文档实体类型，后端给出，前端新建实体时直接用。
  final String entityType;
  final String category;
  final String description;
  final bool requiresPosition;
  final bool requiresAltitude;
  final List<MissionCommandParameter> parameters;
  final bool supported;

  /// 该命令支持的机型类别（copter/plane/vtol/rover/sub/tracker）。
  /// 空列表表示后端未标注机型，前端不得据此过滤。
  final List<String> vehicles;

  factory MissionCommandDescriptor.fromJson(JsonMap json) =>
      MissionCommandDescriptor(
        command: json['command'] as int? ?? 0,
        name: json['name'] as String? ?? '',
        label: json['label'] as String? ?? json['name'] as String? ?? '',
        entityType: json['entity_type'] as String? ?? 'waypoint',
        category: json['category'] as String? ?? '',
        description: json['description'] as String? ?? '',
        requiresPosition: json['requires_position'] == true,
        requiresAltitude: json['requires_altitude'] == true,
        parameters: (json['parameters'] as List<Object?>? ?? const [])
            .whereType<JsonMap>()
            .map(MissionCommandParameter.fromJson)
            .toList(growable: false),
        supported: json['supported'] != false,
        vehicles: (json['vehicles'] as List<Object?>? ?? const [])
            .whereType<String>()
            .toList(growable: false),
      );
}

List<MissionCommandDescriptor> visibleMissionCommands(
  List<MissionCommandDescriptor> catalog, {
  int? keepCommand,
}) => catalog
    .where(
      (descriptor) => descriptor.supported || descriptor.command == keepCommand,
    )
    .toList(growable: false);

JsonMap _map(Object? value) => value is Map<String, Object?> ? value : const {};

JsonMap? _mapOrNull(Object? value) =>
    value is Map<String, Object?> ? value : null;

List<double> _doubleList(Object? value, {required List<double> fallback}) {
  if (value is! List<Object?>) return fallback;
  return [
    for (var index = 0; index < fallback.length; index++)
      index < value.length && value[index] is num
          ? (value[index] as num).toDouble()
          : fallback[index],
  ];
}

/// 任务文件互操作导出结果（native `mg_mission_doc_export_format`）。
///
/// [format] 是规范化后的格式名 `waypoints`（Mission Planner / QGC 的
/// `QGC WPL 110` 文本）或 `plan`（QGroundControl JSON）；[text] 是要写盘的
/// 完整文件内容，[itemCount] 是导出条目数（waypoints 为编译后的任务项数，
/// plan 为实体数），[warnings] 是导出期间可继续但可能丢字段的提示。
final class MissionInterchangeExport {
  const MissionInterchangeExport({
    required this.format,
    required this.itemCount,
    required this.text,
    this.warnings = const [],
  });

  final String format;
  final int itemCount;
  final String text;
  final List<String> warnings;

  factory MissionInterchangeExport.fromJson(JsonMap json) =>
      MissionInterchangeExport(
        format: json['format'] as String? ?? '',
        itemCount: (json['item_count'] as num?)?.toInt() ?? 0,
        text: json['text'] as String? ?? '',
        warnings: [
          for (final warning in (json['warnings'] as List?) ?? const [])
            warning.toString(),
        ],
      );
}

/// 按交换格式导入的结果：新文档 + 原生解析时产生的提示。
///
/// [warnings] 与导出侧同源同义（例如导入旧版 QGC `.plan` 时「围栏/Rally 段被忽略」
/// 这类丢弃事实），必须转达给用户——静默丢数据是这份格式契约里最容易踩的坑。
final class MissionInterchangeImport {
  const MissionInterchangeImport({
    required this.document,
    this.warnings = const [],
  });

  final MissionDocument document;
  final List<String> warnings;

  factory MissionInterchangeImport.fromEnvelope(JsonMap json) =>
      MissionInterchangeImport(
        document: MissionDocument.fromJson(json['document'] as JsonMap),
        warnings: [
          for (final warning in (json['warnings'] as List?) ?? const [])
            warning.toString(),
        ],
      );
}
