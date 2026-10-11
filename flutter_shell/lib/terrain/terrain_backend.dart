import 'terrain_models.dart';

/// 地形高程的原生边界。
///
/// 只暴露 `mg_terrain_*` 导出。core 不发网络请求：高程必须先经
/// [cacheUpdate] 写入，再通过 [queryPoint]/[queryLine] 读取。provider 的
/// 拉取在 [ElevationProvider]（elevation_provider.dart）。
abstract interface class TerrainBackend {
  /// 单点查询缓存。命中返回 `valid:true` + 高程，未命中返回 `valid:false`。
  Map<String, Object?> queryPoint(double lat, double lon);

  /// 沿直线采样 [samples] 个点，返回 `points` 与缺值下标 `missing`。
  Map<String, Object?> queryLine(
    double lat1,
    double lon1,
    double lat2,
    double lon2,
    int samples,
  );

  /// 批量写入高程缓存，返回 `stored` 与 `cache_size`。
  Map<String, Object?> cacheUpdate(List<TerrainSample> samples);

  /// 清空高程缓存。
  Map<String, Object?> cacheClear();
}

/// 原生库不可用时的空实现。
///
/// 让 terrain 路径在 `initializationError != null`（例如 flutter_tester 没有
/// 原生库）时返回确定的错误，而不是在构建期抛异常。
final class TerrainUnavailableBackend implements TerrainBackend {
  const TerrainUnavailableBackend();

  Map<String, Object?> _fail() => const {
    'success': false,
    'error': {'code': -1, 'name': 'UNAVAILABLE', 'message': '原生地形后端不可用'},
  };

  @override
  Map<String, Object?> queryPoint(double lat, double lon) => _fail();

  @override
  Map<String, Object?> queryLine(
    double lat1,
    double lon1,
    double lat2,
    double lon2,
    int samples,
  ) => _fail();

  @override
  Map<String, Object?> cacheUpdate(List<TerrainSample> samples) => _fail();

  @override
  Map<String, Object?> cacheClear() => _fail();
}
