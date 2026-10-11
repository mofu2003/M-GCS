import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:mgcs_frontend/payload/camera_gimbal_page.dart';
import 'package:mgcs_frontend/payload/payload_backend.dart';
import 'package:mgcs_frontend/payload/payload_controller.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import 'support/payload_backend_fake.dart';

Future<void> _pumpPage(
  WidgetTester tester,
  PayloadController controller,
) async {
  tester.view.physicalSize = const Size(1400, 1600);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
  await tester.pumpWidget(
    MacosTheme(
      data: MacosThemeData.light().copyWith(
        typography: MgTypography.macos(Brightness.light),
      ),
      child: MaterialApp(
        home: Scaffold(body: CameraGimbalPage(controller: controller)),
      ),
    ),
  );
  await tester.pumpAndSettle();
}

void main() {
  testWidgets('原生后端不可用时给出确定错误，不静默成功', (tester) async {
    final controller = PayloadController(
      backend: const PayloadUnavailableBackend(),
    );
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);

    expect(find.textContaining('未连接飞控'), findsWidgets);
    expect(find.text('未发现相机。相机上报 CAMERA_INFORMATION 后会出现在这里。'), findsNothing);
    expect(find.text('相机'), findsWidgets);
    expect(find.text('云台'), findsWidgets);
    expect(find.text('视频源'), findsWidgets);
    expect(tester.takeException(), isNull);
  });

  testWidgets('有设备时列出清单、状态与视频流，拍照走控制器', (tester) async {
    final backend = FakePayloadBackend();
    backend.onCameraStatus = (id) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100), cameraDevice(componentId: 101)],
      selected: cameraSelected(componentId: 100, photoCount: 3),
    );
    backend.onGimbalStatus = (_) => gimbalStatusResponse(
      devices: [gimbalDevice(componentId: 154)],
      selected: gimbalSelected(componentId: 154),
    );
    backend.onGimbalAttitude = (_) =>
        gimbalAttitudeResponse(pitchDeg: -20, yawDeg: 30);
    backend.onVideoStatus = () => videoStatusResponse(streams: [videoStream()]);
    backend.onVideoStreams = () =>
        videoStreamsResponse(streams: [videoStream()]);
    final controller = PayloadController(backend: backend);
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);

    // 两张设备选择器都停在「自动（第一台）」，多相机可切换。
    expect(find.text('自动（第一台）'), findsNWidgets(2));
    expect(find.textContaining('MG CAM'), findsWidgets);
    expect(find.textContaining('pitch -20.0°'), findsOneWidget);
    expect(find.text('Front'), findsOneWidget);
    expect(
      find.textContaining('rtsp://192.168.1.10:8554/main'),
      findsOneWidget,
    );

    await tester.tap(find.text('拍照'));
    await tester.pumpAndSettle();
    expect(find.textContaining('已发送拍照指令'), findsOneWidget);
    expect(backend.calls, contains('cameraTakePhoto'));
    expect(tester.takeException(), isNull);
  });

  testWidgets('切换相机后按组件 id 重新读取并更新界面', (tester) async {
    final backend = FakePayloadBackend()
      ..onCameraStatus = (id) => cameraStatusResponse(
        devices: [
          cameraDevice(componentId: 100),
          cameraDevice(componentId: 101),
        ],
        selected: cameraSelected(
          componentId: id == 101 ? 101 : 100,
          modelName: id == 101 ? 'CAM-B' : 'CAM-A',
        ),
      );
    final controller = PayloadController(backend: backend);
    addTearDown(controller.dispose);

    await _pumpPage(tester, controller);
    expect(find.textContaining('CAM-A'), findsWidgets);

    await tester.tap(find.text('自动（第一台）').first);
    await tester.pumpAndSettle();
    await tester.tap(find.textContaining('comp 101').last);
    await tester.pumpAndSettle();

    expect(controller.selectedCameraId, 101);
    expect(backend.lastCameraId, 101);
    expect(find.textContaining('CAM-B'), findsWidgets);
    expect(tester.takeException(), isNull);
  });
}
