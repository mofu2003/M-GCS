import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';

/// 一把命名的 MAVLink signing 密钥。
@immutable
class SigningKeyPreference {
  const SigningKeyPreference({
    this.name = '默认密钥',
    required this.keyHex,
    this.signOutgoing = true,
    this.lastTimestampTicks = 0,
  });

  final String name;
  final String keyHex;
  final bool signOutgoing;
  final int lastTimestampTicks;

  SigningKeyPreference copyWith({
    String? name,
    String? keyHex,
    bool? signOutgoing,
    int? lastTimestampTicks,
  }) => SigningKeyPreference(
    name: name ?? this.name,
    keyHex: keyHex ?? this.keyHex,
    signOutgoing: signOutgoing ?? this.signOutgoing,
    lastTimestampTicks: lastTimestampTicks ?? this.lastTimestampTicks,
  );

  Map<String, Object?> toJson() => {
    'name': name,
    'keyHex': keyHex,
    'signOutgoing': signOutgoing,
    'lastTimestampTicks': lastTimestampTicks,
  };

  static SigningKeyPreference? fromJson(Object? value) {
    if (value is! Map) return null;
    final keyHex = value['keyHex'];
    if (keyHex is! String || !isValidSigningKeyHex(keyHex)) return null;
    final rawName = value['name'];
    final name = rawName is String && rawName.trim().isNotEmpty
        ? rawName.trim()
        : '默认密钥';
    return SigningKeyPreference(
      name: name,
      keyHex: keyHex.toLowerCase(),
      signOutgoing: value['signOutgoing'] != false,
      lastTimestampTicks: (value['lastTimestampTicks'] as num?)?.toInt() ?? 0,
    );
  }
}

/// 本地密钥库；同名 key 不重复保存。
@immutable
class SigningKeyLibrary {
  const SigningKeyLibrary({this.entries = const [], this.activeName});

  final List<SigningKeyPreference> entries;
  final String? activeName;

  SigningKeyPreference? get activeKey {
    final name = activeName;
    if (name == null) return null;
    for (final entry in entries) {
      if (entry.name == name) return entry;
    }
    return null;
  }
}

bool isValidSigningKeyHex(String value) =>
    RegExp(r'^[0-9a-fA-F]{64}$').hasMatch(value.trim());

/// MAVLink signing 本地密钥库。
final class SigningKeyStore {
  SigningKeyStore({Future<Directory> Function()? directoryProvider})
    : _directoryProvider = directoryProvider ?? getApplicationSupportDirectory;

  final Future<Directory> Function() _directoryProvider;

  Future<File> _file() async {
    final directory = await _directoryProvider();
    return File('${directory.path}/m-gcs-mavlink-signing.json');
  }

  Future<SigningKeyLibrary> loadLibrary() async {
    try {
      final file = await _file();
      if (!await file.exists()) return const SigningKeyLibrary();
      final decoded = jsonDecode(await file.readAsString());
      if (decoded is Map && decoded['keys'] is List) {
        final entries = <SigningKeyPreference>[];
        for (final item in decoded['keys'] as List) {
          final entry = SigningKeyPreference.fromJson(item);
          if (entry != null) entries.add(entry);
        }
        final activeRaw = decoded['activeKeyName'];
        final activeName =
            activeRaw is String &&
                entries.any((entry) => entry.name == activeRaw)
            ? activeRaw
            : entries.isEmpty
            ? null
            : entries.first.name;
        return SigningKeyLibrary(entries: entries, activeName: activeName);
      }
      final legacy = SigningKeyPreference.fromJson(decoded);
      if (legacy == null) return const SigningKeyLibrary();
      return SigningKeyLibrary(entries: [legacy], activeName: legacy.name);
    } on Object {
      return const SigningKeyLibrary();
    }
  }

  Future<void> saveLibrary(SigningKeyLibrary library) async {
    final names = <String>{};
    final normalized = <SigningKeyPreference>[];
    for (final raw in library.entries) {
      final name = raw.name.trim();
      final keyHex = raw.keyHex.trim().toLowerCase();
      if (name.isEmpty) {
        throw ArgumentError('MAVLink signing key name must not be empty');
      }
      if (!isValidSigningKeyHex(keyHex)) {
        throw ArgumentError('MAVLink signing key must be 64 hex characters');
      }
      if (!names.add(name.toLowerCase())) {
        throw ArgumentError('MAVLink signing key name must be unique');
      }
      normalized.add(raw.copyWith(name: name, keyHex: keyHex));
    }
    if (normalized.isEmpty) {
      await clear();
      return;
    }
    final activeName =
        normalized.any((entry) => entry.name == library.activeName)
        ? library.activeName
        : normalized.first.name;
    final file = await _file();
    await file.parent.create(recursive: true);
    await file.writeAsString(
      jsonEncode({
        'version': 2,
        'activeKeyName': activeName,
        'keys': [for (final entry in normalized) entry.toJson()],
      }),
      flush: true,
    );
    if (!Platform.isWindows) {
      try {
        await Process.run('chmod', ['600', file.path]);
      } on Object {
        // The settings file is still usable if chmod is unavailable.
      }
    }
  }

  Future<void> clear() async {
    final file = await _file();
    if (await file.exists()) await file.delete();
  }
}
