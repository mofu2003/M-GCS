import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

void main() {
  test('macOS app entitlements allow outbound map tile requests', () {
    for (final path in [
      'macos/Runner/DebugProfile.entitlements',
      'macos/Runner/Release.entitlements',
    ]) {
      final entitlements = File(path).readAsStringSync();

      expect(
        entitlements,
        contains('<key>com.apple.security.network.client</key>'),
        reason: '$path must allow outbound HTTPS tile requests.',
      );
    }
  });
}
