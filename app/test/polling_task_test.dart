import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/polling_task.dart';

void main() {
  test('does not overlap an active refresh', () async {
    final refreshStarted = Completer<void>();
    final releaseRefresh = Completer<void>();
    var refreshCount = 0;
    final task = PollingTask(
      interval: const Duration(days: 1),
      onTick: () async {
        refreshCount++;
        refreshStarted.complete();
        await releaseRefresh.future;
      },
    );
    addTearDown(task.dispose);

    final firstRefresh = task.runOnce();
    await refreshStarted.future;
    await task.runOnce();

    expect(refreshCount, 1);
    releaseRefresh.complete();
    await firstRefresh;
  });

  test('does not refresh after disposal', () async {
    var refreshCount = 0;
    final task = PollingTask(
      interval: const Duration(days: 1),
      onTick: () async => refreshCount++,
    );

    task.dispose();
    await task.runOnce();

    expect(refreshCount, 0);
  });

  test('pauses and resumes scheduled refreshes', () async {
    var refreshCount = 0;
    final task = PollingTask(
      interval: const Duration(milliseconds: 5),
      onTick: () async => refreshCount++,
    );
    addTearDown(task.dispose);

    task.start();
    await Future<void>.delayed(const Duration(milliseconds: 20));
    task.pause();
    final pausedCount = refreshCount;
    await Future<void>.delayed(const Duration(milliseconds: 20));
    expect(refreshCount, pausedCount);

    task.resume();
    await Future<void>.delayed(const Duration(milliseconds: 20));
    expect(refreshCount, greaterThan(pausedCount));
  });
}
