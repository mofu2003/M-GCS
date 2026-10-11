import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/payload/payload_controller.dart';

import 'support/payload_backend_fake.dart';

void main() {
  late FakePayloadBackend backend;
  late PayloadController controller;

  setUp(() {
    backend = FakePayloadBackend();
    controller = PayloadController(backend: backend);
  });

  tearDown(() => controller.dispose());

  test('读出相机/云台清单、姿态与视频流，选中项默认第一台', () async {
    backend.onCameraStatus = (_) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100), cameraDevice(componentId: 101)],
      selected: cameraSelected(componentId: 100, photoCount: 7),
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

    await controller.refresh();

    expect(controller.listError, isNull);
    expect(controller.cameras.map((c) => c.componentId), [100, 101]);
    expect(controller.cameraStatus?.componentId, 100);
    expect(controller.cameraStatus?.photoCount, 7);
    expect(controller.selectedCameraId, isNull);
    expect(controller.gimbals.map((g) => g.componentId), [154]);
    expect(controller.gimbalAttitude?.pitchDeg, -20);
    expect(controller.gimbalAttitude?.yawDeg, 30);
    expect(controller.videoStreams, hasLength(1));
    expect(controller.videoStreams.first.uri, 'rtsp://192.168.1.10:8554/main');
  });

  test('无设备是合法状态（available:false），不当作错误', () async {
    await controller.refresh();

    expect(controller.listError, isNull);
    expect(controller.cameras, isEmpty);
    expect(controller.gimbals, isEmpty);
    expect(controller.cameraStatus?.available, isFalse);
  });

  test('按组件 id 切换相机时用该 id 重新读取', () async {
    backend.onCameraStatus = (id) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100), cameraDevice(componentId: 101)],
      selected: cameraSelected(componentId: id == 101 ? 101 : 100),
    );

    await controller.selectCamera(101);

    expect(controller.selectedCameraId, 101);
    expect(backend.lastCameraId, 101);
    expect(controller.cameraStatus?.componentId, 101);
  });

  test('选中的相机消失后退回第一台，不停在已不存在的设备', () async {
    backend.onCameraStatus = (id) => id == 101
        ? cameraStatusResponse(
            devices: [
              cameraDevice(componentId: 100),
              cameraDevice(componentId: 101),
            ],
            selected: cameraSelected(componentId: 101),
          )
        : cameraStatusResponse(
            devices: [cameraDevice(componentId: 100)],
            selected: cameraSelected(componentId: 100),
          );

    await controller.selectCamera(101);
    expect(controller.selectedCameraId, 101);

    // 相机 101 掉线：native 对未知组件 id 返回 available:false。
    backend.onCameraStatus = (id) => id == 101
        ? cameraStatusResponse(devices: [cameraDevice(componentId: 100)])
        : cameraStatusResponse(
            devices: [cameraDevice(componentId: 100)],
            selected: cameraSelected(componentId: 100),
          );

    await controller.refresh();

    expect(controller.selectedCameraId, isNull);
    expect(controller.cameraStatus?.componentId, 100);
  });

  test('命令成功用「已发送」措辞，不谎报已完成', () async {
    backend.onCameraStatus = (_) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100)],
      selected: cameraSelected(componentId: 100),
    );
    await controller.refresh();

    await controller.takePhoto();

    expect(controller.actionError, isNull);
    expect(controller.actionMessage, contains('已发送'));
    expect(backend.calls, contains('cameraTakePhoto'));
  });

  test('命令失败照实显示 native 错误对象', () async {
    backend.onCameraStatus = (_) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100)],
      selected: cameraSelected(componentId: 100),
    );
    await controller.refresh();

    backend.commandResponse = payloadError(
      'SEND_FAILED',
      code: -7,
      message: 'camera take photo failed',
    );
    await controller.takePhoto();

    expect(controller.actionError, 'camera take photo failed');
    expect(controller.actionMessage, isNull);
  });

  test('未连接时不发命令、刷新给出确定错误', () async {
    backend.connected = false;

    await controller.takePhoto();
    expect(controller.actionError, contains('未连接飞控'));
    expect(backend.calls, isEmpty);

    await controller.refresh();
    expect(controller.listError, contains('未连接飞控'));
    expect(controller.cameraStatus, isNull);
  });

  test('会话切换清空设备缓存，重新刷新后再写回', () async {
    backend.onCameraStatus = (_) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100)],
      selected: cameraSelected(componentId: 100),
    );
    await controller.refresh();
    expect(controller.cameras, hasLength(1));

    controller.bindSession(7);
    expect(controller.cameras, isEmpty);
    expect(controller.cameraStatus, isNull);
    expect(controller.actionMessage, isNull);

    await controller.refresh();
    expect(controller.cameras, hasLength(1));
  });

  test('视频源空地址直接拒绝，非空照实转发 native 的未支持结果', () async {
    await controller.startVideo('   ');
    expect(controller.actionError, '请先填写视频源地址');
    expect(backend.lastVideoUri, isNull);

    backend.commandResponse = payloadError(
      'NOT_SUPPORTED',
      code: -4,
      message:
          'local video decoding backend not implemented yet (planned slice S3)',
    );
    await controller.startVideo('rtsp://192.168.1.10:8554/main');

    expect(backend.lastVideoUri, 'rtsp://192.168.1.10:8554/main');
    expect(controller.actionError, contains('not implemented'));
  });

  test('读出本机解码后端状态，未连接时也如实报告能力', () async {
    backend.onRendererStatus = () => rendererStatusResponse(
      available: true,
      backend: 'ffmpeg',
      running: false,
    );

    await controller.refresh();
    expect(controller.videoRendererStatus?.available, isTrue);
    expect(controller.videoRendererStatus?.backend, 'ffmpeg');
    expect(controller.videoRendererStatus?.running, isFalse);

    // 断链只影响飞控读接口，本机解码能力仍应可读。
    backend.connected = false;
    await controller.refresh();
    expect(controller.listError, contains('未连接飞控'));
    expect(controller.videoRendererStatus?.available, isTrue);
  });

  test('无解码后端是合法状态，不写进错误区', () async {
    await controller.refresh();

    expect(controller.listError, isNull);
    expect(controller.videoRendererStatus?.available, isFalse);
    expect(controller.videoRendererStatus?.backend, 'none');
  });

  test('pollFrame 取用后端帧，无新帧返回 null', () async {
    backend.pollFrames.add(renderedFrame(width: 3, height: 2, sequence: 5));

    final frame = controller.pollFrame();
    expect(frame, isNotNull);
    expect(frame!.width, 3);
    expect(frame.height, 2);
    expect(frame.sequence, 5);
    expect(frame.rgba.length, 3 * 2 * 4);

    expect(controller.pollFrame(), isNull);
  });

  test('启动视频源成功后刷新解码运行状态', () async {
    backend.onRendererStatus = () => rendererStatusResponse(
      available: true,
      backend: 'ffmpeg',
      running: true,
      uri: 'rtsp://192.168.1.10:8554/main',
    );

    await controller.startVideo('rtsp://192.168.1.10:8554/main');

    expect(controller.actionError, isNull);
    expect(controller.videoRendererStatus?.running, isTrue);
    expect(
      controller.videoRendererStatus?.uri,
      'rtsp://192.168.1.10:8554/main',
    );
  });

  test('变焦与云台指向把参数原样交给 native', () async {
    backend.onCameraStatus = (_) => cameraStatusResponse(
      devices: [cameraDevice(componentId: 100)],
      selected: cameraSelected(componentId: 100),
    );
    await controller.refresh();

    // 未显式选择相机时传 0；native 约定 camera_id <= 0 表示「第一台 / 活动目标」。
    await controller.setZoom(2.5);
    expect(backend.lastZoom, 2.5);
    expect(backend.lastCameraId, 0);

    await controller.gimbalControl(pitchDeg: -15, yawDeg: 60);
    expect(backend.lastGimbalControl?.pitch, -15);
    expect(backend.lastGimbalControl?.yaw, 60);
  });
}
