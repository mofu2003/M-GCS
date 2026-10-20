import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection_panel.dart';
import 'package:mgcs_frontend/map/amap_map_view.dart';
import 'package:mgcs_frontend/map/map_preferences.dart';
import 'package:mgcs_frontend/mission/mission_planning_panel.dart';

void main() {
  group('custom tile source (KD-37)', () {
    test('accepts an https template with z/x/y placeholders', () {
      expect(
        validateCustomTileUrl('https://tiles.example.com/{z}/{x}/{y}.png'),
        isNull,
      );
      expect(
        isCustomTileUrlUsable('http://127.0.0.1:8080/tiles/{z}/{x}/{y}.png'),
        isTrue,
      );
    });

    test('rejects empty, non-http and missing-placeholder templates', () {
      expect(validateCustomTileUrl('   '), isNotNull);
      expect(validateCustomTileUrl('file:///tmp/{z}/{x}/{y}.png'), isNotNull);
      expect(
        validateCustomTileUrl('https://tiles.example.com/{z}/{x}.png'),
        isNotNull,
      );
      expect(
        validateCustomTileUrl('https://tiles.example.com/{z}/{y}.png'),
        isNotNull,
      );
    });

    test('falls back to the standard basemap until a template is set', () {
      expect(mapStyleLabel(MapStyle.custom), contains('未配置'));
      expect(
        mapStyleLabel(
          MapStyle.custom,
          customTileUrl: 'https://tiles.example.com/{z}/{x}/{y}.png',
        ),
        '自定义瓦片源',
      );
      expect(
        mapStyleAttribution(
          MapStyle.custom,
          customTileUrl: 'https://tiles.example.com/{z}/{x}/{y}.png',
        ),
        contains('自定义地址'),
      );
    });

    test('presets keep their own labels after adding the custom entry', () {
      expect(mapStyleLabel(MapStyle.amap), '高德标准地图');
      expect(MapStyle.values.first.name, 'standard');
      expect(MapStyle.values.last, MapStyle.custom);
    });
  });

  group('right panel coordination', () {
    test('primary panels share one right-edge slot', () {
      var panel = nextMapRightPanel(
        MapRightPanel.none,
        MapRightPanel.connection,
      );
      expect(panel, MapRightPanel.connection);

      panel = nextMapRightPanel(panel, MapRightPanel.mission);
      expect(panel, MapRightPanel.mission);

      panel = nextMapRightPanel(panel, MapRightPanel.none);
      expect(panel, MapRightPanel.none);
    });

    test('fleet inspector offsets left by the primary panel width', () {
      expect(fleetPanelRightInset(MapRightPanel.none), kMapPanelEdgeInset);
      expect(
        fleetPanelRightInset(MapRightPanel.connection),
        kMapPanelEdgeInset + kConnectionPanelMaxWidth + kMapPanelGap,
      );
      expect(
        fleetPanelRightInset(MapRightPanel.mission),
        kMapPanelEdgeInset + kMissionPanelWidth + kMapPanelGap,
      );
    });

    test('fleet inspector hides instead of overflowing a narrow window', () {
      expect(canShowFleetAlongside(1600, MapRightPanel.mission), isTrue);
      expect(canShowFleetAlongside(800, MapRightPanel.mission), isFalse);
    });
  });

  group('vehicle camera', () {
    test('follows while enabled even after the first centering', () {
      expect(
        shouldMoveCameraToVehicle(followVehicle: true, centeredOnce: true),
        isTrue,
      );
    });

    test('centers once on the first fix', () {
      expect(
        shouldMoveCameraToVehicle(followVehicle: false, centeredOnce: false),
        isTrue,
      );
    });

    test('leaves a manually panned map alone when follow is off', () {
      expect(
        shouldMoveCameraToVehicle(followVehicle: false, centeredOnce: true),
        isFalse,
      );
    });
  });

  group('flight action visibility', () {
    test('shows only takeoff on the ground', () {
      final visibility = flightActionVisibility(
        flying: false,
        landCapability: true,
      );
      expect(visibility.takeoff, isTrue);
      expect(visibility.land, isFalse);
      expect(visibility.rtl, isFalse);
      expect(visibility.changeAltitude, isFalse);
    });

    test('change altitude sits with land and RTL in flight', () {
      final visibility = flightActionVisibility(
        flying: true,
        landCapability: true,
      );
      expect(visibility.changeAltitude, isTrue);
    });

    test('shows land and RTL in flight for non fixed wing', () {
      final visibility = flightActionVisibility(
        flying: true,
        landCapability: true,
      );
      expect(visibility.takeoff, isFalse);
      expect(visibility.land, isTrue);
      expect(visibility.rtl, isTrue);
    });

    test('hides land for fixed wing but keeps RTL', () {
      final visibility = flightActionVisibility(
        flying: true,
        landCapability: false,
      );
      expect(visibility.takeoff, isFalse);
      expect(visibility.land, isFalse);
      expect(visibility.rtl, isTrue);
    });
  });
}
