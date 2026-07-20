import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_controller.dart';
import 'package:mgcs/application/demo_connection_session_backend.dart';
import 'package:mgcs/features/frontstage/frontstage_controller_adapter.dart';

void main() {
  test(
    'adapter maps disconnected controller to drawer-visible model',
    () async {
      final controller = ConnectionController(
        backend: DemoConnectionSessionBackend(),
      );
      await controller.initialize();

      final model = FrontstageControllerAdapter.buildModel(
        controller: controller,
        connectionDrawerVisible: false,
        missionPanelVisible: false,
        dashboardVisible: true,
      );

      expect(model.connection.connected, isFalse);
      expect(model.connectionDrawerVisible, isFalse);
      expect(model.missionPanelVisible, isFalse);
      expect(model.dashboardVisible, isTrue);
      expect(model.connection.baudRateLabel, '115200');
      expect(model.title, isEmpty);
      expect(model.subtitle, isEmpty);
      expect(
        model.status.items.map((item) => item.id),
        orderedEquals(const ['gps', 'battery', 'mode', 'armed', 'output']),
      );
      expect(
        model.status.items.map((item) => item.label),
        orderedEquals(const ['GPS信息', '电池信息', '飞行模式', '解锁状态', '飞控信息']),
      );
      final output = model.status.items.singleWhere(
        (item) => item.id == 'output',
      );
      expect(output.value, '未连接');
      expect(output.detail, '飞控未连接');

      controller.dispose();
    },
  );

  test('adapter maps connected telemetry to attitude labels', () async {
    final controller = ConnectionController(
      backend: DemoConnectionSessionBackend(),
    );
    await controller.initialize();
    await controller.connect();

    final model = FrontstageControllerAdapter.buildModel(
      controller: controller,
      connectionDrawerVisible: true,
      missionPanelVisible: true,
      dashboardVisible: true,
    );

    expect(model.connection.connected, isTrue);
    expect(model.connectionDrawerVisible, isTrue);
    expect(model.missionPanelVisible, isTrue);
    expect(model.dashboardVisible, isTrue);
    expect(model.attitude.altitudeLabel, '12.4m');
    expect(model.attitude.airspeedLabel, '12.6m/s');
    expect(model.attitude.groundSpeedLabel, '0.8m/s');
    expect(model.attitude.verticalSpeedLabel, '-0.2m/s');
    expect(model.attitude.batteryLabel, '12.1V');
    final gps = model.status.items.singleWhere((item) => item.id == 'gps');
    expect(gps.value, '14颗');
    expect(gps.detail, '31.2304, 121.4737');
    final battery = model.status.items.singleWhere(
      (item) => item.id == 'battery',
    );
    expect(battery.value, '12.1V');
    expect(battery.detail, '87%');

    controller.dispose();
  });
}
