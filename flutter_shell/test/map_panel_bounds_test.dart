import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/map/amap_map_view.dart';

void main() {
  for (final size in [
    const Size(800, 392),
    const Size(844, 302),
    const Size(1024, 548),
    const Size(1280, 668),
    const Size(1920, 1028),
  ]) {
    testWidgets('message panel fits the actual map area $size', (tester) async {
      await tester.binding.setSurfaceSize(const Size(2000, 1200));
      addTearDown(() => tester.binding.setSurfaceSize(null));
      var mapTaps = 0;
      const panelKey = ValueKey('panel');
      await tester.pumpWidget(
        MaterialApp(
          home: Align(
            alignment: Alignment.topLeft,
            child: SizedBox.fromSize(
              size: size,
              child: Stack(
                children: [
                  Positioned.fill(
                    child: GestureDetector(
                      behavior: HitTestBehavior.opaque,
                      onTap: () => mapTaps++,
                    ),
                  ),
                  const MapPanelSlot(
                    preferredWidth: 360,
                    preferredHeight: 340,
                    reserveInstruments: true,
                    child: ColoredBox(key: panelKey, color: Colors.blue),
                  ),
                ],
              ),
            ),
          ),
        ),
      );
      final bounds = tester.getRect(find.byKey(panelKey));
      expect(bounds.left, greaterThanOrEqualTo(18));
      expect(bounds.right, size.width - 18);
      expect(bounds.top, 12);
      final bottomInset = size.height < 450 ? 90 : 226;
      expect(bounds.bottom, lessThanOrEqualTo(size.height - bottomInset));
      expect(bounds.height, lessThanOrEqualTo(340));
      await tester.tapAt(const Offset(100, 100));
      expect(mapTaps, 1);
      expect(tester.takeException(), isNull);
    });
  }

  testWidgets('connection slot keeps the content requested height', (
    tester,
  ) async {
    const panelKey = ValueKey('connection');
    await tester.pumpWidget(
      const MaterialApp(
        home: Stack(
          children: [
            MapPanelSlot(
              preferredWidth: 400,
              child: SizedBox(key: panelKey, height: 276),
            ),
          ],
        ),
      ),
    );
    expect(tester.getSize(find.byKey(panelKey)).height, 276);
    expect(tester.takeException(), isNull);
  });
}
