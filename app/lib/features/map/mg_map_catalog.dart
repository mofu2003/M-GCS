enum MgMapStyle { standard, terrain }

final class MgMapSource {
  const MgMapSource({
    required this.id,
    required this.label,
    required this.urlTemplate,
    required this.maxNativeZoom,
    required this.attribution,
    this.fallbackUrlTemplate,
  });

  final String id;
  final String label;
  final String urlTemplate;
  final int maxNativeZoom;
  final String attribution;
  final String? fallbackUrlTemplate;
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

  static MgMapSource sourceFor(MgMapStyle style) {
    return switch (style) {
      MgMapStyle.standard => standard,
      MgMapStyle.terrain => terrain,
    };
  }
}
