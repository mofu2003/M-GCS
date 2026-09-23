import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/calibration_models.dart';
import 'package:mgcs/presentation/controllers/connection_controller.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';
import 'package:mgcs/infrastructure/demo/demo_connection_session_backend.dart';
import 'package:mgcs/presentation/controllers/parameter_controller.dart';
import 'package:mgcs/application/parameter_models.dart';
import 'package:mgcs/presentation/features/backstage/backstage_page.dart';
import 'package:mgcs/presentation/ui/skins/mac_glass/mac_glass_tokens.dart';

void main() {
  testWidgets('parameter tree uses one secondary title style for every group', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final parameterBackend = _ParameterTreeBackend();
    final parameterController = ParameterController(
      backend: parameterBackend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);

    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pump();

    expect(find.text('全部参数'), findsOneWidget);
    expect(find.text('参数配置'), findsOneWidget);
    expect(find.text('读取'), findsOneWidget);
    expect(find.text('保存'), findsOneWidget);
    expect(find.text('刷新'), findsOneWidget);
    expect(find.text('写入'), findsOneWidget);
    final category = tester.widget<Text>(find.text('ATC').first);
    expect(category.style?.fontSize, 13);
    expect(category.style?.fontWeight, FontWeight.w700);
    expect(category.style?.fontFamily, isNull);

    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-tree-toggle-ATC_')),
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('mg-parameter-tree-branch-ATC_RAT_')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('mg-parameter-tree-leaf-ATC_RAT_RLL_I')),
      findsOneWidget,
    );
    final secondCategory = tester.widget<Text>(find.text('BATT').first);
    expect(secondCategory.style?.fontSize, 13);
    expect(secondCategory.style?.fontWeight, FontWeight.w700);
    expect(secondCategory.style?.fontFamily, isNull);
    expect(secondCategory.style?.color, MacGlassTokens.ink);

    final leaf = tester.widget<Text>(
      find.byKey(const ValueKey('mg-parameter-tree-leaf-ATC_RAT_RLL_I')),
    );
    expect(leaf.style?.fontFamily, 'monospace');
    expect(leaf.style?.fontSize, 11.5);

    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-tree-toggle-ATC_')),
    );
    await tester.pumpAndSettle();
    expect(
      find.byKey(const ValueKey('mg-parameter-tree-branch-ATC_RAT_')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('mg-parameter-tree-leaf-ATC_RAT_RLL_I')),
      findsNothing,
    );
    expect(
      tester.widget<Text>(find.text('ATC').first).style?.color,
      MacGlassTokens.blue,
    );
  });

  testWidgets('PID page infers the Copter layout from downloaded parameters', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final parameterController = ParameterController(
      backend: _ParameterTreeBackend(),
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('mg-backstage-pid-tuning')));
    await tester.pumpAndSettle();

    expect(find.text('多旋翼姿态控制'), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-pid-copter-page')), findsOneWidget);
    expect(find.text('角速率控制'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-pid-copter-rate-roll')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-pid-copter-rate-pitch')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-pid-copter-rate-yaw')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('mg-pid-stepper-ATC_RAT_RLL_P-increment')),
      findsOneWidget,
    );
    await tester.tap(
      find.byKey(const ValueKey('mg-pid-stepper-ATC_RAT_RLL_P-increment')),
    );
    await tester.pumpAndSettle();
    expect(parameterController.stagedChanges, isEmpty);
    expect(find.text('固定翼控制器'), findsNothing);
    expect(find.text('固定翼轴速率'), findsNothing);
  });

  testWidgets('parameter metadata selects enum boolean and bitmask editors', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final backend = _ParameterEditorBackend();
    final parameterController = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();

    final returnButtonCenter = tester.getCenter(
      find.byKey(const ValueKey('mg-backstage-return-frontstage')),
    );

    await tester.tap(find.text('SERVO1_FUNCTION'));
    await tester.pumpAndSettle();
    final enumKey = find.byKey(
      const ValueKey('mg-parameter-editor-enum-SERVO1_FUNCTION'),
    );
    expect(enumKey, findsOneWidget);
    await tester.tap(enumKey);
    await tester.pumpAndSettle();
    await tester.tap(find.text('70 · 油门').last);
    await tester.pumpAndSettle();
    expect(
      parameterController.stagedChanges
          .singleWhere((change) => change.parameter.name == 'SERVO1_FUNCTION')
          .stagedValue,
      70,
    );
    expect(
      tester.getCenter(
        find.byKey(const ValueKey('mg-backstage-return-frontstage')),
      ),
      returnButtonCenter,
    );

    await tester.tap(find.text('SERIAL1_PROTOCOL'));
    await tester.pumpAndSettle();
    expect(
      find.byKey(const ValueKey('mg-parameter-editor-enum-SERIAL1_PROTOCOL')),
      findsOneWidget,
    );
    expect(find.text('当前值 2 · MAVLink2'), findsOneWidget);

    await tester.tap(find.text('BATT_ENABLE'));
    await tester.pumpAndSettle();
    final boolKey = find.byKey(
      const ValueKey('mg-parameter-editor-bool-BATT_ENABLE'),
    );
    expect(boolKey, findsOneWidget);
    await tester.tap(boolKey);
    await tester.pumpAndSettle();
    expect(
      parameterController.stagedChanges
          .singleWhere((change) => change.parameter.name == 'BATT_ENABLE')
          .stagedValue,
      0,
    );

    await tester.tap(find.text('SERIAL1_OPTIONS'));
    await tester.pumpAndSettle();
    final bitmaskKey = find.byKey(
      const ValueKey('mg-parameter-editor-bitmask-SERIAL1_OPTIONS-2'),
    );
    expect(bitmaskKey, findsOneWidget);
    await tester.tap(bitmaskKey);
    await tester.pumpAndSettle();
    expect(
      parameterController.stagedChanges
          .singleWhere((change) => change.parameter.name == 'SERIAL1_OPTIONS')
          .stagedValue,
      3,
    );
  });

  testWidgets('servo output page writes channels 1 through 16 immediately', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final backend = _ServoEditorBackend();
    final parameterController = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-backstage-servo-outputs')));
    await tester.pump(const Duration(milliseconds: 300));

    expect(find.text('电机输出'), findsOneWidget);
    expect(find.text('读取'), findsNothing);
    expect(find.text('最小'), findsAtLeastNWidgets(1));
    expect(find.text('中位'), findsAtLeastNWidgets(1));
    expect(find.text('最大'), findsAtLeastNWidgets(1));
    expect(find.text('微调'), findsNothing);
    expect(find.byKey(const ValueKey('mg-servo-channel-1')), findsOneWidget);
    expect(find.text('33 · 电机 1'), findsOneWidget);
    await tester.scrollUntilVisible(
      find.byKey(const ValueKey('mg-servo-channel-16')),
      300,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-servo-output-list')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    expect(find.byKey(const ValueKey('mg-servo-channel-16')), findsOneWidget);
    expect(find.text('1516 PWM'), findsOneWidget);

    await tester.scrollUntilVisible(
      find.byKey(const ValueKey('mg-servo-1-function')),
      -1000,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-servo-output-list')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('mg-servo-1-function')));
    await tester.pumpAndSettle();
    await tester.tap(find.text('70 · 油门').last);
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('mg-servo-1-reversed')));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.byKey(const ValueKey('mg-servo-1-min')),
      '1050',
    );
    await tester.testTextInput.receiveAction(TextInputAction.done);
    await tester.pumpAndSettle();

    expect(parameterController.stagedChanges, isEmpty);
    final values = {
      for (final parameter in parameterController.parameters)
        parameter.name: parameter.value,
    };
    expect(values['SERVO1_FUNCTION'], 70);
    expect(values['SERVO1_REVERSED'], 1);
    expect(values['SERVO1_MIN'], 1050);
    expect(backend.writeCount, 3);
  });

  testWidgets('serial page detects available ports and writes immediately', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final backend = _SerialEditorBackend();
    final parameterController = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-backstage-serial-ports')));
    await tester.pumpAndSettle();

    expect(find.text('端口配置'), findsAtLeastNWidgets(1));
    expect(find.text('2 个可用串口 · 修改后立即写入飞控'), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-serial-port-0')), findsOneWidget);
    expect(find.byKey(const ValueKey('mg-serial-port-1')), findsNothing);
    expect(find.byKey(const ValueKey('mg-serial-port-2')), findsOneWidget);
    expect(find.text('OTG1'), findsOneWidget);
    expect(find.text('USART3'), findsOneWidget);
    expect(find.text('读取'), findsNothing);
    expect(find.text('57 · 57600'), findsNothing);
    expect(find.text('57600'), findsOneWidget);

    final protocolRect = tester.getRect(
      find.byKey(const ValueKey('mg-serial-2-protocol')),
    );
    final baudRect = tester.getRect(
      find.byKey(const ValueKey('mg-serial-2-baud')),
    );
    final optionsRect = tester.getRect(
      find.byKey(const ValueKey('mg-serial-2-options')),
    );
    expect(protocolRect.center.dy, baudRect.center.dy);
    expect(protocolRect.right, lessThan(baudRect.left));
    expect(optionsRect.top - baudRect.bottom, closeTo(18, 0.1));
    expect(
      tester.getSize(find.byKey(const ValueKey('mg-serial-port-2'))).height,
      lessThanOrEqualTo(176),
    );
    expect(tester.takeException(), isNull);

    await tester.tap(find.byKey(const ValueKey('mg-serial-2-flow-control')));
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-editor-enum-option-1')),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-serial-2-baud')));
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-editor-enum-option-115')),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-serial-2-protocol')));
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-editor-enum-option-1')),
    );
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-serial-2-options')));
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-serial-option-SERIAL2_OPTIONS-1')),
    );
    await tester.pumpAndSettle();

    expect(parameterController.stagedChanges, isEmpty);
    expect(backend.writes, <(String, double)>[
      ('BRD_SER2_RTSCTS', 1),
      ('SERIAL2_BAUD', 115),
      ('SERIAL2_PROTOCOL', 1),
      ('SERIAL2_OPTIONS', 0),
      ('SERIAL2_OPTIONS', 1),
    ]);
  });

  testWidgets('remote control page maps channels and calibrates four axes', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final backend = _RemoteControlEditorBackend();
    final parameterController = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('mg-backstage-remote-control')));
    await tester.pump(const Duration(milliseconds: 150));

    expect(find.text('遥控配置'), findsOneWidget);
    expect(find.text('通道映射'), findsOneWidget);
    expect(find.text('摇杆校准'), findsOneWidget);
    expect(find.text('飞行模式'), findsOneWidget);
    expect(find.text('RC 通道功能'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-remote-flight-mode-5')),
      findsOneWidget,
    );
    await tester.scrollUntilVisible(
      find.byKey(const ValueKey('mg-remote-aux-channel-16')),
      300,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-remote-aux-channel-list')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.pump();
    expect(
      find.byKey(const ValueKey('mg-remote-aux-channel-16')),
      findsOneWidget,
    );

    await tester.tap(
      find.byKey(const ValueKey('mg-remote-aux-channel-16-option')),
    );
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-editor-enum-option-0')),
    );
    await tester.pumpAndSettle();
    expect(backend.writes.last, ('RC16_OPTION', 0));
    final writeCountBeforeCalibration = backend.writes.length;

    await tester.tap(find.text('摇杆校准'));
    await tester.pump(const Duration(milliseconds: 100));
    expect(find.text('横滚'), findsOneWidget);
    expect(find.text('俯仰'), findsOneWidget);
    expect(find.text('油门'), findsOneWidget);
    expect(find.text('偏航'), findsOneWidget);

    await tester.tap(
      find.byKey(const ValueKey('mg-remote-calibration-primary')),
    );
    backend.inputs = <int>[990, 1050, 900, 1100, ...List<int>.filled(14, 1500)];
    await tester.pump(const Duration(milliseconds: 120));
    backend.inputs = <int>[
      2010,
      1950,
      2100,
      1900,
      ...List<int>.filled(14, 1500),
    ];
    await tester.pump(const Duration(milliseconds: 120));
    await tester.tap(
      find.byKey(const ValueKey('mg-remote-calibration-primary')),
    );
    await tester.pump();

    backend.inputs = <int>[
      1510,
      1490,
      950,
      1520,
      ...List<int>.filled(14, 1500),
    ];
    await tester.pump(const Duration(milliseconds: 120));
    await tester.tap(
      find.byKey(const ValueKey('mg-remote-calibration-primary')),
    );
    await tester.pumpAndSettle();

    expect(
      backend.writes.skip(writeCountBeforeCalibration).toList(),
      <(String, double)>[
        ('RC1_MIN', 990),
        ('RC1_TRIM', 1510),
        ('RC1_MAX', 2010),
        ('RC2_MIN', 1050),
        ('RC2_TRIM', 1490),
        ('RC2_MAX', 1950),
        ('RC3_MIN', 900),
        ('RC3_TRIM', 950),
        ('RC3_MAX', 2100),
        ('RC4_MIN', 1100),
        ('RC4_TRIM', 1520),
        ('RC4_MAX', 1900),
      ],
    );
    expect(parameterController.stagedChanges, isEmpty);

    await tester.tap(
      find.byKey(const ValueKey('mg-remote-axis-rcmap_roll-channel')),
    );
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('mg-parameter-editor-enum-option-2')),
    );
    await tester.pumpAndSettle();
    expect(backend.writes.last, ('RCMAP_ROLL', 2));
  });

  testWidgets(
    'remote control page writes axis reversal and elevon output mapping immediately',
    (WidgetTester tester) async {
      tester.view.physicalSize = const Size(1200, 800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      final backend = _RemoteControlEditorBackend();
      final parameterController = ParameterController(
        backend: backend,
        isSessionReady: () => true,
      );
      await parameterController.refresh();
      addTearDown(parameterController.dispose);
      final connectionController = ConnectionController(
        backend: DemoConnectionSessionBackend(),
      );
      addTearDown(connectionController.dispose);

      await tester.pumpWidget(
        MaterialApp(
          home: BackstagePage(
            connectionController: connectionController,
            parameterController: parameterController,
            onReturnToFrontstage: () {},
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(
        find.byKey(const ValueKey('mg-backstage-remote-control')),
      );
      await tester.pump(const Duration(milliseconds: 150));
      expect(
        find.byKey(const ValueKey('mg-remote-modern-elevon-settings')),
        findsOneWidget,
      );
      expect(find.text('副翼输出配置'), findsOneWidget);
      expect(find.text('副翼混控'), findsNothing);
      await tester.tap(
        find.byKey(const ValueKey('mg-remote-modern-elevon-左副翼-reversed')),
      );
      await tester.pumpAndSettle();
      expect(backend.writes.last, ('SERVO1_REVERSED', 1));
      final writeCountBeforeElevonOutput = backend.writes.length;
      await tester.tap(
        find.byKey(const ValueKey('mg-remote-modern-elevon-左副翼-output')),
      );
      await tester.pumpAndSettle();
      await tester.tap(
        find.byKey(const ValueKey('mg-parameter-editor-enum-option-2')),
      );
      await tester.pumpAndSettle();
      expect(
        backend.writes.skip(writeCountBeforeElevonOutput).toList(),
        <(String, double)>[('SERVO2_FUNCTION', 77), ('SERVO1_FUNCTION', 0)],
      );
      await tester.tap(find.text('摇杆校准'));
      await tester.pump(const Duration(milliseconds: 100));

      for (final label in ['副翼', '升降舵', '动力', '方向舵']) {
        expect(find.text(label), findsOneWidget);
      }
      for (final mappingName in [
        'rcmap_roll',
        'rcmap_pitch',
        'rcmap_throttle',
        'rcmap_yaw',
      ]) {
        expect(
          find.byKey(ValueKey('mg-remote-axis-$mappingName-reversed')),
          findsOneWidget,
        );
      }
      await tester.tap(
        find.byKey(const ValueKey('mg-remote-axis-rcmap_pitch-reversed')),
      );
      await tester.pumpAndSettle();
      expect(backend.writes.last, ('RC2_REVERSED', 1));
      expect(parameterController.stagedChanges, isEmpty);
    },
  );

  testWidgets('calibration page exposes all sensor workflows', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 1100);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final backend = _CalibrationEditorBackend();
    final parameterController = ParameterController(
      backend: backend,
      isSessionReady: () => true,
    );
    await parameterController.refresh();
    addTearDown(parameterController.dispose);
    final connectionController = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    addTearDown(connectionController.dispose);

    await tester.pumpWidget(
      MaterialApp(
        home: BackstagePage(
          connectionController: connectionController,
          parameterController: parameterController,
          onReturnToFrontstage: () {},
        ),
      ),
    );
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('mg-backstage-calibration')));
    await tester.pumpAndSettle();

    expect(find.text('传感器/外设配置'), findsOneWidget);
    expect(find.text('读取'), findsNothing);
    expect(find.text('IMU'), findsOneWidget);
    expect(find.text('罗盘'), findsOneWidget);
    expect(find.text('GPS'), findsOneWidget);
    expect(find.text('空速计'), findsOneWidget);
    expect(find.text('最近一次六面校准'), findsOneWidget);
    expect(find.text('极佳 Excellent'), findsOneWidget);
    expect(
      find.byKey(const ValueKey('mg-calibration-accel-step-6')),
      findsOneWidget,
    );

    final sixFaceButton = find.byKey(
      const ValueKey('mg-calibration-accel-six-face'),
    );
    await tester.scrollUntilVisible(
      sixFaceButton,
      180,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-calibration-accelerometer')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.tap(sixFaceButton);
    await tester.pumpAndSettle();
    await tester.tap(find.text('开始').last);
    await tester.pumpAndSettle();
    expect(backend.starts.single.$1, SensorCalibrationType.accelerometer);
    expect(backend.starts.single.$2, AccelerometerCalibrationMode.sixFace);

    final confirmButton = find.byKey(
      const ValueKey('mg-calibration-accel-confirm'),
    );
    await tester.scrollUntilVisible(
      confirmButton,
      180,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-calibration-accelerometer')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.tap(confirmButton);
    await tester.pumpAndSettle();
    expect(backend.confirmedSteps, [1]);

    await tester.tap(find.text('取消').last);
    await tester.pumpAndSettle();
    await tester.tap(find.text('取消校准').last);
    await tester.pumpAndSettle();

    await tester.tap(
      find.byKey(const ValueKey('mg-calibration-tab-magnetometer')),
    );
    await tester.pumpAndSettle();
    expect(find.text('最近一次罗盘校准结果'), findsOneWidget);
    expect(find.text('良好 Good'), findsOneWidget);
    expect(find.text('自动检测安装方向'), findsOneWidget);
    expect(find.text('自动磁偏角'), findsOneWidget);
    final magnetometerStart = find.byKey(
      const ValueKey('mg-calibration-mag-start'),
    );
    await tester.scrollUntilVisible(
      magnetometerStart,
      180,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-calibration-magnetometer')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.tap(magnetometerStart);
    await tester.pumpAndSettle();
    await tester.tap(find.text('开始').last);
    await tester.pumpAndSettle();
    expect(backend.starts.last.$1, SensorCalibrationType.magnetometer);
    expect(find.text('42%'), findsOneWidget);

    await tester.tap(find.text('取消校准'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('取消校准').last);
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const ValueKey('mg-calibration-tab-gps')));
    await tester.pumpAndSettle();
    expect(find.text('GPS 状态'), findsOneWidget);
    expect(find.text('自动配置 GPS'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('mg-calibration-tab-airspeed')));
    await tester.pumpAndSettle();
    expect(find.text('当前偏移量 12.5'), findsOneWidget);
    final airspeedStart = find.byKey(
      const ValueKey('mg-calibration-airspeed-start'),
    );
    await tester.scrollUntilVisible(
      airspeedStart,
      180,
      scrollable: find
          .descendant(
            of: find.byKey(const ValueKey('mg-calibration-airspeed')),
            matching: find.byType(Scrollable),
          )
          .first,
    );
    await tester.tap(airspeedStart);
    await tester.pumpAndSettle();
    await tester.tap(find.text('开始').last);
    await tester.pumpAndSettle();
    expect(backend.starts.last.$1, SensorCalibrationType.airspeed);
    expect(find.text('已发送'), findsOneWidget);
  });

  testWidgets(
    'safety settings writes fence arming and battery controls immediately',
    (WidgetTester tester) async {
      tester.view.physicalSize = const Size(1200, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);

      final backend = _SafetySettingsBackend();
      final parameterController = ParameterController(
        backend: backend,
        isSessionReady: () => true,
      );
      await parameterController.refresh();
      addTearDown(parameterController.dispose);
      final connectionController = ConnectionController(
        backend: DemoConnectionSessionBackend(),
      );
      addTearDown(connectionController.dispose);

      await tester.pumpWidget(
        MaterialApp(
          home: BackstagePage(
            connectionController: connectionController,
            parameterController: parameterController,
            onReturnToFrontstage: () {},
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('mg-backstage-safety')));
      await tester.pumpAndSettle();

      expect(find.text('安全设置'), findsOneWidget);
      expect(find.text('电子围栏'), findsWidgets);
      expect(find.text('解锁检查'), findsOneWidget);
      expect(find.text('电池检查'), findsOneWidget);
      expect(
        find.byKey(const ValueKey('mg-safety-fence-alt-max')),
        findsOneWidget,
      );
      await tester.tap(find.byType(cupertino.CupertinoSwitch).first);
      await tester.pumpAndSettle();
      expect(backend.writes.single, ('FENCE_ENABLE', 1));

      await tester.tap(
        find.descendant(
          of: find.byKey(const ValueKey('mg-safety-fence-alt-max')),
          matching: find.byIcon(Icons.add_rounded),
        ),
      );
      await tester.pumpAndSettle();
      expect(backend.writes.last, ('FENCE_ALT_MAX', 121));

      await tester.tap(find.text('电池检查'));
      await tester.pumpAndSettle();
      expect(find.text('低电保护动作'), findsOneWidget);
      expect(parameterController.stagedChanges, isEmpty);
    },
  );
}

final class _ParameterTreeBackend implements ParameterSessionBackend {
  static const _parameters = [
    FlightControllerParameter(
      name: 'ATC_RAT_PIT_I',
      value: 0.1,
      mavlinkType: 9,
      index: 0,
      count: 5,
    ),
    FlightControllerParameter(
      name: 'ATC_RAT_RLL_I',
      value: 0.1,
      mavlinkType: 9,
      index: 1,
      count: 5,
    ),
    FlightControllerParameter(
      name: 'ATC_RAT_RLL_P',
      value: 0.135,
      mavlinkType: 9,
      index: 2,
      count: 5,
    ),
    FlightControllerParameter(
      name: 'BATT_MONITOR',
      value: 4,
      mavlinkType: 9,
      index: 3,
      count: 5,
    ),
    FlightControllerParameter(
      name: 'GPS_TYPE',
      value: 1,
      mavlinkType: 6,
      index: 4,
      count: 5,
    ),
  ];

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      ParameterDescriptor(
        name: name,
        displayName: name,
        description: '',
        group: '',
        groupDisplayName: '',
        type: '',
        found: false,
      );

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      const [];

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async => const ConnectionOperationResult(success: true, message: 'ok');
}

final class _ParameterEditorBackend implements ParameterSessionBackend {
  static const _parameters = [
    FlightControllerParameter(
      name: 'SERVO1_FUNCTION',
      value: 33,
      mavlinkType: 6,
      index: 0,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'SERIAL1_PROTOCOL',
      value: 2,
      mavlinkType: 6,
      index: 1,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'BATT_ENABLE',
      value: 1,
      mavlinkType: 6,
      index: 2,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'SERIAL1_OPTIONS',
      value: 1,
      mavlinkType: 6,
      index: 3,
      count: 4,
    ),
  ];

  static const _descriptors = [
    ParameterDescriptor(
      name: 'SERVO1_FUNCTION',
      displayName: '舵机输出功能',
      description: '选择舵机输出功能。',
      group: 'SERVO',
      groupDisplayName: '舵机输出',
      type: 'enum',
      found: true,
      options: [
        ParameterOption(value: 0, label: '禁用'),
        ParameterOption(value: 33, label: '电机 1'),
        ParameterOption(value: 70, label: '油门'),
      ],
    ),
    ParameterDescriptor(
      name: 'SERIAL1_PROTOCOL',
      displayName: '串口协议',
      description: '选择串口使用的协议。',
      group: 'SERIAL',
      groupDisplayName: '串口',
      type: 'enum',
      found: true,
      options: [
        ParameterOption(value: -1, label: '无'),
        ParameterOption(value: 1, label: 'MAVLink1'),
        ParameterOption(value: 2, label: 'MAVLink2'),
        ParameterOption(value: 5, label: 'GPS'),
      ],
    ),
    ParameterDescriptor(
      name: 'BATT_ENABLE',
      displayName: '电池监测启用',
      description: '控制电池监测是否启用。',
      group: 'BATT',
      groupDisplayName: '电池',
      type: 'enum',
      found: true,
      options: [
        ParameterOption(value: 0, label: '禁用'),
        ParameterOption(value: 1, label: '启用'),
      ],
    ),
    ParameterDescriptor(
      name: 'SERIAL1_OPTIONS',
      displayName: '串口选项',
      description: '选择串口线路功能。',
      group: 'SERIAL',
      groupDisplayName: '串口',
      type: 'bitmask',
      found: true,
      options: [
        ParameterOption(value: 1, label: '反转接收'),
        ParameterOption(value: 2, label: '反转发送'),
      ],
    ),
  ];

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      _descriptors.singleWhere((descriptor) => descriptor.name == name);

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      _descriptors;

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async => const ConnectionOperationResult(success: true, message: 'ok');
}

final class _ServoEditorBackend
    implements ParameterSessionBackend, ServoOutputSessionBackend {
  int writeCount = 0;

  late final List<FlightControllerParameter> _parameters = [
    for (var channel = 1; channel <= 16; channel++)
      for (final entry in <(String, double)>[
        ('FUNCTION', channel == 1 ? 33 : 0),
        ('REVERSED', 0),
        ('MIN', 1000),
        ('TRIM', 1500),
        ('MAX', 2000),
      ])
        FlightControllerParameter(
          name: 'SERVO${channel}_${entry.$1}',
          value: entry.$2,
          mavlinkType: 6,
          index: (channel - 1) * 5,
          count: 80,
        ),
  ];

  ParameterDescriptor _descriptor(String name) {
    if (name.endsWith('_FUNCTION')) {
      return ParameterDescriptor(
        name: name,
        displayName: '舵机输出功能',
        description: '选择舵机输出功能。',
        group: 'SERVO',
        groupDisplayName: '舵机输出',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: 0, label: '禁用'),
          ParameterOption(value: 33, label: '电机 1'),
          ParameterOption(value: 70, label: '油门'),
        ],
      );
    }
    if (name.endsWith('_REVERSED')) {
      return ParameterDescriptor(
        name: name,
        displayName: '舵机反向',
        description: '反转舵机输出。',
        group: 'SERVO',
        groupDisplayName: '舵机输出',
        type: 'bool',
        found: true,
      );
    }
    return ParameterDescriptor(
      name: name,
      displayName: name,
      description: 'PWM 范围。',
      group: 'SERVO',
      groupDisplayName: '舵机输出',
      type: 'number',
      found: true,
      unit: 'PWM',
      minValue: 800,
      maxValue: 2200,
      increment: 1,
    );
  }

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      _descriptor(name);

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      _parameters.map((parameter) => _descriptor(parameter.name)).toList();

  @override
  Future<ServoOutputSnapshot> loadServoOutputs() async => ServoOutputSnapshot(
    values: List<int>.generate(16, (index) => 1501 + index),
    valid: true,
  );

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    writeCount++;
    return const ConnectionOperationResult(success: true, message: 'ok');
  }
}

