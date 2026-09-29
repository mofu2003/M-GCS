import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/map/gcj_coordinates.dart';

void main() {
  test('国内坐标会被偏移，偏移量在几百米量级', () {
    // 上海人民广场附近。
    const latitude = 31.2304;
    const longitude = 121.4737;
    final (gcjLat, gcjLon) = GcjCoordinates.wgs84ToGcj02(latitude, longitude);

    expect(gcjLat, isNot(closeTo(latitude, 1e-9)));
    expect(gcjLon, isNot(closeTo(longitude, 1e-9)));

    // 经度方向偏移换算成米：1° ≈ 111km × cos(纬度)。
    final offsetMeters =
        (gcjLon - longitude).abs() * 111000 * 0.855; // cos(31.23°) ≈ 0.855
    expect(offsetMeters, greaterThan(100));
    expect(offsetMeters, lessThan(700));
  });

  test('境外坐标原样返回', () {
    const latitude = 35.6762; // Tokyo
    const longitude = 139.6503;
    final (gcjLat, gcjLon) = GcjCoordinates.wgs84ToGcj02(latitude, longitude);
    expect(gcjLat, latitude);
    expect(gcjLon, longitude);
  });

  test('WGS-84 -> GCJ-02 -> WGS-84 往返误差在厘米级', () {
    const samples = [
      (31.2304, 121.4737), // 上海
      (39.9042, 116.4074), // 北京
      (23.1291, 113.2644), // 广州
      (45.8038, 126.5349), // 哈尔滨
    ];
    for (final (latitude, longitude) in samples) {
      final (gcjLat, gcjLon) = GcjCoordinates.wgs84ToGcj02(latitude, longitude);
      final (backLat, backLon) = GcjCoordinates.gcj02ToWgs84(gcjLat, gcjLon);
      // 0.000001° 约 0.11 米。
      expect(backLat, closeTo(latitude, 1e-6));
      expect(backLon, closeTo(longitude, 1e-6));
    }
  });
}
