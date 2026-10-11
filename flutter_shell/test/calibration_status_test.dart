import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/connection_controller.dart';

void main() {
  test('calibration status with compass progress is JSON encodable', () {
    const status = CalibrationStatus(
      active: true,
      type: CalibrationType.magnetometer,
      stage: 'calibrating',
      progress: 63,
      compassProgress: {0: 63, 1: 12},
    );

    final encoded = jsonEncode(status.toJson());

    expect(encoded, contains('"compassProgress":{"0":63,"1":12}'));
  });

  test('a cancelling calibration stays active until the native side stops', () {
    const status = CalibrationStatus(
      active: true,
      cancelling: true,
      stage: 'calibrating',
      message: '正在取消校准…',
    );

    // S8：取消请求在途时仍然算“进行中”，界面不得显示为已停止。
    expect(status.active, isTrue);
    expect(status.cancelling, isTrue);
    expect(jsonEncode(status.toJson()), contains('"cancelling":true'));
  });
}