final class _SerialEditorBackend
    implements ParameterSessionBackend, SerialHardwareSessionBackend {
  final List<(String, double)> writes = [];

  static const _parameters = [
    FlightControllerParameter(
      name: 'SERIAL0_BAUD',
      value: 115,
      mavlinkType: 6,
      index: 0,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'SERIAL0_PROTOCOL',
      value: 2,
      mavlinkType: 6,
      index: 1,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'SERIAL2_BAUD',
      value: 57,
      mavlinkType: 6,
      index: 2,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'SERIAL2_PROTOCOL',
      value: 5,
      mavlinkType: 6,
      index: 3,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'SERIAL2_OPTIONS',
      value: 1,
      mavlinkType: 6,
      index: 4,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'BRD_SER2_RTSCTS',
      value: 2,
      mavlinkType: 6,
      index: 5,
      count: 7,
    ),
    FlightControllerParameter(
      name: 'SERIAL4_PROTOCOL',
      value: 1,
      mavlinkType: 6,
      index: 6,
      count: 7,
    ),
  ];

  ParameterDescriptor _descriptor(String name) {
    if (name.endsWith('_BAUD')) {
      return ParameterDescriptor(
        name: name,
        displayName: '波特率',
        description: '',
        group: 'SERIAL',
        groupDisplayName: '串口',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: 57, label: '57600'),
          ParameterOption(value: 115, label: '115200'),
        ],
      );
    }
    if (name.endsWith('_PROTOCOL')) {
      return ParameterDescriptor(
        name: name,
        displayName: '协议',
        description: '',
        group: 'SERIAL',
        groupDisplayName: '串口',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: -1, label: '禁用'),
          ParameterOption(value: 1, label: 'MAVLink 1'),
          ParameterOption(value: 2, label: 'MAVLink 2'),
          ParameterOption(value: 5, label: 'GPS'),
        ],
      );
    }
    if (name.endsWith('_OPTIONS')) {
      return ParameterDescriptor(
        name: name,
        displayName: '串口选项',
        description: '',
        group: 'SERIAL',
        groupDisplayName: '串口',
        type: 'bitmask',
        found: true,
        options: const [
          ParameterOption(value: 1, label: '反转接收'),
          ParameterOption(value: 2, label: '反转发送'),
        ],
      );
    }
    return ParameterDescriptor(
      name: name,
      displayName: name,
      description: '',
      group: 'BRD',
      groupDisplayName: '飞控硬件',
      type: 'enum',
      found: true,
    );
  }

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<List<SerialPortHardwareDescriptor>> loadSerialPortHardware() async =>
      const [
        SerialPortHardwareDescriptor(
          serialIndex: 0,
          name: 'OTG1',
          flowControl: 0,
        ),
        SerialPortHardwareDescriptor(
          serialIndex: 2,
          name: 'USART3',
          flowControl: 2,
        ),
      ];

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      _descriptor(name);

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      _parameters.map((parameter) => _descriptor(parameter.name)).toList();

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    writes.add((parameter.name, value));
    return const ConnectionOperationResult(success: true, message: 'ok');
  }
}

