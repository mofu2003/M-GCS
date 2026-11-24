import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import 'squad_models.dart';

/// 小队本地存储。UI 与控制器只依赖这个接口，测试用内存实现。
abstract interface class SquadStore {
  Future<List<Squad>> load();
  Future<void> save(List<Squad> squads);
}

/// `Application Support/squads.json`，先写临时文件再 rename，避免半截文件。
final class FileSquadStore implements SquadStore {
  FileSquadStore({Future<Directory> Function()? directoryProvider})
    : _directoryProvider = directoryProvider ?? getApplicationSupportDirectory;

  /// 文件格式版本。读到不认识的版本时返回空列表，绝不覆盖原文件。
  static const schemaVersion = 1;

  final Future<Directory> Function() _directoryProvider;

  Future<File> _file() async {
    final directory = await _directoryProvider();
    return File('${directory.path}/squads.json');
  }

  @override
  Future<List<Squad>> load() async {
    try {
      final file = await _file();
      if (!await file.exists()) return const [];
      final decoded = jsonDecode(await file.readAsString(encoding: utf8));
      if (decoded is! Map || decoded['schema'] != schemaVersion) {
        return const [];
      }
      final rawSquads = decoded['squads'];
      if (rawSquads is! List) return const [];
      final squads = <Squad>[];
      final seen = <String>{};
      for (final item in rawSquads) {
        final squad = Squad.fromJson(item);
        if (squad == null) continue;
        if (!seen.add(squad.id)) continue;
        squads.add(squad);
      }
      return squads;
    } on Object {
      // 损坏或不可读：回退空列表，但不写回，保留用户原文件。
      return const [];
    }
  }

  @override
  Future<void> save(List<Squad> squads) async {
    final file = await _file();
    await file.parent.create(recursive: true);
    final text = const JsonEncoder.withIndent('  ').convert({
      'schema': schemaVersion,
      'squads': [for (final squad in squads) squad.toJson()],
    });
    final tmp = File('${file.path}.tmp');
    await tmp.writeAsString(text, encoding: utf8, flush: true);
    await tmp.rename(file.path);
  }
}
