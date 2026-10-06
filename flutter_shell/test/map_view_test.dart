import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/map/amap_map_view.dart';

void main() {
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
      final visibility = flightActionVisibility(flying: false, fixedWing: false);
      expect(visibility.takeoff, isTrue);
      expect(visibility.land, isFalse);
      expect(visibility.rtl, isFalse);
      expect(visibility.changeAltitude, isFalse);
    });

    test('change altitude sits with land and RTL in flight', () {
      final visibility = flightActionVisibility(flying: true, fixedWing: false);
      expect(visibility.changeAltitude, isTrue);
    });

    test('shows land and RTL in flight for non fixed wing', () {
      final visibility = flightActionVisibility(flying: true, fixedWing: false);
      expect(visibility.takeoff, isFalse);
      expect(visibility.land, isTrue);
      expect(visibility.rtl, isTrue);
    });

    test('hides land for fixed wing but keeps RTL', () {
      final visibility = flightActionVisibility(flying: true, fixedWing: true);
      expect(visibility.takeoff, isFalse);
      expect(visibility.land, isFalse);
      expect(visibility.rtl, isTrue);
    });
  });
}
