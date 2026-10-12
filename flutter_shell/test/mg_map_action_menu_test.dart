import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_map_action_menu.dart';

void main() {
  group('map action gating matches QGC', () {
    test('on the ground only set home appears', () {
      final visibility = mapActionVisibility(
        guidedEnabled: true,
        flying: false,
        homeReady: true,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: true,
        supportsRoi: true,
        supportsChangeHeading: true,
      );
      expect(visibility.gotoLocation, isFalse);
      expect(visibility.orbit, isFalse);
      expect(visibility.roi, isFalse);
      expect(visibility.setHome, isTrue);
      expect(visibility.changeHeading, isFalse);
    });

    test('flying multirotor gets goto, orbit, roi, home and heading', () {
      final visibility = mapActionVisibility(
        guidedEnabled: true,
        flying: true,
        homeReady: true,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: true,
        supportsRoi: true,
        supportsChangeHeading: true,
      );
      expect(visibility.gotoLocation, isTrue);
      expect(visibility.orbit, isTrue);
      expect(visibility.roi, isTrue);
      expect(visibility.setHome, isTrue);
      expect(visibility.changeHeading, isTrue);
      expect(visibility.setEstimatorOrigin, isFalse);
    });

    test('flying VTOL keeps goto/roi/home but drops heading', () {
      final visibility = mapActionVisibility(
        guidedEnabled: true,
        flying: true,
        homeReady: true,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: true,
        supportsRoi: true,
        supportsChangeHeading: false, // VTOL 不在 QGC 的 ChangeHeading 名单里
      );
      expect(visibility.gotoLocation, isTrue);
      expect(visibility.roi, isTrue);
      expect(visibility.changeHeading, isFalse);
    });

    test('orbit and ROI follow the firmware capability, not guided mode', () {
      // 固件声明支持引导但没给 orbit/roi 时，
      // 地图上不应该出现飞控不执行的入口。
      final fixedWing = mapActionVisibility(
        guidedEnabled: true,
        flying: true,
        homeReady: true,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: true,
        supportsRoi: false,
        supportsChangeHeading: false,
      );
      expect(fixedWing.gotoLocation, isTrue);
      expect(fixedWing.orbit, isTrue);
      expect(fixedWing.roi, isFalse);

      final noOrbit = mapActionVisibility(
        guidedEnabled: true,
        flying: true,
        homeReady: true,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: false,
        supportsRoi: true,
        supportsChangeHeading: false,
      );
      expect(noOrbit.orbit, isFalse);
      expect(noOrbit.roi, isTrue);
    });

    test('orbit is hidden while a mission is active', () {
      final visibility = mapActionVisibility(
        guidedEnabled: true,
        flying: true,
        homeReady: true,
        missionActive: true,
        hasGpsSensor: true,
        supportsOrbit: true,
        supportsRoi: true,
        supportsChangeHeading: true,
      );
      expect(visibility.orbit, isFalse);
      expect(visibility.gotoLocation, isTrue);
    });

    test('estimator origin only appears without a GPS sensor', () {
      final withGps = mapActionVisibility(
        guidedEnabled: true,
        flying: false,
        homeReady: false,
        missionActive: false,
        hasGpsSensor: true,
        supportsOrbit: false,
        supportsRoi: false,
        supportsChangeHeading: false,
      );
      expect(withGps.setEstimatorOrigin, isFalse);

      final noGps = mapActionVisibility(
        guidedEnabled: false,
        flying: false,
        homeReady: false,
        missionActive: false,
        hasGpsSensor: false,
        supportsOrbit: false,
        supportsRoi: false,
        supportsChangeHeading: false,
      );
      expect(noGps.setEstimatorOrigin, isTrue);
      expect(noGps.setHome, isFalse);
    });
  });

  testWidgets('menu renders the actions and the clicked coordinates', (
    tester,
  ) async {
    var gotoSelected = false;
    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(
            body: Center(
              child: MgMapActionMenu(
                latitude: 32.544271,
                longitude: 114.737054,
                actions: [
                  MgMapAction(
                    label: '移动到此处',
                    onSelected: () => gotoSelected = true,
                  ),
                  MgMapAction(label: '在此设置 Home', onSelected: () {}),
                ],
              ),
            ),
          ),
        ),
      ),
    );

    expect(find.text('移动到此处'), findsOneWidget);
    expect(find.text('在此设置 Home'), findsOneWidget);
    expect(find.text('纬度 32.544271'), findsOneWidget);
    expect(find.text('经度 114.737054'), findsOneWidget);

    await tester.tap(find.text('移动到此处'));
    await tester.pumpAndSettle();
    expect(gotoSelected, isTrue);
    expect(tester.takeException(), isNull);
  });
}
