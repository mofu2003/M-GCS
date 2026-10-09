import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/vehicle_signing_bindings.dart';

void main() {
  test('vehicle bindings do not leak across focus switches', () {
    final bindings = VehicleSigningBindings();
    bindings.bind(1, 'key-A');
    bindings.markApplied(1, 'A');
    expect(bindings.nameFor(2), isNull);
    bindings.bind(2, 'key-B');
    bindings.markApplied(2, 'B');
    expect(bindings.nameFor(1), 'key-A');
    expect(bindings.isApplied(1, 'A'), isTrue);
    bindings.bind(2, 'key-C');
    expect(bindings.isApplied(2, 'B'), isFalse);
    expect(bindings.isApplied(1, 'A'), isTrue);
    expect(bindings.removeKey('key-A'), [1]);
    expect(bindings.nameFor(1), isNull);
    expect(bindings.nameFor(2), 'key-C');
  });
}
