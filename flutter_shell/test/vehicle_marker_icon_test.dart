import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/map/vehicle_marker_icon.dart';

void main() {
  testWidgets('both aircraft silhouettes retain upright aircraft numbers', (
    tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Row(
          children: [
            for (final kind in VehicleMarkerKind.values)
              SizedBox.square(
                dimension: 56,
                child: VehicleMarkerIcon(
                  kind: kind,
                  fill: Colors.blue,
                  outline: Colors.white,
                  headingDegrees: 90,
                  vehicleId: kind.index + 1,
                ),
              ),
          ],
        ),
      ),
    );
    for (final number in ['1', '2']) {
      final label = find.text(number);
      expect(label, findsOneWidget);
      for (final transform in tester.widgetList<Transform>(
        find.ancestor(of: label, matching: find.byType(Transform)),
      )) {
        expect(transform.transform.storage[0], 1);
        expect(transform.transform.storage[1], 0);
      }
    }
    expect(tester.takeException(), isNull);
  });
}
