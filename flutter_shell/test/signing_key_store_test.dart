import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/connection/signing_key_store.dart';

void main() {
  test('signing preference persists, normalizes and clears', () async {
    final directory = await Directory.systemTemp.createTemp(
      'mgcs-signing-test-',
    );
    addTearDown(() async {
      if (await directory.exists()) await directory.delete(recursive: true);
    });
    final store = SigningKeyStore(directoryProvider: () async => directory);
    const key =
        '00112233445566778899AABBCCDDEEFF'
        '102030405060708090A0B0C0D0E0F001';

    expect((await store.loadLibrary()).entries, isEmpty);
    await store.saveLibrary(
      SigningKeyLibrary(
        entries: [
          SigningKeyPreference(name: '机队主密钥', keyHex: key, signOutgoing: false),
        ],
        activeName: '机队主密钥',
      ),
    );
    final loaded = await store.loadLibrary();
    expect(loaded.entries, hasLength(1));
    expect(loaded.activeKey?.name, '机队主密钥');
    expect(loaded.activeKey?.keyHex, key.toLowerCase());
    expect(loaded.activeKey?.signOutgoing, isFalse);

    await store.clear();
    expect((await store.loadLibrary()).entries, isEmpty);
  });

  test(
    'key library preserves multiple named keys and active selection',
    () async {
      final directory = await Directory.systemTemp.createTemp(
        'mgcs-signing-library-',
      );
      addTearDown(() async {
        if (await directory.exists()) await directory.delete(recursive: true);
      });
      final store = SigningKeyStore(directoryProvider: () async => directory);
      const first =
          '00112233445566778899aabbccddeeff'
          '102030405060708090a0b0c0d0e0f001';
      const second =
          'ffeeddccbbaa99887766554433221100'
          'f0e0d0c0b0a090807060504030201000';

      await store.saveLibrary(
        const SigningKeyLibrary(
          entries: [
            SigningKeyPreference(name: '主密钥', keyHex: first),
            SigningKeyPreference(name: '备用密钥', keyHex: second),
          ],
          activeName: '备用密钥',
        ),
      );
      final loaded = await store.loadLibrary();
      expect(loaded.entries, hasLength(2));
      expect(loaded.activeKey?.name, '备用密钥');
      expect(loaded.activeKey?.keyHex, second);

      await expectLater(
        store.saveLibrary(
          const SigningKeyLibrary(
            entries: [
              SigningKeyPreference(name: '重复', keyHex: first),
              SigningKeyPreference(name: '重复', keyHex: second),
            ],
          ),
        ),
        throwsArgumentError,
      );
    },
  );

  test('invalid signing key is rejected', () async {
    final directory = await Directory.systemTemp.createTemp(
      'mgcs-signing-invalid-',
    );
    addTearDown(() async {
      if (await directory.exists()) await directory.delete(recursive: true);
    });
    final store = SigningKeyStore(directoryProvider: () async => directory);
    await expectLater(
      store.saveLibrary(
        const SigningKeyLibrary(
          entries: [SigningKeyPreference(keyHex: 'not-a-key')],
        ),
      ),
      throwsArgumentError,
    );
  });
}
