import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/mgcore/mgcore_connection_client.dart';
import 'package:mgcs/application/mgcore/mgcore_ffi_api.dart';

void main() {
  test('loads mgcore and enumerates serial ports through Dart FFI', () {
    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);

    final ports = client.listPorts();
    final items = ports['items'];
    expect(items, isA<List<Object?>>());
    for (final item in items! as List<Object?>) {
      expect(item, isA<Map<Object?, Object?>>());
    }
  });
}
