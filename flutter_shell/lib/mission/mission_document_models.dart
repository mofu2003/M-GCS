typedef JsonMap = Map<String, Object?>;

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
  };
}

final class MissionDocument {
  const MissionDocument({
    this.schemaVersion = 1,
    this.metadata = const MissionDocumentMetadata(),
    this.home = const MissionHome(),
    this.defaults = const MissionDefaults(),
    this.revision = 1,
    this.updatedAtMs = 0,
    this.entities = const [],
  });

  final int schemaVersion;
  final MissionDocumentMetadata metadata;
  final MissionHome home;
  final MissionDefaults defaults;
  final int revision;
  final int updatedAtMs;
  final List<MissionEntity> entities;

  factory MissionDocument.fromJson(JsonMap json) => MissionDocument(
    schemaVersion: json['schema_version'] as int? ?? 1,
    metadata: MissionDocumentMetadata.fromJson(_map(json['metadata'])),
    home: MissionHome.fromJson(_mapOrNull(json['home'])),
    defaults: MissionDefaults.fromJson(_mapOrNull(json['defaults'])),
    revision: json['revision'] as int? ?? 1,
    updatedAtMs: json['updated_at_ms'] as int? ?? 0,
    entities: (json['entities'] as List<Object?>? ?? const [])
        .whereType<JsonMap>()
        .map(MissionEntity.fromJson)
        .toList(growable: false),
  );

  JsonMap toJson() => {
    'schema_version': schemaVersion,
    'metadata': metadata.toJson(),
    'home': home.toJson(),
    'defaults': defaults.toJson(),
    'revision': revision,
    'updated_at_ms': updatedAtMs,
    'entities': [for (final entity in entities) entity.toJson()],
  };

  /// 只替换需要改的字段；`replace` 操作会用整份文档覆盖后端，
  /// 所以 revision / updatedAtMs 必须原样带过去。
  MissionDocument copyWith({
    MissionDocumentMetadata? metadata,
    MissionHome? home,
    MissionDefaults? defaults,
    int? revision,
    int? updatedAtMs,
    List<MissionEntity>? entities,
  }) => MissionDocument(
    schemaVersion: schemaVersion,
    metadata: metadata ?? this.metadata,
    home: home ?? this.home,
    defaults: defaults ?? this.defaults,
    revision: revision ?? this.revision,
    updatedAtMs: updatedAtMs ?? this.updatedAtMs,
    entities: entities ?? this.entities,
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
    this.finished = false,
  });

  final bool valid;
  final int currentIndex;
  final int currentSeq;
  final int total;
  final int reachedCount;
  final int lastReachedSeq;
  /// 航段飞完：最后一项已到达。固定翼此时还在返航/降落，不等于任务完成。
  final bool reachedEnd;
  /// 任务完成，判定与 QGC 一致：曾在任务模式下解锁飞行，之后上锁。
  final bool finished;

  /// 有可判断的飞控任务时才展示进度。
  bool get active => valid && total > 0;

  factory MissionExecutionProgress.fromJson(JsonMap json) =>
      MissionExecutionProgress(
        valid: json['valid'] == true,
        currentIndex: (json['current_index'] as num?)?.toInt() ?? 0,
        currentSeq: (json['current_seq'] as num?)?.toInt() ?? 0,
        total: (json['total'] as num?)?.toInt() ?? 0,
        reachedCount: (json['reached_count'] as num?)?.toInt() ?? 0,
        lastReachedSeq: (json['last_reached_seq'] as num?)?.toInt() ?? 0,
        reachedEnd: json['reached_end'] == true,
        finished: json['finished'] == true,
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
  });

  final bool matches;
  final int differenceCount;
  final int expectedCount;
  final int vehicleCount;
  final List<MissionVerificationDifference> differences;
  final int normalizationCount;
  final List<MissionVerificationDifference> normalizations;

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
      );
}

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
