import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/payload/video_storage.dart';

void main() {
  late Directory temp;

  setUp(() => temp = Directory.systemTemp.createTempSync('mgcs-storage-'));
  tearDown(() {
    if (temp.existsSync()) temp.deleteSync(recursive: true);
  });

  File writeRecording(String name, int bytes) =>
      File('${temp.path}/$name')..writeAsBytesSync(List<int>.filled(bytes, 0));

  test('容量上限钳到合法区间', () {
    expect(clampVideoLimit(10), kMinVideoLimitMegabytes);
    expect(clampVideoLimit(1024), 1024);
    expect(clampVideoLimit(1 << 40), 1 << 30);
  });

  test('文件名带时间戳且后缀固定', () {
    final at = DateTime(2026, 10, 11, 9, 5, 3);
    expect(recordingFileName(at), 'mgcs-20261011-090503.mkv');
    expect(snapshotFileName(at), 'mgcs-20261011-090503.png');
  });

  test('只列出录制文件且按时间从旧到新', () {
    writeRecording('b.mkv', 16);
    writeRecording('a.mov', 16);
    writeRecording('notes.txt', 16);
    writeRecording('still.png', 16);

    final names = listRecordings(temp.path).map((f) {
      return f.uri.pathSegments.last;
    }).toList();

    expect(names, containsAll(['a.mov', 'b.mkv']));
    expect(names, isNot(contains('notes.txt')));
    expect(names, isNot(contains('still.png')));
  });

  test('超出上限时删除最旧文件，直到落回上限内', () async {
    writeRecording('a.mkv', 1024 * 1024);
    writeRecording('b.mkv', 1024 * 1024);
    writeRecording('c.mkv', 1024 * 1024);

    final deleted = await pruneOldRecordings(
      directory: temp.path,
      maxMegabytes: 2,
    );

    expect(deleted, 1);
    expect(File('${temp.path}/a.mkv').existsSync(), isFalse);
    expect(File('${temp.path}/b.mkv').existsSync(), isTrue);
    expect(File('${temp.path}/c.mkv').existsSync(), isTrue);
  });

  test('仍在写入的文件即使最旧也不删', () async {
    final keep = writeRecording('a.mkv', 2 * 1024 * 1024);
    writeRecording('b.mkv', 1024 * 1024);

    final deleted = await pruneOldRecordings(
      directory: temp.path,
      maxMegabytes: 2,
      keep: keep.path,
    );

    expect(deleted, 1);
    expect(keep.existsSync(), isTrue);
    expect(File('${temp.path}/b.mkv').existsSync(), isFalse);
  });

  test('未设上限或目录不存在时不做任何事', () async {
    writeRecording('a.mkv', 1024 * 1024);
    expect(await pruneOldRecordings(directory: temp.path, maxMegabytes: 0), 0);
    expect(
      await pruneOldRecordings(
        directory: '${temp.path}/missing',
        maxMegabytes: 1,
      ),
      0,
    );
    expect(File('${temp.path}/a.mkv').existsSync(), isTrue);
  });

  test('ensureDirectory 建出缺失的目录，失败时给出原因', () async {
    final target = '${temp.path}/nested/recordings';
    expect(await ensureDirectory(target), isNull);
    expect(Directory(target).existsSync(), isTrue);

    // 用文件占住路径，让创建目录必然失败。
    final blocked = File('${temp.path}/blocked')..writeAsStringSync('x');
    expect(await ensureDirectory(blocked.path), isNotNull);
  });
}
