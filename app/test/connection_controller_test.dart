import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/presentation/controllers/connection_controller.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/connection_session_backend.dart';

void main() {
  test('initializes ports and connects through session backend', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    await controller.initialize();
    expect(controller.ports, hasLength(1));
    expect(controller.request.port, isEmpty);
    controller.setSerialPort('/dev/tty.test');

    await controller.connect();
    expect(controller.status, ConnectionStatus.connected);
    expect(controller.snapshot?.connection.label, 'Test vehicle');
    expect(backend.connected, isTrue);

    await controller.disconnect();
    expect(controller.status, ConnectionStatus.idle);
    expect(controller.snapshot, isNull);
    expect(backend.connected, isFalse);
  });

  test('increments the session epoch for each connection lifecycle', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    expect(controller.sessionEpoch, 0);
    await controller.connect();
    expect(controller.sessionEpoch, 1);
    await controller.disconnect();
    expect(controller.sessionEpoch, 2);
    await controller.connect();
    expect(controller.sessionEpoch, 3);
  });

  test('connects through TCP and UDP endpoint requests', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    controller.setNetworkEndpoint(ConnectionTransport.tcp, '127.0.0.1:5760');
    expect(controller.request.transport, ConnectionTransport.tcp);
    expect(controller.request.endpoint, '127.0.0.1:5760');
    await controller.connect();
    expect(backend.lastRequest?.transport, ConnectionTransport.tcp);
    expect(backend.lastRequest?.endpoint, '127.0.0.1:5760');

    await controller.disconnect();
    controller.setNetworkEndpoint(ConnectionTransport.udp, '0.0.0.0:14550');
    expect(controller.request.transport, ConnectionTransport.udp);
    expect(controller.request.endpoint, '0.0.0.0:14550');
    await controller.connect();
    expect(backend.lastRequest?.transport, ConnectionTransport.udp);
    expect(backend.lastRequest?.endpoint, '0.0.0.0:14550');
  });

  test('converts a thrown connect error into an error state', () async {
    final backend = _FakeBackend()..connectError = StateError('port busy');
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    await controller.connect();

    expect(controller.status, ConnectionStatus.error);
    expect(controller.errorMessage, contains('port busy'));
    expect(controller.snapshot, isNull);
  });

  test('cleans up local state when disconnect throws', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    await controller.connect();
    backend.disconnectError = StateError('transport closed');
    await controller.disconnect();

    expect(controller.status, ConnectionStatus.error);
    expect(controller.errorMessage, contains('transport closed'));
    expect(controller.snapshot, isNull);
    expect(controller.bootstrapState, ConnectionBootstrapState.idle);
  });

  test('keeps the session alive after a transient telemetry error', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    await controller.connect();
    backend.telemetryError = StateError('temporary read failure');
    await controller.refreshTelemetry();

    expect(controller.status, ConnectionStatus.connected);
    expect(controller.errorMessage, contains('temporary read failure'));

    backend.telemetryError = null;
    await controller.refreshTelemetry();
    expect(controller.errorMessage, isNull);
  });

  test('refreshes serial ports without clearing the selected port', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(backend: backend);
    addTearDown(controller.dispose);

    await controller.initialize();
    controller.setSerialPort('/dev/tty.test');
    backend.ports = const [
      ConnectionSerialPort(name: 'Detected board', path: '/dev/tty.new'),
    ];

    await controller.refreshSerialPorts();

    expect(controller.ports.single.name, 'Detected board');
    expect(controller.request.port, '/dev/tty.test');
  });

  test(
    'retains flight controller text messages from the backend stream',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(backend: backend);
      final message = FlightControllerMessage(
        text: 'PreArm: Waiting for RC',
        severity: 'warning',
        receivedAt: DateTime(2026, 7, 18, 12),
      );

      backend.emitMessage(message);
      await Future<void>.delayed(Duration.zero);

      expect(controller.messages, [message]);
      controller.dispose();
    },
  );

  test(
    'refreshes telemetry at the configured cadence after connecting',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(
        backend: backend,
        telemetryPollInterval: const Duration(milliseconds: 10),
      );
      addTearDown(controller.dispose);

      await controller.connect();
      final initialSnapshotCount = backend.sessionSnapshotCount;

      await backend.nextTelemetryRefresh.future.timeout(
        const Duration(seconds: 1),
      );

      expect(backend.sessionSnapshotCount, initialSnapshotCount);
      expect(backend.telemetrySnapshotCount, greaterThan(1));
    },
  );

  test('retains startup information while refreshing telemetry', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(
      backend: backend,
      telemetryPollInterval: const Duration(milliseconds: 10),
    );
    addTearDown(controller.dispose);

    await controller.connect();
    await backend.nextTelemetryRefresh.future.timeout(
      const Duration(seconds: 1),
    );

    expect(controller.snapshot?.bootstrap.parameterProgress.received, 1);
    expect(controller.snapshot?.bootstrap.parameterProgress.total, 2);
  });

  test(
    'starts parameter sync once after the connection is confirmed',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(
        backend: backend,
        telemetryPollInterval: const Duration(milliseconds: 10),
        connectionRefreshInterval: const Duration(milliseconds: 10),
      );
      addTearDown(controller.dispose);

      await controller.connect();
      expect(controller.bootstrapState, ConnectionBootstrapState.downloading);
      expect(backend.parameterSyncRequests, 1);

      await backend.nextSessionRefresh.future.timeout(
        const Duration(seconds: 1),
      );
      expect(backend.parameterSyncRequests, 1);

      await controller.disconnect();
      expect(controller.bootstrapState, ConnectionBootstrapState.idle);
    },
  );

  test(
    'retries a stalled parameter download while telemetry stays active',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(
        backend: backend,
        telemetryPollInterval: const Duration(milliseconds: 5),
        parameterSyncStallTimeout: Duration.zero,
      );
      addTearDown(controller.dispose);

      await controller.connect();
      await Future<void>.delayed(const Duration(milliseconds: 350));

      expect(backend.parameterSyncRequests, greaterThanOrEqualTo(2));
      expect(backend.telemetrySnapshotCount, greaterThan(1));
      expect(controller.bootstrapState, ConnectionBootstrapState.downloading);
    },
  );

  test(
    'fails parameter bootstrap after stalled retries are exhausted',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(
        backend: backend,
        telemetryPollInterval: const Duration(milliseconds: 5),
        parameterSyncStallTimeout: Duration.zero,
        parameterSyncMaxRetries: 0,
      );
      addTearDown(controller.dispose);

      await controller.connect();
      await Future<void>.delayed(const Duration(milliseconds: 350));

      expect(controller.bootstrapState, ConnectionBootstrapState.failed);
      expect(controller.bootstrapError, contains('参数下载连续停滞'));
      expect(backend.telemetrySnapshotCount, greaterThan(1));
    },
  );

  test(
    'serializes disconnect and reconnect behind an active telemetry refresh',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(backend: backend);
      addTearDown(controller.dispose);

      await controller.connect();
      backend.blockNextTelemetryRefresh();
      final telemetryRefresh = controller.refreshTelemetry();
      await backend.telemetryRefreshStarted.future;

      final disconnect = controller.disconnect();
      final reconnect = controller.connect();
      await Future<void>.delayed(Duration.zero);

      expect(backend.disconnectCalls, 0);
      expect(backend.connectCalls, 1);

      backend.completeBlockedTelemetryRefresh();
      await Future.wait([telemetryRefresh, disconnect, reconnect]);

      expect(backend.disconnectCalls, 1);
      expect(backend.connectCalls, 2);
      expect(controller.status, ConnectionStatus.connected);
      expect(backend.connected, isTrue);
    },
  );

  test(
    'releases the session after the backend reports a disconnected link',
    () async {
      final backend = _FakeBackend();
      final controller = ConnectionController(
        backend: backend,
        telemetryPollInterval: const Duration(milliseconds: 5),
        connectionRefreshInterval: const Duration(milliseconds: 5),
      );
      addTearDown(controller.dispose);

      await controller.connect();
      backend.reportDisconnected();

      await backend.disconnectObserved.future.timeout(
        const Duration(seconds: 1),
      );
      await Future<void>.delayed(Duration.zero);

      expect(controller.status, ConnectionStatus.idle);
      expect(controller.snapshot, isNull);
      expect(controller.errorMessage, '飞控连接已断开');
      expect(backend.connected, isFalse);
    },
  );

  test('releases the session immediately after heartbeat loss', () async {
    final backend = _FakeBackend();
    final controller = ConnectionController(
      backend: backend,
      telemetryPollInterval: const Duration(milliseconds: 5),
      connectionRefreshInterval: const Duration(milliseconds: 5),
    );
    addTearDown(controller.dispose);

    await controller.connect();
    backend.reportHeartbeatLost();
    await backend.disconnectObserved.future.timeout(const Duration(seconds: 1));
    await Future<void>.delayed(Duration.zero);

    expect(controller.status, ConnectionStatus.idle);
    expect(controller.snapshot, isNull);
    expect(controller.messages, isEmpty);
    expect(controller.errorMessage, '飞控连接已断开');
    expect(backend.disconnectCalls, 1);
  });
}

