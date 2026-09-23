import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/infrastructure/mgcore/mgcore_connection_client.dart';
import 'package:mgcs/infrastructure/mgcore/mgcore_ffi_api.dart';

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

  test('imports and exports Mission Planner parameter files offline', () {
    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);

    final imported = client.importParameterFile(
      'BATT_MONITOR,4\nFORMAT_VERSION,120\nATC_RAT_RLL_P 0.135\n',
    );
    expect(imported['success'], true);
    expect(imported['count'], 2);
    expect(
      (imported['items']! as List<Object?>)
          .whereType<Map<Object?, Object?>>()
          .map((item) => item['name']),
      ['BATT_MONITOR', 'ATC_RAT_RLL_P'],
    );

    final exported = client.exportParameterFile(const [
      {'name': 'BATT_MONITOR', 'value': 4},
      {'name': 'ATC_RAT_RLL_P', 'value': 0.135},
    ]);
    expect(exported, 'ATC_RAT_RLL_P,0.135\nBATT_MONITOR,4\n');
  });

  test('returns enum options through the live descriptor FFI', () {
    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);

    final descriptor = client.parameterDescriptor('SERIAL1_PROTOCOL');
    expect(descriptor['type'], 'enum');
    expect(descriptor['requires_reboot'], true);
    expect(descriptor['options'], isA<List<Object?>>());
    expect((descriptor['options']! as List<Object?>).length, greaterThan(5));
  });

  test('exposes the offline calibration status through Dart FFI', () {
    final client = MgCoreConnectionClient(api: MgCoreFfiApi.open());
    addTearDown(client.dispose);

    final status = client.calibrationStatus();
    expect(status['active'], false);
    expect(status['stage'], 'idle');
  });
}
