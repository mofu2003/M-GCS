import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;

import 'terrain_models.dart';

/// provider 取数失败（网络不可达、非 200、响应结构异常）。
class ElevationProviderException implements Exception {
  ElevationProviderException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// 拉取一组坐标的高程。返回顺序与入参一致；某个点没有高程时
/// [TerrainSample.elevation] 为 null。
abstract interface class ElevationProvider {
  Future<List<TerrainSample>> fetchElevations(List<TerrainSample> coords);
}

/// 明确不可用的替身：在任何环境下都抛 [ElevationProviderException]。
///
/// 用于 flutter_tester 或用户显式关闭在线高程时给出确定错误，而不是静默返回空值。
final class UnavailableElevationProvider implements ElevationProvider {
  const UnavailableElevationProvider();

  @override
  Future<List<TerrainSample>> fetchElevations(List<TerrainSample> coords) async {
    throw ElevationProviderException('在线高程服务未启用');
  }
}

/// open-elevation（`api.open-elevation.com`）实现。
///
/// 请求 `GET <endpoint>?locations=lat,lon|lat,lon|...`，响应
/// `{"results":[{"latitude":..,"longitude":..,"elevation":..}]}`，顺序与请求一致。
/// 单次请求最多 [batchSize] 个点，超出自动分批。端点可替换，便于离线部署自建服务。
final class OpenElevationProvider implements ElevationProvider {
  OpenElevationProvider({
    Uri? endpoint,
    HttpClient? client,
    this.batchSize = 100,
    this.timeout = const Duration(seconds: 20),
  }) : _endpoint =
           endpoint ??
           Uri.parse('https://api.open-elevation.com/api/v1/lookup'),
       _client = client ?? HttpClient() {
    _client.connectionTimeout = const Duration(seconds: 10);
  }

  final Uri _endpoint;
  final HttpClient _client;
  final int batchSize;
  final Duration timeout;

  @override
  Future<List<TerrainSample>> fetchElevations(List<TerrainSample> coords) async {
    if (coords.isEmpty) return const <TerrainSample>[];
    final out = <TerrainSample>[];
    for (var i = 0; i < coords.length; i += batchSize) {
      final end = math.min(i + batchSize, coords.length);
      out.addAll(await _fetchChunk(coords.sublist(i, end)));
    }
    return out;
  }

  Future<List<TerrainSample>> _fetchChunk(List<TerrainSample> chunk) async {
    final locations = chunk.map((c) => '${c.lat},${c.lon}').join('|');
    final uri = _endpoint.replace(
      queryParameters: <String, String>{
        ..._endpoint.queryParameters,
        'locations': locations,
      },
    );

    HttpClientResponse response;
    try {
      final request = await _client.getUrl(uri).timeout(timeout);
      response = await request.close().timeout(timeout);
    } on TimeoutException {
      throw ElevationProviderException('高程服务超时（${timeout.inSeconds}s）');
    } on SocketException catch (e) {
      throw ElevationProviderException('高程服务不可达：${e.message}');
    } on HandshakeException catch (e) {
      throw ElevationProviderException('高程服务 TLS 握手失败：${e.message}');
    }

    final body = await response.transform(utf8.decoder).join().timeout(timeout);
    if (response.statusCode != 200) {
      throw ElevationProviderException('高程服务返回 HTTP ${response.statusCode}');
    }

    final Object? decoded;
    try {
      decoded = jsonDecode(body);
    } on FormatException {
      throw ElevationProviderException('高程服务响应不是合法 JSON');
    }
    if (decoded is! Map || decoded['results'] is! List) {
      throw ElevationProviderException('高程服务响应缺少 results 数组');
    }

    final results = decoded['results'] as List;
    final out = <TerrainSample>[];
    for (var i = 0; i < chunk.length; i++) {
      double? elevation;
      if (i < results.length) {
        final item = results[i];
        if (item is Map && item['elevation'] is num) {
          elevation = (item['elevation'] as num).toDouble();
        }
      }
      out.add(
        TerrainSample(lat: chunk[i].lat, lon: chunk[i].lon, elevation: elevation),
      );
    }
    return out;
  }
}
