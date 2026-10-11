import 'package:mgcs_frontend/terrain/elevation_provider.dart';
import 'package:mgcs_frontend/terrain/terrain_backend.dart';
import 'package:mgcs_frontend/terrain/terrain_models.dart';

/// `mg_terrain_*` 的替身后端：不接触 `dart:ffi`，在内存里复刻 native 的
/// 0.01° 网格缓存与剖面采样语义，供控制器测试驱动「缺值 → 回填」流程。
class FakeTerrainBackend implements TerrainBackend {
  final Map<String, double> cache = <String, double>{};
  int queryLineCalls = 0;
  int cacheUpdateCalls = 0;
  int cacheClearCalls = 0;

  /// 与 native `TerrainCache::to_key` 一致：四舍五入到 0.01°。
  static String key(double lat, double lon) =>
      '${(lat * 100).round()},${(lon * 100).round()}';

  @override
  Map<String, Object?> queryPoint(double lat, double lon) {
    final value = cache[key(lat, lon)];
    return <String, Object?>{
      'lat': lat,
      'lon': lon,
      'valid': value != null,
      'elevation': value,
    };
  }

  @override
  Map<String, Object?> queryLine(
    double lat1,
    double lon1,
    double lat2,
    double lon2,
    int samples,
  ) {
    queryLineCalls++;
    final n = samples < 2 ? 2 : samples;
    final points = <Map<String, Object?>>[];
    final missing = <int>[];
    for (var i = 0; i < n; i++) {
      final t = i / (n - 1);
      final lat = lat1 + t * (lat2 - lat1);
      final lon = lon1 + t * (lon2 - lon1);
      final value = cache[key(lat, lon)];
      points.add(<String, Object?>{
        'lat': lat,
        'lon': lon,
        'valid': value != null,
        'elevation': value,
      });
      if (value == null) missing.add(i);
    }
    return <String, Object?>{
      'complete': missing.isEmpty,
      'missing_count': missing.length,
      'missing': missing,
      'points': points,
    };
  }

  @override
  Map<String, Object?> cacheUpdate(List<TerrainSample> samples) {
    cacheUpdateCalls++;
    var stored = 0;
    for (final sample in samples) {
      final elevation = sample.elevation;
      if (elevation == null) continue;
      cache[key(sample.lat, sample.lon)] = elevation;
      stored++;
    }
    return <String, Object?>{'stored': stored, 'cache_size': cache.length};
  }

  @override
  Map<String, Object?> cacheClear() {
    cacheClearCalls++;
    cache.clear();
    return <String, Object?>{'success': true};
  }
}

/// 高程 provider 的替身：记录每次请求，可注入错误或空值。
class FakeElevationProvider implements ElevationProvider {
  final List<List<TerrainSample>> calls = <List<TerrainSample>>[];
  Object? error;

  /// 按坐标给出高程；默认是可预测的线性函数。
  double? Function(double lat, double lon) elevationFor =
      (lat, lon) => 100 + lat + lon;

  int get callCount => calls.length;

  @override
  Future<List<TerrainSample>> fetchElevations(List<TerrainSample> coords) async {
    calls.add(coords);
    final failure = error;
    if (failure != null) throw failure;
    return coords
        .map(
          (c) => TerrainSample(
            lat: c.lat,
            lon: c.lon,
            elevation: elevationFor(c.lat, c.lon),
          ),
        )
        .toList();
  }
}
