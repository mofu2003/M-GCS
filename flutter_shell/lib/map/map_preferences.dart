import 'dart:convert';
import 'dart:io';

import 'package:latlong2/latlong.dart';
import 'package:path_provider/path_provider.dart';

/// 地图默认中心（上海，GCJ-02）。
const kDefaultMapCenter = LatLng(31.2304, 121.4737);

/// 地图默认缩放与可选范围。
const kDefaultMapZoom = 11.0;
const kMinMapZoom = 3.0;
const kMaxMapZoom = 19.0;

/// 地图界面偏好：上次选的地图源、缩放级别和中心点。
///
/// 存成应用支持目录下的一个 JSON 文件。macOS 沙盒下该目录落在应用容器里，
/// 不需要平台专属 API，也不依赖后端。
class MapPreferences {
  const MapPreferences({
    required this.styleName,
    required this.zoom,
    required this.center,
  });

  /// 地图源的名字（`MapStyle.name`）。存名字而不是序号，
  /// 以后在枚举里插新地图也不会把旧偏好错位。
  final String styleName;
  final double zoom;
  final LatLng center;

  static const _fileName = 'map_preferences.json';

  static const fallback = MapPreferences(
    styleName: 'amap',
    zoom: kDefaultMapZoom,
    center: kDefaultMapCenter,
  );

  static Future<MapPreferences> load() async {
    try {
      final file = await _file();
      if (!file.existsSync()) return fallback;
      final json = jsonDecode(await file.readAsString());
      if (json is! Map<String, Object?>) return fallback;
      final latitude = (json['lat'] as num?)?.toDouble();
      final longitude = (json['lng'] as num?)?.toDouble();
      return MapPreferences(
        styleName: json['style'] as String? ?? fallback.styleName,
        zoom: ((json['zoom'] as num?)?.toDouble() ?? fallback.zoom).clamp(
          kMinMapZoom,
          kMaxMapZoom,
        ),
        center: latitude == null || longitude == null
            ? fallback.center
            : LatLng(latitude, longitude),
      );
    } on Object {
      // 偏好读失败就用默认值，不打扰使用者。
      return fallback;
    }
  }

  Future<void> save() async {
    try {
      final file = await _file();
      await file.writeAsString(
        jsonEncode({
          'style': styleName,
          'zoom': zoom,
          'lat': center.latitude,
          'lng': center.longitude,
        }),
      );
    } on Object {
      // 写失败不影响本次使用。
    }
  }

  static Future<File> _file() async {
    final directory = await getApplicationSupportDirectory();
    return File('${directory.path}/$_fileName');
  }
}
