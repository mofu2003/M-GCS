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
}
