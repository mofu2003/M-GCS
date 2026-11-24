import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import 'update_models.dart';

class UpdatePreferences {
  const UpdatePreferences({
    required this.manifestUrl,
    required this.automaticChecks,
    this.lastCheckAt,
    this.lastCheckSucceeded = false,
  });

  final String manifestUrl;
  final bool automaticChecks;
  final DateTime? lastCheckAt;
  final bool lastCheckSucceeded;

  UpdatePreferences copyWith({
    String? manifestUrl,
    bool? automaticChecks,
    DateTime? lastCheckAt,
    bool? lastCheckSucceeded,
  }) => UpdatePreferences(
    manifestUrl: manifestUrl ?? this.manifestUrl,
    automaticChecks: automaticChecks ?? this.automaticChecks,
    lastCheckAt: lastCheckAt ?? this.lastCheckAt,
    lastCheckSucceeded: lastCheckSucceeded ?? this.lastCheckSucceeded,
  );
}

abstract interface class UpdatePreferencesStore {
  Future<UpdatePreferences> load();

  Future<void> save(UpdatePreferences preferences);
}

final class FileUpdatePreferencesStore implements UpdatePreferencesStore {
  FileUpdatePreferencesStore({Future<Directory> Function()? directoryProvider})
    : _directoryProvider = directoryProvider ?? getApplicationSupportDirectory;

  final Future<Directory> Function() _directoryProvider;

  Future<File> _file() async {
    final directory = await _directoryProvider();
    return File('${directory.path}/update_preferences.json');
  }

  @override
  Future<UpdatePreferences> load() async {
    final fallback = UpdatePreferences(
      manifestUrl: kDefaultUpdateManifestUrl,
      automaticChecks: true,
    );
    try {
      final file = await _file();
      if (!await file.exists()) return fallback;
      final decoded = jsonDecode(await file.readAsString());
      if (decoded is! Map) return fallback;
      final rawUrl = decoded['manifestUrl'];
      final rawChecked = decoded['lastCheckAt'];
      return UpdatePreferences(
        manifestUrl: rawUrl is String ? rawUrl.trim() : fallback.manifestUrl,
        automaticChecks: decoded['automaticChecks'] != false,
        lastCheckAt: rawChecked is String
            ? DateTime.tryParse(rawChecked)
            : null,
        lastCheckSucceeded: decoded['lastCheckSucceeded'] == true,
      );
    } on Object {
      return fallback;
    }
  }

  @override
  Future<void> save(UpdatePreferences preferences) async {
    final file = await _file();
    await file.parent.create(recursive: true);
    final temporary = File('${file.path}.tmp');
    await temporary.writeAsString(
      jsonEncode({
        'version': 1,
        'manifestUrl': preferences.manifestUrl,
        'automaticChecks': preferences.automaticChecks,
        'lastCheckAt': preferences.lastCheckAt?.toUtc().toIso8601String(),
        'lastCheckSucceeded': preferences.lastCheckSucceeded,
      }),
      flush: true,
    );
    await temporary.rename(file.path);
  }
}
