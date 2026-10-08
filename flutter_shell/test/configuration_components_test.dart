import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/configuration/calibration/widgets/calibration_progress.dart';
import 'package:mgcs_frontend/configuration/pid/models/pid_models.dart';
import 'package:mgcs_frontend/configuration/widgets/mac_tab_surface.dart';
import 'package:mgcs_frontend/configuration/widgets/parameter_helpers.dart';
import 'package:mgcs_frontend/configuration/widgets/parameter_value_well.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

Widget _host(Widget child, {Brightness brightness = Brightness.light}) =>
    MaterialApp(
      home: MacosTheme(
        data: MacosThemeData(brightness: brightness),
        child: Scaffold(body: child),
      ),
    );

void main() {
  testWidgets('external tab controller survives theme rebuild and removal', (
    tester,
  ) async {
    final tabs = MacosTabController(initialIndex: 0, length: 2);
    Widget surface() => MacTabSurface(
      controller: tabs,
      labels: const ['第一页', '第二页'],
      children: const [Text('内容一'), Text('内容二')],
    );
    await tester.pumpWidget(_host(surface()));
    await tester.pumpWidget(_host(surface(), brightness: Brightness.dark));
    tabs.index = 1;
    await tester.pump();
    expect(find.text('内容二'), findsOneWidget);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox.shrink());
    tabs.index = 0;
    expect(tester.takeException(), isNull);
    tabs.dispose();
  });

  testWidgets('owned tab controller is disposed with its surface', (
    tester,
  ) async {
    await tester.pumpWidget(
      _host(
        const MacTabSurface(
          labels: ['第一页', '第二页'],
          children: [Text('内容一'), Text('内容二')],
        ),
      ),
    );
    final tabs = tester
        .widget<MacosSegmentedControl>(find.byType(MacosSegmentedControl))
        .controller;
    await tester.pumpWidget(const SizedBox.shrink());
    expect(() => tabs.addListener(() {}), throwsFlutterError);
    expect(tester.takeException(), isNull);
  });

  testWidgets(
    'calibration progress keeps per-compass values and clamps display',
    (tester) async {
      await tester.pumpWidget(
        _host(
          const CalibrationProgress(
            instances: [
              WaveformInstance(index: 0, active: true, name: 'A'),
              WaveformInstance(index: 1, active: true, name: 'B'),
            ],
            progressByCompass: {0: 120, 1: -5},
            fallbackProgress: 50,
          ),
        ),
      );
      final bars = tester.widgetList<ProgressBar>(find.byType(ProgressBar));
      expect(bars.map((bar) => bar.value), [100, 0]);
      expect(find.text('100%'), findsOneWidget);
      expect(find.text('0%'), findsOneWidget);
    },
  );

  testWidgets(
    'calibration progress falls back when compass instances are absent',
    (tester) async {
      await tester.pumpWidget(
        _host(
          const CalibrationProgress(
            instances: [],
            progressByCompass: {},
            fallbackProgress: 42,
          ),
        ),
      );
      expect(tester.widget<ProgressBar>(find.byType(ProgressBar)).value, 42);
      expect(find.text('42%'), findsOneWidget);
    },
  );

  testWidgets('numeric editor rolls back a rejected parameter write', (
    tester,
  ) async {
    final submitted = <double>[];
    await tester.pumpWidget(
      _host(
        ParameterValueWell(
          text: '1',
          enabled: true,
          center: true,
          onSubmitted: (value) async {
            submitted.add(value);
            return false;
          },
        ),
      ),
    );
    final field = tester.widget<MacosTextField>(find.byType(MacosTextField));
    field.controller!.text = '2.5';
    field.onSubmitted!('2.5');
    await tester.pump();
    expect(submitted, [2.5]);
    expect(field.controller!.text, '1');
    field.controller!.text = 'NaN';
    field.onSubmitted!('NaN');
    await tester.pump();
    expect(submitted, [2.5]);
    expect(field.controller!.text, '1');
    await tester.pumpWidget(const SizedBox.shrink());
    expect(tester.takeException(), isNull);
  });

  testWidgets('numeric editor commits the same value only once', (
    tester,
  ) async {
    var writes = 0;
    await tester.pumpWidget(
      _host(
        ParameterValueWell(
          text: '1',
          enabled: true,
          center: false,
          onSubmitted: (_) async {
            writes++;
            return true;
          },
        ),
      ),
    );
    final field = tester.widget<MacosTextField>(find.byType(MacosTextField));
    field.controller!.text = '3';
    field.onSubmitted!('3');
    await tester.pump();
    field.onSubmitted!('3');
    await tester.pump();
    expect(writes, 1);
    await tester.pumpWidget(const SizedBox.shrink());
    expect(tester.takeException(), isNull);
  });

  test(
    'PID model preserves Copter, fixed-wing and VTOL parameter namespaces',
    () {
      List<String> names(
        VehicleControlProfile profile,
        bool vtol,
        PidLevel level,
      ) => [
        for (final section in pidSections(profile, vtol, level))
          for (final card in section.cards)
            for (final field in card.fields) field.parameterName,
      ];
      final copter = names(VehicleControlProfile.copter, false, PidLevel.basic);
      final vtol = names(VehicleControlProfile.planeVtol, true, PidLevel.basic);
      final plane = names(
        VehicleControlProfile.fixedWing,
        false,
        PidLevel.basic,
      );
      expect(copter, containsAll(['ATC_ANG_RLL_P', 'ATC_RAT_PIT_I']));
      expect(vtol, containsAll(['Q_A_ANG_RLL_P', 'Q_A_RAT_PIT_I']));
      expect(plane, containsAll(['RLL_RATE_P', 'PTCH_RATE_I', 'YAW_RATE_D']));
      expect(plane.any((name) => name.startsWith('ATC_')), isFalse);
      expect(
        names(VehicleControlProfile.fixedWing, false, PidLevel.extended),
        contains('TECS_TIME_CONST'),
      );
      expect(
        names(VehicleControlProfile.unknown, false, PidLevel.basic),
        isEmpty,
      );
    },
  );

  test(
    'parameter helpers preserve metadata fallback and numeric formatting',
    () {
      expect(enumOptionsFor(null, [4, 8, 16]), [4, 8, 16]);
      expect(enumLabelFor(null, 16), '16');
      expect(formatParameterValue(2), '2');
      expect(formatParameterValue(2.5), '2.5');
    },
  );
}
