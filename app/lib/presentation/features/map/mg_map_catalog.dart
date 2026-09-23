enum MgMapStyle { standard, terrain, amap, amapSatellite, amapSatelliteHybrid }

enum MgMapCoordinateSystem { wgs84, gcj02 }

final class MgMapSource {
  const MgMapSource({
    required this.id,
    required this.label,
    required this.urlTemplate,
    required this.maxNativeZoom,
    required this.attribution,
    this.fallbackUrlTemplate,
    this.subdomains = const <String>[],
    this.coordinateSystem = MgMapCoordinateSystem.wgs84,
    this.overlay,
  });

  final String id;
  final String label;
  final String urlTemplate;
  final int maxNativeZoom;
  final String attribution;
  final String? fallbackUrlTemplate;
  final List<String> subdomains;
  final MgMapCoordinateSystem coordinateSystem;
  final MgMapSource? overlay;
}

final class MgMapCatalog {
  const MgMapCatalog._();

  static const MgMapSource standard = MgMapSource(
    id: 'osm_fr_hot',
    label: 'OpenStreetMap HOT 标准地图',
    urlTemplate: 'https://b.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    fallbackUrlTemplate: 'https://a.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    maxNativeZoom: 19,
    attribution: '© OpenStreetMap contributors | HOT',
  );

  static const MgMapSource terrain = MgMapSource(
    id: 'openmaps_hiking',
    label: 'OpenHikingMap 地形地图',
    urlTemplate: 'https://tile.openmaps.fr/openhikingmap/{z}/{x}/{y}.png',
    fallbackUrlTemplate: 'https://b.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png',
    maxNativeZoom: 17,
    attribution: '© OpenStreetMap contributors | OpenHikingMap',
  );

  /// 高德栅格底图。该源使用公开的瓦片接口，适合无密钥的基础浏览。
  /// 若后续需要官方 Web 服务能力（搜索、路径规划等），应单独配置高德
  /// Key，不要把 Key 拼接到瓦片模板中。
  static const MgMapSource amap = MgMapSource(
    id: 'amap_standard',
    label: '高德标准地图',
    urlTemplate:
        'https://webrd0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=7',
    subdomains: <String>['1', '2', '3', '4'],
    maxNativeZoom: 18,
    attribution: '© 高德地图 | 高德开放平台',
    coordinateSystem: MgMapCoordinateSystem.gcj02,
  );

  static const MgMapSource amapSatellite = MgMapSource(
    id: 'amap_satellite',
    label: '高德卫星地图',
    urlTemplate:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: <String>['1', '2', '3', '4'],
    maxNativeZoom: 18,
    attribution:
        '© 高德地图 | Image© DigitalGlobe & chinasiwei | AIRBUS & EastDawn',
    coordinateSystem: MgMapCoordinateSystem.gcj02,
  );

  static const MgMapSource amapSatelliteLabels = MgMapSource(
    id: 'amap_satellite_labels',
    label: '高德卫星路网标注',
    urlTemplate:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=8',
    subdomains: <String>['1', '2', '3', '4'],
    maxNativeZoom: 18,
    attribution: '',
    coordinateSystem: MgMapCoordinateSystem.gcj02,
  );

  static const MgMapSource amapSatelliteHybrid = MgMapSource(
    id: 'amap_satellite_hybrid',
    label: '高德卫星+路网',
    urlTemplate:
        'https://webst0{s}.is.autonavi.com/appmaptile?x={x}&y={y}&z={z}&lang=zh_cn&size=1&scale=1&style=6',
    subdomains: <String>['1', '2', '3', '4'],
    maxNativeZoom: 18,
    attribution:
        '© 高德地图 | Image© DigitalGlobe & chinasiwei | AIRBUS & EastDawn',
    coordinateSystem: MgMapCoordinateSystem.gcj02,
    overlay: amapSatelliteLabels,
  );

  static MgMapSource sourceFor(MgMapStyle style) {
    return switch (style) {
      MgMapStyle.standard => standard,
      MgMapStyle.terrain => terrain,
      MgMapStyle.amap => amap,
      MgMapStyle.amapSatellite => amapSatellite,
      MgMapStyle.amapSatelliteHybrid => amapSatelliteHybrid,
    };
  }
}
