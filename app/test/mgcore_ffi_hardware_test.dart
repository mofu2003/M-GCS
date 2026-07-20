import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/mgcore/mgcore_connection_client.dart';
import 'package:mgcs/application/mgcore/mgcore_ffi_api.dart';

void main() {
  test('connects to an explicitly requested serial port', () async {
    final port = Platform.environment['MGCORE_TEST_SERIAL'];
    if (port == null || port.isEmpty) {
      return;
    }

    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);
    addTearDown(() => client.disconnect());

    final result = client.connectSerial(port, 115200);
    expect(result, containsPair('success', true));

    await Future<void>.delayed(const Duration(seconds: 2));
    final diagnostics = client.connectionDiagnostics();
    expect(diagnostics['connected'], true);
  });

  test('reconnects to an explicitly requested serial port three times', () async {
    final port = Platform.environment['MGCORE_TEST_SERIAL'];
    if (port == null || port.isEmpty) {
      return;
    }

    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);
    addTearDown(() => client.disconnect());

    for (var attempt = 1; attempt <= 3; attempt += 1) {
      final connectResult = client.connectSerial(port, 115200);
      expect(connectResult, containsPair('success', true), reason: 'cycle $attempt');

      await Future<void>.delayed(const Duration(seconds: 2));
      expect(
        client.connectionDiagnostics()['connected'],
        true,
        reason: 'cycle $attempt should receive a heartbeat',
      );

      final disconnectResult = client.disconnect();
      expect(disconnectResult, containsPair('success', true), reason: 'cycle $attempt');
    }
  });
}
