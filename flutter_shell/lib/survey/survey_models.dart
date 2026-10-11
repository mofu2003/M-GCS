import '../mission/mission_document_models.dart';

/// 相机参数，字段与 native `mg::core::mission::CameraSpec` 一一对应。
///
/// 单位与默认值与 native 保持一致；前端只做编辑与 JSON 透传，重算在 core。
final class SurveyCameraSpec {
  const SurveyCameraSpec({
    this.focalLengthMm = 35,
    this.sensorWidthMm = 23.5,
    this.sensorHeightMm = 15.6,
    this.imageWidthPx = 6000,
    this.imageHeightPx = 4000,
    this.minOverlapPct = 70,
    this.sideOverlapPct = 60,
  });

  final double focalLengthMm;
  final double sensorWidthMm;
  final double sensorHeightMm;
  final double imageWidthPx;
  final double imageHeightPx;
  final double minOverlapPct;
  final double sideOverlapPct;

  JsonMap toJson() => {
    'focal_length': focalLengthMm,
    'sensor_width': sensorWidthMm,
    'sensor_height': sensorHeightMm,
    'image_width': imageWidthPx,
    'image_height': imageHeightPx,
    'min_overlap': minOverlapPct,
    'side_overlap': sideOverlapPct,
  };
}

/// 矩形/交叉网格测绘配置，对应 native `SurveyConfig`。
final class SurveyGridConfig {
  const SurveyGridConfig({
    required this.centerLat,
    required this.centerLon,
    this.widthM = 200,
    this.lengthM = 200,
    this.altitudeM = 80,
    this.speedMps = 5,
    this.angleDeg = 0,
    this.crossGrid = false,
    this.returnToStart = true,
    this.camera = const SurveyCameraSpec(),
  });

  final double centerLat;
  final double centerLon;
  final double widthM;
  final double lengthM;
  final double altitudeM;
  final double speedMps;
  final double angleDeg;
  final bool crossGrid;
  final bool returnToStart;
  final SurveyCameraSpec camera;

  JsonMap toJson() => {
    'center_lat': centerLat,
    'center_lon': centerLon,
    'width': widthM,
    'length': lengthM,
    'altitude': altitudeM,
    'speed': speedMps,
    'angle': angleDeg,
    'cross_grid': crossGrid,
    'return_to_start': returnToStart,
    'camera': camera.toJson(),
  };
}

/// 走廊扫描配置，对应 native `CorridorConfig`。
final class SurveyCorridorConfig {
  const SurveyCorridorConfig({
    required this.startLat,
    required this.startLon,
    required this.endLat,
    required this.endLon,
    this.widthM = 100,
    this.altitudeM = 80,
    this.speedMps = 5,
    this.sideOverlapPct = 60,
    this.returnToStart = true,
    this.camera = const SurveyCameraSpec(),
  });

  final double startLat;
  final double startLon;
  final double endLat;
  final double endLon;
  final double widthM;
  final double altitudeM;
  final double speedMps;
  final double sideOverlapPct;
  final bool returnToStart;
  final SurveyCameraSpec camera;

  JsonMap toJson() => {
    'start_lat': startLat,
    'start_lon': startLon,
    'end_lat': endLat,
    'end_lon': endLon,
    'width': widthM,
    'altitude': altitudeM,
    'speed': speedMps,
    'side_overlap': sideOverlapPct,
    'return_to_start': returnToStart,
    'camera': camera.toJson(),
  };
}

/// 生成结果里的一条任务项。字段与 native `mission_item_json` 对齐。
final class SurveyWaypoint {
  const SurveyWaypoint({
    required this.seq,
    required this.command,
    required this.frame,
    required this.autocontinue,
    required this.params,
    this.latitude,
    this.longitude,
    this.altitudeMeters,
  });

  final int seq;
  final int command;
  final int frame;
  final int autocontinue;
  final List<double> params;
  final double? latitude;
  final double? longitude;
  final double? altitudeMeters;

  factory SurveyWaypoint.fromJson(JsonMap json) => SurveyWaypoint(
    seq: json['seq'] as int? ?? 0,
    command: json['command'] as int? ?? 0,
    frame: json['frame'] as int? ?? 3,
    autocontinue: json['autocontinue'] as int? ?? 1,
    params: [
      for (final key in const ['param1', 'param2', 'param3', 'param4'])
        (json[key] as num?)?.toDouble() ?? 0,
    ],
    latitude: (json['lat'] as num?)?.toDouble(),
    longitude: (json['lon'] as num?)?.toDouble(),
    altitudeMeters: (json['z'] as num?)?.toDouble(),
  );

  /// 转成任务文档实体。`type` 由命令目录提供（[entityTypeFor]）。
  ///
  /// [frame] 覆盖 native 返回的高度基准帧（例如测绘页选「地形跟随 (AGL)」时
  /// 写入 `MAV_FRAME_GLOBAL_TERRAIN_ALT`）；省略时沿用 native 返回值。
  MissionEntity toMissionEntity(String entityType, {int? frame}) => MissionEntity(
    type: entityType,
    command: command,
    frame: frame ?? this.frame,
    autocontinue: autocontinue,
    latitude: latitude,
    longitude: longitude,
    altitudeMeters: altitudeMeters,
    params: params,
  );
}

/// 测绘生成结果。`valid == false` 时 [message] 说明原因。
final class SurveyResult {
  const SurveyResult({
    required this.valid,
    this.message = '',
    this.waypointCount = 0,
    this.flightLineCount = 0,
    this.totalDistanceM = 0,
    this.estimatedTimeS = 0,
    this.areaM2 = 0,
    this.photoCount = 0,
    this.waypoints = const [],
  });

  final bool valid;
  final String message;
  final int waypointCount;
  final int flightLineCount;
  final double totalDistanceM;
  final double estimatedTimeS;
  final double areaM2;
  final int photoCount;
  final List<SurveyWaypoint> waypoints;

  factory SurveyResult.fromJson(JsonMap json) {
    final valid = json['valid'] == true;
    if (!valid) {
      return SurveyResult(
        valid: false,
        message: json['message'] as String? ?? '测绘生成失败',
      );
    }
    return SurveyResult(
      valid: true,
      waypointCount: json['waypoint_count'] as int? ?? 0,
      flightLineCount: json['flight_line_count'] as int? ?? 0,
      totalDistanceM: (json['total_distance_m'] as num?)?.toDouble() ?? 0,
      estimatedTimeS: (json['estimated_time_s'] as num?)?.toDouble() ?? 0,
      areaM2: (json['area_m2'] as num?)?.toDouble() ?? 0,
      photoCount: json['photo_count'] as int? ?? 0,
      waypoints: [
        for (final raw in (json['waypoints'] as List<Object?>? ?? const []))
          if (raw is JsonMap) SurveyWaypoint.fromJson(raw),
      ],
    );
  }
}