final class _RemoteControlEditorBackend
    implements ParameterSessionBackend, RemoteControlInputSessionBackend {
  _RemoteControlEditorBackend() : _parameters = _buildParameters();

  final List<FlightControllerParameter> _parameters;
  final List<(String, double)> writes = [];
  List<int> inputs = <int>[
    1500,
    1500,
    1000,
    1500,
    1500,
    1900,
    1500,
    1500,
    1500,
    1700,
    ...List<int>.filled(8, 1500),
  ];

  static List<FlightControllerParameter> _buildParameters() {
    final values = <String, double>{
      'RCMAP_ROLL': 1,
      'RCMAP_PITCH': 2,
      'RCMAP_THROTTLE': 3,
      'RCMAP_YAW': 4,
      for (var channel = 1; channel <= 4; channel++) 'RC${channel}_REVERSED': 0,
      'ELEVON_MIXING': 0,
      'ELEVON_REVERSE': 0,
      'ELEVON_CH1_REV': 0,
      'ELEVON_CH2_REV': 0,
      'SERVO1_FUNCTION': 77,
      'SERVO1_REVERSED': 0,
      'SERVO2_FUNCTION': 78,
      'SERVO2_REVERSED': 0,
      'FLTMODE_CH': 10,
      for (var index = 1; index <= 6; index++) 'FLTMODE$index': 0,
      for (var channel = 1; channel <= 4; channel++) ...{
        'RC${channel}_MIN': 1000,
        'RC${channel}_TRIM': channel == 3 ? 1000 : 1500,
        'RC${channel}_MAX': 2000,
      },
      for (var channel = 5; channel <= 16; channel++) ...{
        'RC${channel}_OPTION': channel == 6 ? 0 : 4,
        'RC${channel}_TRIM': 1500,
      },
    };
    var index = 0;
    return values.entries
        .map(
          (entry) => FlightControllerParameter(
            name: entry.key,
            value: entry.value,
            mavlinkType: 6,
            index: index++,
            count: values.length,
          ),
        )
        .toList(growable: false);
  }

  ParameterDescriptor _descriptor(String name) {
    if (name.startsWith('FLTMODE') && name != 'FLTMODE_CH') {
      return ParameterDescriptor(
        name: name,
        displayName: '飞行模式',
        description: '',
        group: 'FLTMODE',
        groupDisplayName: '飞行模式',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: 0, label: '垂起自稳'),
          ParameterOption(value: 5, label: '飞控辅助 A'),
        ],
      );
    }
    if (name.endsWith('_OPTION')) {
      return ParameterDescriptor(
        name: name,
        displayName: '通道功能',
        description: '',
        group: 'RC',
        groupDisplayName: '遥控器',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: 0, label: '无动作'),
          ParameterOption(value: 4, label: '返航'),
        ],
      );
    }
    if (name.endsWith('_FUNCTION')) {
      return ParameterDescriptor(
        name: name,
        displayName: '输出功能',
        description: '',
        group: 'SERVO',
        groupDisplayName: '舵机输出',
        type: 'enum',
        found: true,
        options: const [
          ParameterOption(value: 0, label: '禁用'),
          ParameterOption(value: 77, label: 'ElevonLeft'),
          ParameterOption(value: 78, label: 'ElevonRight'),
        ],
      );
    }
    return ParameterDescriptor(
      name: name,
      displayName: name,
      description: '',
      group: 'RC',
      groupDisplayName: '遥控器',
      type: 'number',
      found: true,
      minValue: 800,
      maxValue: 2200,
      increment: 1,
    );
  }

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      _descriptor(name);

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      _parameters.map((parameter) => _descriptor(parameter.name)).toList();

  @override
  Future<RemoteControlInputSnapshot> loadRemoteControlInputs() async =>
      RemoteControlInputSnapshot(
        values: List<int>.unmodifiable(inputs),
        channelCount: 16,
        valid: true,
      );

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    writes.add((parameter.name, value));
    return const ConnectionOperationResult(success: true, message: 'ok');
  }
}

