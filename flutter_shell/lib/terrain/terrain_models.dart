/// 地形高程的领域模型。
///
/// 对应 FFI `mg_terrain_query` / `mg_terrain_query_line` 的 JSON：
///
/// ```json
/// {"complete":false,"missing_count":9,"missing":[1,...],
///  "points":[{"lat":..,"lon":..,"valid":false,"elevation":null}, ...]}
/// ```
///
/// 这里只做解析；「缺值 → 拉 provider → 回填」的编排在
/// [TerrainController]（terrain_controller.dart）。
library;

/// 一个地形采样点。[elevation] 为 null 表示高程尚未缓存。
class TerrainSample {
  const TerrainSample({required this.lat, required this.lon, this.elevation});

  factory TerrainSample.fromJson(Map<String, Object?> json) {
    final elevation = json['elevation'];
    return TerrainSample(
      lat: (json['lat'] as num?)?.toDouble() ?? 0,
      lon: (json['lon'] as num?)?.toDouble() ?? 0,
      elevation: elevation is num ? elevation.toDouble() : null,
    );
  }

  final double lat;
  final double lon;
  final double? elevation;

  bool get valid => elevation != null;

  Map<String, Object?> toJson() => <String, Object?>{
    'lat': lat,
    'lon': lon,
    'elevation': elevation,
  };
}

/// 一条航线剖面。
///
/// [points] 是沿线采样点，[missing] 是其中还没有高程的下标；两者都由 native
/// 返回，前端据此决定要补拉哪些点，不自己重算采样几何。
class TerrainProfile {
  const TerrainProfile({required this.points, required this.missing});

  factory TerrainProfile.fromJson(Map<String, Object?> json) {
    final rawPoints = json['points'];
    final points = <TerrainSample>[];
    if (rawPoints is List) {
      for (final item in rawPoints) {
        if (item is Map) {
          points.add(TerrainSample.fromJson(Map<String, Object?>.from(item)));
        }
      }
    }

    final rawMissing = json['missing'];
    final missing = <int>[];
    if (rawMissing is List) {
      for (final item in rawMissing) {
        if (item is num) missing.add(item.toInt());
      }
    }

    return TerrainProfile(points: points, missing: missing);
  }

  final List<TerrainSample> points;
  final List<int> missing;

  bool get complete => missing.isEmpty;
}
