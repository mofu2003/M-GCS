import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/presentation/features/map/mg_map_catalog.dart';

void main() {
  test(
    'map catalog exposes sources with attribution and coordinate metadata',
    () {
      final standard = MgMapCatalog.sourceFor(MgMapStyle.standard);
      final terrain = MgMapCatalog.sourceFor(MgMapStyle.terrain);
      final amap = MgMapCatalog.sourceFor(MgMapStyle.amap);
      final satellite = MgMapCatalog.sourceFor(MgMapStyle.amapSatellite);
      final hybrid = MgMapCatalog.sourceFor(MgMapStyle.amapSatelliteHybrid);

      expect(standard.id, 'osm_fr_hot');
      expect(standard.label, 'OpenStreetMap HOT 标准地图');
      expect(standard.urlTemplate, contains('b.tile.openstreetmap.fr/hot'));
      expect(
        standard.fallbackUrlTemplate,
        contains('a.tile.openstreetmap.fr/hot'),
      );
      expect(standard.attribution, isNotEmpty);
      expect(terrain.id, 'openmaps_hiking');
      expect(terrain.label, 'OpenHikingMap 地形地图');
      expect(terrain.urlTemplate, contains('tile.openmaps.fr/openhikingmap'));
      expect(
        terrain.fallbackUrlTemplate,
        contains('b.tile.openstreetmap.fr/hot'),
      );
      expect(terrain.attribution, isNotEmpty);
      expect(amap.id, 'amap_standard');
      expect(amap.urlTemplate, contains('autonavi.com/appmaptile'));
      expect(amap.subdomains, ['1', '2', '3', '4']);
      expect(amap.coordinateSystem, MgMapCoordinateSystem.gcj02);
      expect(satellite.id, 'amap_satellite');
      expect(satellite.urlTemplate, contains('webst0{s}.is.autonavi.com'));
      expect(satellite.urlTemplate, contains('style=6'));
      expect(satellite.coordinateSystem, MgMapCoordinateSystem.gcj02);
      expect(hybrid.id, 'amap_satellite_hybrid');
      expect(hybrid.overlay?.id, 'amap_satellite_labels');
      expect(hybrid.overlay?.urlTemplate, contains('style=8'));
    },
  );
}
