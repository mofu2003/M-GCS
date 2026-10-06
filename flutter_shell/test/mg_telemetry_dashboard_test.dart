import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/widgets/mg_telemetry_dashboard.dart';

void main() {
  Future<void> show(WidgetTester tester, MgTelemetryDashboardData data) async {
    tester.view.physicalSize = const Size(1200, 240);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      MacosTheme(
        data: MacosThemeData.light(),
        child: MaterialApp(
          home: Scaffold(body: MgTelemetryDashboard(data: data)),
        ),
      ),
    );
    await tester.pumpAndSettle();
  }

  test('flight time matches the QGC hh:mm:ss format', () {
    expect(formatFlightTime(0), '00:00:00');
    expect(formatFlightTime(9), '00:00:09');
    expect(formatFlightTime(64), '00:01:04');
    expect(formatFlightTime(3725), '01:02:05');
  });

  testWidgets('dashboard shows the QGC default panel values in Chinese', (
    tester,
  ) async {
    await show(
      tester,
      const MgTelemetryDashboardData(
        relativeAltitudeMeters: 21.2,
        distanceToHomeMeters: 0.1,
        climbRateMetersPerSecond: -0.2,
        groundSpeedMetersPerSecond: 0,
        airSpeedMetersPerSecond: 0.3,
        throttlePercent: 42,
        flightTimeSeconds: 64,
        flightDistanceMeters: 12.5,
      ),
    );

    for (final label in const [
      '相对高度',
      '距 Home',
      '垂直速度',
      '地速',
      '空速',
      '油门',
      '飞行时间',
      '飞行里程',
    ]) {
      expect(find.text(label), findsOneWidget);
    }
    expect(find.text('21.2'), findsOneWidget);
    expect(find.text('00:01:04'), findsOneWidget);
    expect(find.text('12.5'), findsOneWidget);
    expect(find.text('42'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('missing telemetry falls back to placeholders', (tester) async {
    await show(
      tester,
      const MgTelemetryDashboardData(
        relativeAltitudeMeters: null,
        distanceToHomeMeters: null,
        climbRateMetersPerSecond: null,
        groundSpeedMetersPerSecond: null,
        airSpeedMetersPerSecond: null,
        throttlePercent: null,
        flightTimeSeconds: 0,
        flightDistanceMeters: 0,
      ),
    );

    expect(find.text('—'), findsNWidgets(6));
    expect(tester.takeException(), isNull);
  });
}
