import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/theme/mg_material_theme.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/quick_mission_draft.dart';
import 'package:mgcs_frontend/mission/quick_mission_geometry.dart';
import 'package:mgcs_frontend/mission/quick_mission_panel.dart';
import 'package:latlong2/latlong.dart';

void main() {
  for (final size in [
    const Size(460, 520),
    const Size(460, 440),
    const Size(420, 360),
    const Size(360, 360),
    const Size(760, 200),
    const Size(360, 200),
  ]) {
    for (final shape in QuickMissionShape.values) {
      testWidgets('generator is fully visible for $shape in $size', (
        tester,
      ) async {
        final draft = QuickMissionDraft(shape, altitudeMeters: 50, frame: 3)
          ..addPoint(const LatLng(32.5, 114.7));
        draft.addPoint(const LatLng(32.501, 114.701));
        if (shape == QuickMissionShape.polygon) {
          draft.addPoint(const LatLng(32.5, 114.702));
        }
        final count = draft.generate().length;
        final inputCount = shape == QuickMissionShape.circle ? 4 : 2;
        addTearDown(draft.dispose);
        var generated = 0;
        await tester.pumpWidget(
          MacosApp(
            builder: (context, child) => MgMaterialTheme(child: child!),
            home: Scaffold(
              body: Align(
                alignment: Alignment.topLeft,
                child: SizedBox.fromSize(
                  size: size,
                  child: QuickMissionPanel(
                    draft: draft,
                    onGenerate: () async {
                      generated++;
                    },
                    onCancel: () {},
                  ),
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.textContaining('将生成 $count 个航点'), findsOneWidget);
        final button = find.widgetWithText(PushButton, '生成航点');
        expect(find.byType(MacosTextField), findsNWidgets(inputCount));
        expect(find.byType(OutlinedButton), findsNothing);
        expect(find.byType(SingleChildScrollView), findsNothing);
        if (size.height >= 440) {
          expect(find.text('形状参数'), findsOneWidget);
          expect(find.text('飞行高度'), findsOneWidget);
        }
        expect(
          find.byType(MacosTextField).hitTestable(),
          findsNWidgets(inputCount),
        );
        final fields = find.byType(MacosTextField);
        if (size.height >= 300 && shape == QuickMissionShape.circle) {
          expect(
            tester.getCenter(find.text('首点角度')).dy,
            tester.getCenter(find.text('方向')).dy,
          );
          expect(
            tester.getTopLeft(fields.at(0)).dy,
            tester.getTopLeft(fields.at(1)).dy,
          );
        }
        expect(button.hitTestable(), findsOneWidget);
        await tester.ensureVisible(button);
        await tester.pumpAndSettle();
        await tester.tap(button);
        await tester.pumpAndSettle();
        expect(generated, 1);
        final radius = find.byType(MacosTextField).first;
        await tester.ensureVisible(radius);
        await tester.enterText(radius, 'abc');
        await tester.tap(button);
        await tester.pumpAndSettle();
        expect(generated, 1);
        if (size.height >= 300) {
          expect(find.text('请输入大于 0 的数字'), findsOneWidget);
        }
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      });
    }
  }
  test(
    'circle requires a clicked first waypoint and preserves its position',
    () {
      final draft = QuickMissionDraft(
        QuickMissionShape.circle,
        altitudeMeters: 50,
        frame: 3,
      )..addPoint(const LatLng(32.5, 114.7));
      addTearDown(draft.dispose);
      expect(draft.generate, throwsArgumentError);
      const first = LatLng(32.501, 114.702);
      draft.addPoint(first);
      expect(draft.generate().first, first);
      expect(draft.generate().last, first);
      draft.clockwise = false;
      expect(draft.generate().first, first);
      const moved = LatLng(32.499, 114.699);
      draft.movePoint(1, moved);
      expect(draft.generate().first, moved);
      draft.startBearing = 90;
      draft.update();
      final expected = quickMissionDestination(
        draft.controls.first,
        90,
        draft.radiusMeters,
      );
      expect(draft.controls[1], expected);
      expect(draft.generate().first, expected);
      draft.removeLastPoint();
      expect(draft.generate, throwsArgumentError);
    },
  );
  test('moving circle center retains radius and moves its handle', () {
    final draft =
        QuickMissionDraft(
            QuickMissionShape.circle,
            altitudeMeters: 50,
            frame: 3,
          )
          ..addPoint(const LatLng(32.5, 114.7))
          ..addPoint(const LatLng(32.501, 114.7));
    final radius = draft.radiusMeters;
    draft.movePoint(0, const LatLng(32.6, 114.8));
    expect(draft.radiusMeters, closeTo(radius, 0.001));
    expect(draft.controls[1].latitude, greaterThan(32.6));
    draft.dispose();
  });
}
