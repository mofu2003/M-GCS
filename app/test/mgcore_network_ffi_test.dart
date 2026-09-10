import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/mgcore/mgcore_connection_client.dart';
import 'package:mgcs/application/mgcore/mgcore_ffi_api.dart';

MgCoreFfiApi? _tryOpenApi() {
  try {
    return MgCoreFfiApi.open();
  } on Object {
    return null;
  }
}

Future<int> _reserveUdpPort() async {
  final socket = await RawDatagramSocket.bind(InternetAddress.loopbackIPv4, 0);
  final port = socket.port;
  socket.close();
  return port;
}

void main() {
  test('TCP connection reaches the native FFI transport', () async {
    final api = _tryOpenApi();
    if (api == null) {
      return;
    }

    final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
    addTearDown(() => server.close());
    final accepted = server.first;

    final client = MgCoreConnectionClient(api: api);
    addTearDown(client.dispose);

    final result = client.connectTcp('127.0.0.1:${server.port}');
    expect(result, containsPair('success', true));
    final socket = await accepted.timeout(const Duration(seconds: 3));
    final heartbeat = await socket.first.timeout(const Duration(seconds: 3));
    expect(heartbeat, isNotEmpty);
    socket.destroy();
    expect(client.disconnect(), containsPair('success', true));
  });

  test('UDP connection binds the native FFI listener', () async {
    final api = _tryOpenApi();
    if (api == null) {
      return;
    }

    final client = MgCoreConnectionClient(api: api);
    addTearDown(client.dispose);
    final port = await _reserveUdpPort();
    final sender = await RawDatagramSocket.bind(
      InternetAddress.loopbackIPv4,
      0,
    );
    addTearDown(() => sender.close());

    final result = client.connectUdp('127.0.0.1:$port');
    expect(result, containsPair('success', true));
    sender.send([0xfe, 0x00], InternetAddress.loopbackIPv4, port);
    await Future<void>.delayed(const Duration(milliseconds: 50));
    expect(client.disconnect(), containsPair('success', true));
  });
}
