import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/features/map/mg_map_catalog.dart';

void main() {
  test('map catalog exposes standard and terrain sources with attribution', () {
    final standard = MgMapCatalog.sourceFor(MgMapStyle.standard);
    final terrain = MgMapCatalog.sourceFor(MgMapStyle.terrain);

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
  });
}
