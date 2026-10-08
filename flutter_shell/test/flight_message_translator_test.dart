import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/flight/flight_message_translator.dart';

void main() {
  test('translates common ArduPilot status text templates', () {
    expect(
      translateFlightMessage('PreArm: GPS 3D fix required').displayText,
      '预解锁：GPS 3D fix required',
    );
    expect(
      translateFlightMessage('EKF3 IMU0 is using GPS').displayText,
      'EKF3 IMU0 正在使用 GPS',
    );
    expect(translateFlightMessage('Battery 1 is low').displayText, '电池 1 电量低');
    expect(
      translateFlightMessage('Mode changed to AUTO').displayText,
      '飞行模式已切换为 AUTO',
    );
  });

  test('unknown messages stay verbatim', () {
    const text = 'Custom firmware diagnostic XYZ-42';
    final result = translateFlightMessage(text);
    expect(result.translated, isFalse);
    expect(result.displayText, text);
    expect(result.originalText, text);
  });
}
