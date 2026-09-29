import 'dart:math' as math;

/// WGS-84 与 GCJ-02 互转。
///
/// 飞控上报的 GPS 坐标是 WGS-84，而高德一类国内瓦片用的是 GCJ-02
/// （俗称"火星坐标"）。两者直接混用会偏 100–700 米，表现为"飞机图标不在实际
/// 位置上"。算法与 Mission Planner 的
/// `ExtLibs/GMap.NET.Core/GMap.NET.Projections/MercatorProjectionGCJ.cs` 一致，
/// 不是自己发明的近似。
class GcjCoordinates {
  const GcjCoordinates._();

  /// 克拉索夫斯基椭球长半轴与第一偏心率平方，GCJ-02 算法固定用这两个常数。
  static const double _axis = 6378245.0;
  static const double _eccentricitySquared = 0.0066934216229659433;

  /// 中国大陆大致范围；范围外不做偏移（港澳台与境外瓦片和 WGS-84 对齐）。
  static bool outOfChina(double latitude, double longitude) =>
      longitude < 72.004 ||
      longitude > 137.8347 ||
      latitude < 0.8293 ||
      latitude > 55.8271;

  /// WGS-84 → GCJ-02。
  static (double latitude, double longitude) wgs84ToGcj02(
    double latitude,
    double longitude,
  ) {
    if (outOfChina(latitude, longitude)) return (latitude, longitude);

    var deltaLatitude = _transformLatitude(longitude - 105.0, latitude - 35.0);
    var deltaLongitude = _transformLongitude(
      longitude - 105.0,
      latitude - 35.0,
    );

    final radianLatitude = latitude / 180.0 * math.pi;
    var magic = math.sin(radianLatitude);
    magic = 1 - _eccentricitySquared * magic * magic;
    final sqrtMagic = math.sqrt(magic);

    deltaLatitude =
        (deltaLatitude * 180.0) /
        ((_axis * (1 - _eccentricitySquared)) / (magic * sqrtMagic) * math.pi);
    deltaLongitude =
        (deltaLongitude * 180.0) /
        (_axis / sqrtMagic * math.cos(radianLatitude) * math.pi);

    return (latitude + deltaLatitude, longitude + deltaLongitude);
  }

  /// GCJ-02 → WGS-84。
  ///
  /// GCJ-02 没有公开的解析反函数，用不动点迭代反解：`wgs -= f(wgs) - gcj`。
  /// MP 的 `FromPixelToLatLng` 只迭代一次，往返误差约 1.5 米；这里迭代三次，
  /// 收敛到厘米级。偏移函数在局部近似常数，三次足够。
  static (double latitude, double longitude) gcj02ToWgs84(
    double latitude,
    double longitude,
  ) {
    var resultLatitude = latitude;
    var resultLongitude = longitude;
    for (var iteration = 0; iteration < 3; iteration++) {
      final (forwardLatitude, forwardLongitude) = wgs84ToGcj02(
        resultLatitude,
        resultLongitude,
      );
      resultLatitude -= forwardLatitude - latitude;
      resultLongitude -= forwardLongitude - longitude;
    }
    return (resultLatitude, resultLongitude);
  }

  static double _transformLatitude(double x, double y) {
    var value =
        -100.0 +
        2.0 * x +
        3.0 * y +
        0.2 * y * y +
        0.1 * x * y +
        0.2 * math.sqrt(x.abs());
    value +=
        (20.0 * math.sin(6.0 * x * math.pi) +
            20.0 * math.sin(2.0 * x * math.pi)) *
        2.0 /
        3.0;
    value +=
        (20.0 * math.sin(y * math.pi) + 40.0 * math.sin(y / 3.0 * math.pi)) *
        2.0 /
        3.0;
    value +=
        (160.0 * math.sin(y / 12.0 * math.pi) +
            320.0 * math.sin(y * math.pi / 30.0)) *
        2.0 /
        3.0;
    return value;
  }

  static double _transformLongitude(double x, double y) {
    var value =
        300.0 +
        x +
        2.0 * y +
        0.1 * x * x +
        0.1 * x * y +
        0.1 * math.sqrt(x.abs());
    value +=
        (20.0 * math.sin(6.0 * x * math.pi) +
            20.0 * math.sin(2.0 * x * math.pi)) *
        2.0 /
        3.0;
    value +=
        (20.0 * math.sin(x * math.pi) + 40.0 * math.sin(x / 3.0 * math.pi)) *
        2.0 /
        3.0;
    value +=
        (150.0 * math.sin(x / 12.0 * math.pi) +
            300.0 * math.sin(x / 30.0 * math.pi)) *
        2.0 /
        3.0;
    return value;
  }
}