final class _CalibrationEditorBackend
    implements
        ParameterSessionBackend,
        CalibrationSessionBackend,
        CalibrationHistorySessionBackend {
  final List<(SensorCalibrationType, AccelerometerCalibrationMode)> starts = [];
  final List<int> confirmedSteps = [];
  CalibrationStatusSnapshot status = const CalibrationStatusSnapshot();

  static const _parameters = [
    FlightControllerParameter(
      name: 'ARSPD_TYPE',
      value: 1,
      mavlinkType: 6,
      index: 0,
      count: 2,
    ),
    FlightControllerParameter(
      name: 'ARSPD_OFFSET',
      value: 12.5,
      mavlinkType: 9,
      index: 1,
      count: 2,
    ),
  ];

  @override
  Future<ConnectionOperationResult> startCalibration(
    SensorCalibrationType type, {
    AccelerometerCalibrationMode accelerometerMode =
        AccelerometerCalibrationMode.sixFace,
  }) async {
    starts.add((type, accelerometerMode));
    status = switch (type) {
      SensorCalibrationType.accelerometer => CalibrationStatusSnapshot(
        active: true,
        type: 'accelerometer',
        stage: 'waiting_orientation',
        currentStep: accelerometerMode == AccelerometerCalibrationMode.sixFace
            ? 1
            : 0,
        totalSteps: accelerometerMode == AccelerometerCalibrationMode.sixFace
            ? 6
            : 1,
        accelerometerMode:
            accelerometerMode == AccelerometerCalibrationMode.sixFace
            ? 'six_face'
            : 'level',
      ),
      SensorCalibrationType.magnetometer => const CalibrationStatusSnapshot(
        active: true,
        type: 'magnetometer',
        stage: 'calibrating',
        progress: 42,
      ),
      SensorCalibrationType.airspeed => const CalibrationStatusSnapshot(),
    };
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionOperationResult> confirmAccelerometerOrientation(
    int step,
  ) async {
    confirmedSteps.add(step);
    status = CalibrationStatusSnapshot(
      active: true,
      type: 'accelerometer',
      stage: 'waiting_orientation',
      progress: 16,
      currentStep: step + 1,
      totalSteps: 6,
      accelerometerMode: 'six_face',
    );
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionOperationResult> cancelCalibration() async {
    status = const CalibrationStatusSnapshot(stage: 'cancelled');
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<CalibrationStatusSnapshot> loadCalibrationStatus() async => status;

  @override
  Future<List<CalibrationResultSnapshot>> loadCalibrationHistory(
    SensorCalibrationType type,
  ) async => switch (type) {
    SensorCalibrationType.accelerometer => const [
      CalibrationResultSnapshot(
        type: 'accelerometer',
        stage: 'success',
        quality: 'excellent',
        timestampMilliseconds: 1730000000000,
        offsets: SensorVector3(x: 0.02, y: -0.03, z: 0.01),
        scale: SensorVector3(x: 1.0, y: 1.0, z: 0.99),
      ),
    ],
    SensorCalibrationType.magnetometer => const [
      CalibrationResultSnapshot(
        type: 'magnetometer',
        stage: 'success',
        quality: 'good',
        fitness: 18.5,
      ),
    ],
    SensorCalibrationType.airspeed => const [],
  };

  @override
  Future<CalibrationRetrySnapshot> loadCalibrationRetryInfo() async =>
      const CalibrationRetrySnapshot();

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      ParameterDescriptor(
        name: name,
        displayName: name,
        description: '',
        group: 'ARSPD',
        groupDisplayName: '空速计',
        type: 'number',
        found: true,
      );

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      Future.wait(
        _parameters.map((parameter) => loadParameterDescriptor(parameter.name)),
      );

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async => const ConnectionOperationResult(success: true, message: 'ok');
}

final class _SafetySettingsBackend implements ParameterSessionBackend {
  final List<(String, double)> writes = [];

  static const _values = <String, double>{
    'FENCE_ENABLE': 0,
    'FENCE_TYPE': 3,
    'FENCE_ACTION': 1,
    'FENCE_ALT_MAX': 120,
    'FENCE_ALT_MIN': 15,
    'FENCE_RADIUS': 500,
    'FENCE_MARGIN': 20,
    'ARMING_CHECK': 1,
    'BATT_MONITOR': 4,
    'BATT_ARM_VOLT': 14.0,
    'BATT_ARM_MAH': 800,
    'BATT_LOW_VOLT': 13.2,
    'BATT_CRT_VOLT': 12.6,
    'BATT_FS_LOW_ACT': 1,
    'BATT_FS_CRT_ACT': 2,
  };

  late List<FlightControllerParameter> _parameters = _values.entries
      .map(
        (entry) => FlightControllerParameter(
          name: entry.key,
          value: entry.value,
          mavlinkType: 9,
          index: _values.keys.toList().indexOf(entry.key),
          count: _values.length,
        ),
      )
      .toList(growable: false);

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async =>
      _descriptor(name);

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async =>
      _parameters.map((parameter) => _descriptor(parameter.name)).toList();

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    writes.add((parameter.name, value));
    _parameters = _parameters
        .map(
          (item) =>
              item.name == parameter.name ? item.copyWith(value: value) : item,
        )
        .toList(growable: false);
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  ParameterDescriptor _descriptor(String name) {
    final options = switch (name) {
      'FENCE_TYPE' => const [
        ParameterOption(value: 1, label: '最大高度'),
        ParameterOption(value: 2, label: '圆形边界'),
        ParameterOption(value: 4, label: '多边形'),
        ParameterOption(value: 8, label: '最小高度'),
      ],
      'FENCE_ACTION' => const [
        ParameterOption(value: 0, label: '仅报告'),
        ParameterOption(value: 1, label: '返航或降落'),
        ParameterOption(value: 2, label: '总是降落'),
      ],
      'BATT_MONITOR' => const [
        ParameterOption(value: 0, label: '禁用'),
        ParameterOption(value: 4, label: '模拟电压和电流'),
      ],
      'BATT_FS_LOW_ACT' || 'BATT_FS_CRT_ACT' => const [
        ParameterOption(value: 0, label: '仅警告'),
        ParameterOption(value: 1, label: '降落'),
        ParameterOption(value: 2, label: '返航'),
      ],
      _ => const <ParameterOption>[],
    };
    return ParameterDescriptor(
      name: name,
      displayName: name,
      description: '',
      group: name.startsWith('FENCE') ? 'FENCE' : 'BATT',
      groupDisplayName: '安全设置',
      type: options.isEmpty ? 'number' : 'enum',
      found: true,
      options: options,
      minValue: 0,
      maxValue: 10000,
      increment: 1,
    );
  }
}
