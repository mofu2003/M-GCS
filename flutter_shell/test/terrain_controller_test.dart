import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/terrain/elevation_provider.dart';
import 'package:mgcs_frontend/terrain/terrain_controller.dart';

import 'support/terrain_backend_fake.dart';

/// 一条 0.1° 长、11 个采样点的剖面：每个采样点落在不同的 0.01° 网格里，
/// 因此首次取数时 11 个点全部缺值。
Future<TerrainProfileResult> _load(TerrainController controller) =>
    controller.loadProfile(
      lat1: 39.9,
      lon1: 116.4,
      lat2: 40.0,
      lon2: 116.5,
      samples: 11,
    );

void main() {
  late FakeTerrainBackend backend;
  late FakeElevationProvider provider;
  late TerrainController controller;

  setUp(() {
    backend = FakeTerrainBackend();
    provider = FakeElevationProvider();
    controller = TerrainController(backend: backend, provider: provider);
  });

  test('首次取数：缺值经 provider 回填，剖面完整', () async {
    final result = await _load(controller);

    expect(result.complete, isTrue);
    expect(result.error, isNull);
    expect(result.providerCalls, 1);
    expect(result.fetched, 11);
    expect(result.profile.points, hasLength(11));
    expect(result.profile.missing, isEmpty);
    expect(provider.callCount, 1);
    expect(provider.calls.single, hasLength(11));
    expect(backend.cacheUpdateCalls, 1);
    // 高程来自 provider，而不是 -1001 之类的桩值。
    expect(result.profile.points.first.elevation, greaterThan(0));
  });

  test('缓存全命中：第二次取数不再联网', () async {
    await _load(controller);
    final callsAfterFirst = provider.callCount;
    final updatesAfterFirst = backend.cacheUpdateCalls;

    final second = await _load(controller);

    expect(second.complete, isTrue);
    expect(second.providerCalls, 0);
    expect(second.fetched, 0);
    expect(provider.callCount, callsAfterFirst);
    expect(backend.cacheUpdateCalls, updatesAfterFirst);
  });

  test('provider 失败：返回可读错误，剖面保留缺值', () async {
    provider.error = ElevationProviderException('网络不可达');

    final result = await _load(controller);

    expect(result.complete, isFalse);
    expect(result.error, '网络不可达');
    expect(result.profile.missing, hasLength(11));
    expect(backend.cacheUpdateCalls, 0);
  });

  test('provider 部分返回：写入有效点并报告剩余缺值', () async {
    provider.elevationFor = (lat, lon) => lat == 39.9 ? 55 : null;

    final result = await _load(controller);

    expect(result.complete, isFalse);
    expect(result.fetched, 1);
    expect(result.error, contains('仍有'));
    expect(result.profile.missing, hasLength(10));
  });

  test('clearCache 透传到 native 后端', () {
    controller.clearCache();
    expect(backend.cacheClearCalls, 1);
  });

  test('单点：allowNetwork=false 只读缓存，allowNetwork=true 回填', () async {
    expect(await controller.elevationAt(39.9, 116.4, allowNetwork: false), isNull);

    final point = await controller.elevationAt(39.9, 116.4);
    expect(point, isNotNull);
    expect(point!.valid, isTrue);
    expect(provider.callCount, 1);

    // 再查同一点应命中缓存，不再联网。
    final cached = await controller.elevationAt(39.9, 116.4);
    expect(cached, isNotNull);
    expect(provider.callCount, 1);
  });
}