final class _FakeBackend
    implements ConnectionSessionBackend, FlightControllerMessageSource {
  bool connected = false;
  ConnectionRequest? lastRequest;
  int connectCalls = 0;
  int disconnectCalls = 0;
  int sessionSnapshotCount = 0;
  int telemetrySnapshotCount = 0;
  int parameterSyncRequests = 0;
  Object? connectError;
  Object? disconnectError;
  Object? telemetryError;
  List<ConnectionSerialPort> ports = const [
    ConnectionSerialPort(name: 'Test port', path: '/dev/tty.test'),
  ];
  final StreamController<FlightControllerMessage> _messages =
      StreamController.broadcast();
  final Completer<void> nextTelemetryRefresh = Completer<void>();
  final Completer<void> nextSessionRefresh = Completer<void>();
  final Completer<void> telemetryRefreshStarted = Completer<void>();
  final Completer<void> disconnectObserved = Completer<void>();
  Completer<FlightDashboardData>? _blockedTelemetryRefresh;
  ConnectionStatus? _reportedStatus;
  String? _reportedDetail;

  @override
  Stream<FlightControllerMessage> get messages => _messages.stream;

  void emitMessage(FlightControllerMessage message) {
    _messages.add(message);
  }

  void blockNextTelemetryRefresh() {
    _blockedTelemetryRefresh = Completer<FlightDashboardData>();
  }

  void completeBlockedTelemetryRefresh() {
    final blockedRefresh = _blockedTelemetryRefresh;
    _blockedTelemetryRefresh = null;
    blockedRefresh!.complete(
      const FlightDashboardData(valid: true, mode: 'STALE', armed: false),
    );
  }

  void reportDisconnected() {
    _reportedStatus = ConnectionStatus.idle;
    _reportedDetail = 'disconnected';
  }

  void reportHeartbeatLost() {
    _reportedStatus = ConnectionStatus.idle;
    _reportedDetail = 'heartbeat_lost';
  }

  @override
  Future<List<ConnectionSerialPort>> listSerialPorts() async {
    return ports;
  }

  @override
  Future<ConnectionOperationResult> connect(ConnectionRequest request) async {
    final error = connectError;
    if (error != null) {
      throw error;
    }
    connectCalls += 1;
    lastRequest = request;
    connected = true;
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionOperationResult> disconnect() async {
    final error = disconnectError;
    if (error != null) {
      throw error;
    }
    disconnectCalls += 1;
    connected = false;
    if (!disconnectObserved.isCompleted) {
      disconnectObserved.complete();
    }
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionSessionSnapshot> loadSessionSnapshot(
    ConnectionRequest request,
  ) async {
    sessionSnapshotCount += 1;
    if (sessionSnapshotCount > 1 && !nextSessionRefresh.isCompleted) {
      nextSessionRefresh.complete();
    }
    return ConnectionSessionSnapshot(
      connection: AppConnectionSummary(
        status:
            _reportedStatus ??
            (connected ? ConnectionStatus.connected : ConnectionStatus.idle),
        label: 'Test vehicle',
        detail: _reportedDetail ?? 'ready',
      ),
      dashboard: await loadTelemetrySnapshot(),
    );
  }

  @override
  Future<FlightDashboardData> loadTelemetrySnapshot() async {
    final error = telemetryError;
    if (error != null) {
      throw error;
    }
    telemetrySnapshotCount += 1;
    if (telemetrySnapshotCount > 1 && !nextTelemetryRefresh.isCompleted) {
      nextTelemetryRefresh.complete();
    }
    final blockedRefresh = _blockedTelemetryRefresh;
    if (blockedRefresh != null) {
      if (!telemetryRefreshStarted.isCompleted) {
        telemetryRefreshStarted.complete();
      }
      return blockedRefresh.future;
    }
    return const FlightDashboardData(valid: true, mode: 'GUIDED', armed: false);
  }

  @override
  Future<ConnectionOperationResult> startParameterSync() async {
    parameterSyncRequests += 1;
    return const ConnectionOperationResult(success: true, message: 'ok');
  }

  @override
  Future<ConnectionBootstrapSnapshot> loadBootstrapSnapshot() async {
    return const ConnectionBootstrapSnapshot(
      parameterProgress: ParameterSyncProgress(received: 1, total: 2),
    );
  }

  @override
  void dispose() {
    _messages.close();
  }
}
