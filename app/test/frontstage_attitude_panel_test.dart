import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/presentation/features/frontstage/frontstage_models.dart';
import 'package:mgcs/presentation/ui/skins/mac_glass/mac_glass_attitude_panel.dart';

void main() {
  testWidgets(
    'MacGlass attitude panel stays at the supplied telemetry values',
    (WidgetTester tester) async {
      await tester.pumpWidget(
        const MaterialApp(
          home: Scaffold(
            body: MacGlassAttitudePanel(
              model: FrontstageAttitudeModel(
                pitchDegrees: 0,
                rollDegrees: 0,
                headingDegrees: 0,
                altitudeLabel: '--',
                airspeedLabel: '--',
                groundSpeedLabel: '--',
                verticalSpeedLabel: '--',
                batteryLabel: '--',
              ),
            ),
          ),
        ),
      );

      expect(find.text('000°'), findsWidgets);

      await tester.pump(const Duration(seconds: 1));

      expect(find.text('000°'), findsWidgets);
    },
  );

  testWidgets('MacGlass attitude panel renders attitude-first telemetry', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: MacGlassAttitudePanel(
            model: FrontstageAttitudeModel(
              pitchDegrees: 0,
              rollDegrees: 0,
              headingDegrees: 86,
              altitudeLabel: '12.4m',
              airspeedLabel: '10.5m/s',
              groundSpeedLabel: '0.8m/s',
              verticalSpeedLabel: '-0.4m/s',
              batteryLabel: '12.1V',
            ),
          ),
        ),
      ),
    );

    expect(find.byKey(const ValueKey('mg-attitude-panel')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-attitude-instrument')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-attitude-reference-dial')),
      findsOneWidget,
    );
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-attitude-reference-dial'))),
      const Size.square(184),
    );
    expect(find.byKey(const ValueKey('mg-compass-dial-layer')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-attitude-ball-layer')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-heading-pointer-layer')),
      findsOneWidget,
    );
    expect(find.text('姿态仪'), findsNothing);
    expect(
      find.byKey(const ValueKey('mg-attitude-flight-metrics')),
      findsOneWidget,
    );
    expect(find.text('PITCH'), findsNothing);
    expect(find.text('ROLL'), findsNothing);
    expect(find.text('HDG'), findsNothing);
    expect(find.text('俯仰'), findsOneWidget);
    expect(find.text('横滚'), findsOneWidget);
    expect(find.text('航向'), findsOneWidget);
    expect(find.text('空速'), findsOneWidget);
    expect(find.text('地速'), findsOneWidget);
    expect(find.text('升降速度'), findsOneWidget);
    expect(find.text('高度'), findsNothing);
    expect(find.text('飞行距离'), findsNothing);
    expect(find.text('飞行时间'), findsNothing);
    expect(find.text('12.4m'), findsNothing);
    expect(find.text('0.8m/s'), findsOneWidget);
    expect(find.text('086°'), findsOneWidget);
    expect(find.text('12.1V'), findsNothing);
    expect(find.byKey(const ValueKey('mg-attitude-readouts')), findsNothing);

    final airspeedTile = find.byKey(
      const ValueKey('mg-telemetry-tile-airspeed'),
    );
    expect(airspeedTile, findsOneWidget);
    expect(tester.getSize(airspeedTile).height, 60);
    final airspeedLabel = find.descendant(
      of: airspeedTile,
      matching: find.text('空速'),
    );
    final airspeedValue = find.descendant(
      of: airspeedTile,
      matching: find.text('10.5m/s'),
    );
    expect(
      tester.getCenter(airspeedLabel).dy,
      lessThan(tester.getCenter(airspeedValue).dy),
    );

    final headingTile = tester.getRect(
      find.byKey(const ValueKey('mg-telemetry-tile-heading')),
    );
    final pitchTile = tester.getRect(
      find.byKey(const ValueKey('mg-telemetry-tile-pitch')),
    );
    final rollTile = tester.getRect(
      find.byKey(const ValueKey('mg-telemetry-tile-roll')),
    );
    final airspeedRect = tester.getRect(airspeedTile);
    final groundSpeedTile = tester.getRect(
      find.byKey(const ValueKey('mg-telemetry-tile-groundspeed')),
    );
    final verticalSpeedTile = tester.getRect(
      find.byKey(const ValueKey('mg-telemetry-tile-vertical-speed')),
    );

    expect(headingTile.left, closeTo(pitchTile.left, 0.01));
    expect(pitchTile.left, closeTo(rollTile.left, 0.01));
    expect(headingTile.top, lessThan(pitchTile.top));
    expect(pitchTile.top, lessThan(rollTile.top));
    expect(airspeedRect.left, greaterThan(headingTile.left));
    expect(headingTile.top, closeTo(airspeedRect.top, 0.01));
    expect(pitchTile.top, closeTo(groundSpeedTile.top, 0.01));
    expect(rollTile.top, closeTo(verticalSpeedTile.top, 0.01));
    expect(find.text('-0.4m/s'), findsOneWidget);
  });
}
