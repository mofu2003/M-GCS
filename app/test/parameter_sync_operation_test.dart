import 'dart:async';

import 'package:flutter_test/flutter_test.dart';

import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/parameter_sync_operation.dart';

void main() {
  test('completes after the backend reports all parameters', () async {
    ConnectionBootstrapSnapshot? applied;
    var changes = 0;
    final operation = ParameterSyncOperation(
      start: () async =>
          const ConnectionOperationResult(success: true, message: 'ok'),
      loadSnapshot: () async => const ConnectionBootstrapSnapshot(
        parameterProgress: ParameterSyncProgress(received: 4, total: 4),
      ),
      onSnapshot: (snapshot) => applied = snapshot,
      onChanged: () => changes++,
    );

    await operation.startIfNeeded();

    expect(operation.state, ConnectionBootstrapState.complete);
    expect(applied?.parameterProgress.isComplete, isTrue);
    expect(changes, 2);
  });

  test('converts a rejected start into a failed state', () async {
    final operation = ParameterSyncOperation(
      start: () async =>
          const ConnectionOperationResult(success: false, message: '拒绝同步'),
      loadSnapshot: () async => const ConnectionBootstrapSnapshot(),
      onSnapshot: (_) {},
      onChanged: () {},
    );

    await operation.startIfNeeded();

    expect(operation.state, ConnectionBootstrapState.failed);
    expect(operation.error, '拒绝同步');
  });

  test('converts backend exceptions into a failed state', () async {
    final operation = ParameterSyncOperation(
      start: () async => throw StateError('transport unavailable'),
      loadSnapshot: () async => const ConnectionBootstrapSnapshot(),
      onSnapshot: (_) {},
      onChanged: () {},
    );

    await operation.startIfNeeded();

    expect(operation.state, ConnectionBootstrapState.failed);
    expect(operation.error, contains('transport unavailable'));
  });

  test('reset returns the operation to idle', () async {
    final operation = ParameterSyncOperation(
      start: () async =>
          const ConnectionOperationResult(success: false, message: 'failed'),
      loadSnapshot: () async => const ConnectionBootstrapSnapshot(),
      onSnapshot: (_) {},
      onChanged: () {},
    );

    await operation.startIfNeeded();
    operation.reset();

    expect(operation.state, ConnectionBootstrapState.idle);
    expect(operation.error, isNull);
  });

  test('ignores a result that belongs to a reset generation', () async {
    final started = Completer<void>();
    final result = Completer<ConnectionOperationResult>();
    final operation = ParameterSyncOperation(
      start: () {
        started.complete();
        return result.future;
      },
      loadSnapshot: () async => const ConnectionBootstrapSnapshot(
        parameterProgress: ParameterSyncProgress(received: 1, total: 1),
      ),
      onSnapshot: (_) {},
      onChanged: () {},
    );

    final request = operation.startIfNeeded();
    await started.future;
    operation.reset();
    result.complete(
      const ConnectionOperationResult(success: true, message: 'ok'),
    );
    await request;

    expect(operation.state, ConnectionBootstrapState.idle);
    expect(operation.error, isNull);
  });
}
