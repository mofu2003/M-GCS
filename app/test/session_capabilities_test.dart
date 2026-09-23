import 'package:flutter_test/flutter_test.dart';

import 'package:mgcs/application/demo_connection_session_backend.dart';
import 'package:mgcs/application/session_capabilities.dart';

void main() {
  test('discovers implemented capabilities without changing contracts', () {
    final capabilities = SessionCapabilities(DemoConnectionSessionBackend());

    expect(capabilities.connection, isNotNull);
    expect(capabilities.parameters, isNotNull);
    expect(capabilities.parameterFiles, isNotNull);
    expect(capabilities.mission, isNotNull);
    expect(capabilities.supportsMission, isTrue);
    expect(capabilities.calibration, isNull);
    expect(capabilities.sensorTelemetry, isNull);
  });

  test('does not invent capabilities for an unrelated object', () {
    final capabilities = SessionCapabilities(Object());

    expect(capabilities.connection, isNull);
    expect(capabilities.parameters, isNull);
    expect(capabilities.mission, isNull);
    expect(capabilities.supportsMission, isFalse);
  });
}
