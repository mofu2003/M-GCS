import '../ffi_response.dart';
import 'elevation_provider.dart';
import 'terrain_backend.dart';
import 'terrain_models.dart';

/// 一次剖面取数的结果。
class TerrainProfileResult {
  const TerrainProfileResult({
    required this.profile,
    this.error,
    this.providerCalls = 0,
    this.fetched = 0,
  });

  final TerrainProfile profile;

  /// provider 失败或回填后仍有缺值时的可读错误；null 表示完整。
  final String? error;

  /// 本次触发的 provider 请求批次数（0 表示缓存全命中）。
  final int providerCalls;

  /// provider 实际返回并写入缓存的高程点数。
  final int fetched;

  bool get complete => error == null && profile.complete;
}

/// 地形取数编排：native 缓存优先，缺值才请求 provider，回填后重新采样。
///
/// core 不带网络（见 core/terrain/terrain_store.h），所以「拉高程」这一步由本
/// 控制器驱动：`queryLine` → 缺值下标 → provider 批量拉取 → `cacheUpdate` →
/// 再 `queryLine` 拿到完整剖面。缓存挂在 native 上，同一 process 内跨调用共享，
/// 所以第二次查询同一条线通常不会再有 provider 请求。
class TerrainController {
  TerrainController({
    required this.backend,
    required this.provider,
    this.defaultSamples = 64,
  });

  final TerrainBackend backend;
  final ElevationProvider provider;

  /// `loadProfile` 未显式给 samples 时的默认采样点数。
  final int defaultSamples;

  /// 单点高程。缓存命中直接返回；未命中时可选地经 provider 补拉。
  ///
  /// [allowNetwork] 为 false 时只读缓存，未命中返回 null。provider 取数失败会
  /// 抛 [ElevationProviderException]（单点场景没有剖面可以部分返回）。
  Future<TerrainSample?> elevationAt(
    double lat,
    double lon, {
    bool allowNetwork = true,
  }) async {
    final raw = backend.queryPoint(lat, lon);
    final point = TerrainSample.fromJson(raw);
    if (point.valid) return point;
    if (!allowNetwork) return null;

    final fetched = await provider.fetchElevations(<TerrainSample>[
      TerrainSample(lat: lat, lon: lon),
    ]);
    final stored = fetched.where((s) => s.valid).toList();
    if (stored.isEmpty) return null;
    backend.cacheUpdate(stored);

    final refilled = TerrainSample.fromJson(backend.queryPoint(lat, lon));
    return refilled.valid ? refilled : null;
  }

  /// 沿直线取一条完整剖面。
  ///
  /// 返回的 [TerrainProfileResult] 里：
  /// * `providerCalls == 0` 且 `complete`：缓存全命中，没有联网。
  /// * `error == null` 且 `complete`：provider 补齐过缺值。
  /// * `error != null`：provider 失败，或补齐后仍有缺值（`profile.missing` 给出）。
  Future<TerrainProfileResult> loadProfile({
    required double lat1,
    required double lon1,
    required double lat2,
    required double lon2,
    int? samples,
  }) async {
    final count = samples ?? defaultSamples;
    final firstRaw = backend.queryLine(lat1, lon1, lat2, lon2, count);
    // 查询返回是「数据」（valid/missing/points），没有 success 字段；
    // 只有带规范 error 对象的失败响应才按错误处理。
    if (firstRaw.containsKey('error')) {
      return TerrainProfileResult(
        profile: const TerrainProfile(points: [], missing: []),
        error: ffiErrorText(firstRaw, '地形查询失败'),
      );
    }

    var profile = TerrainProfile.fromJson(firstRaw);
    if (profile.points.isEmpty) {
      return TerrainProfileResult(
        profile: profile,
        error: '地形查询没有返回采样点',
      );
    }
    if (profile.complete) {
      return TerrainProfileResult(profile: profile);
    }

    final coords = <TerrainSample>[];
    for (final idx in profile.missing) {
      if (idx < 0 || idx >= profile.points.length) continue;
      final pt = profile.points[idx];
      coords.add(TerrainSample(lat: pt.lat, lon: pt.lon));
    }

    final List<TerrainSample> fetched;
    try {
      fetched = await provider.fetchElevations(coords);
    } on ElevationProviderException catch (e) {
      return TerrainProfileResult(
        profile: profile,
        error: e.message,
        providerCalls: 1,
      );
    }

    final stored = fetched.where((s) => s.valid).toList();
    if (stored.isNotEmpty) backend.cacheUpdate(stored);

    profile = TerrainProfile.fromJson(backend.queryLine(lat1, lon1, lat2, lon2, count));
    final error = profile.complete
        ? null
        : '仍有 ${profile.missing.length} 个采样点没有高程';
    return TerrainProfileResult(
      profile: profile,
      error: error,
      providerCalls: 1,
      fetched: stored.length,
    );
  }

  /// 清空 native 缓存（切换任务或强制重新取数时用）。
  void clearCache() => backend.cacheClear();
}
