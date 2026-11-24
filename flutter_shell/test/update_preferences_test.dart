import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/updates/update_preferences.dart';

void main() {
  test('persists the update source and last check result', () async {
    final directory = await Directory.systemTemp.createTemp(
      'mgcs-update-preferences-',
    );
    addTearDown(() => directory.delete(recursive: true));
    final store = FileUpdatePreferencesStore(
      directoryProvider: () async => directory,
    );
    final saved = UpdatePreferences(
      manifestUrl: 'https://example.com/update.json',
      automaticChecks: false,
      lastCheckAt: DateTime.utc(2026, 11, 16, 8, 30),
      lastCheckSucceeded: true,
    );

    await store.save(saved);
    final loaded = await store.load();

    expect(loaded.manifestUrl, saved.manifestUrl);
    expect(loaded.automaticChecks, isFalse);
    expect(loaded.lastCheckAt, saved.lastCheckAt);
    expect(loaded.lastCheckSucceeded, isTrue);
  });
}
