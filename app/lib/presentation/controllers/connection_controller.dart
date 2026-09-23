import 'dart:async';

import 'package:flutter/foundation.dart';

import '../../application/connection_models.dart';
import '../../application/connection_session_backend.dart';
import '../../application/parameter_sync_operation.dart';
import '../../application/session_capabilities.dart';

final class ConnectionController extends ChangeNotifier {
  ConnectionController({
    required this.backend,
    this.telemetryPollInterval = const Duration(milliseconds: 50),
    this.connectionRefreshInterval = const Duration(seconds: 1),
    this.parameterSyncStallTimeout = const Duration(seconds: 8),
    this.parameterSyncMaxRetries = 2,
  }) : capabilities = SessionCapabilities(backend) {
    _parameterSync = ParameterSyncOperation(
      start: backend.startParameterSync,
      loadSnapshot: backend.loadBootstrapSnapshot,
      onSnapshot: _applyBootstrapSnapshot,
      onChanged: notifyListeners,
      stallTimeout: parameterSyncStallTimeout,
      maxRetries: parameterSyncMaxRetries,
    );
    if (backend case final FlightControllerMessageSource source) {
      _messageSubscription = source.messages.listen(_recordMessage);
    }
    if (backend case final ConnectionBackendAvailability unavailable) {
      _status = ConnectionStatus.error;
      _errorMessage = unavailable.unavailableReason;
    }
  }

  final ConnectionSessionBackend backend;
  final SessionCapabilities capabilities;
  final Duration telemetryPollInterval;
  final Duration connectionRefreshInterval;
  final Duration parameterSyncStallTimeout;
  final int parameterSyncMaxRetries;

  ConnectionStatus _status = ConnectionStatus.idle;
  ConnectionRequest _request = const ConnectionRequest.serial(port: '');
  List<ConnectionSerialPort> _ports = const [];
  ConnectionSessionSnapshot? _snapshot;
  String? _errorMessage;
  Timer? _telemetryTimer;
  Future<void> _operationTail = Future<void>.value();
  bool _telemetryPollScheduled = false;
  bool _telemetryRefreshInProgress = false;
  bool _portRefreshInProgress = false;
  DateTime? _lastConnectionRefresh;
  late final ParameterSyncOperation _parameterSync;
  StreamSubscription<FlightControllerMessage>? _messageSubscription;
  final List<FlightControllerMessage> _messages = [];
  int _sessionEpoch = 0;

  ConnectionStatus get status => _status;
  ConnectionRequest get request => _request;
  List<ConnectionSerialPort> get ports => _ports;
  ConnectionSessionSnapshot? get snapshot => _snapshot;
  String? get errorMessage => _errorMessage;
  ConnectionBootstrapState get bootstrapState => _parameterSync.state;
  String? get bootstrapError => _parameterSync.error;
  int get sessionEpoch => _sessionEpoch;
  List<FlightControllerMessage> get messages => List.unmodifiable(_messages);
  bool get isBusy =>
      _status == ConnectionStatus.connecting ||
      _status == ConnectionStatus.disconnecting;

  Future<void> initialize() async {
    try {
      _ports = await backend.listSerialPorts();
      _errorMessage = null;
    } on Object catch (error) {
      _ports = const [];
      _status = ConnectionStatus.error;
      _errorMessage = '串口枚举失败：$error';
    }
    notifyListeners();
  }

  /// Re-enumerates ports without changing the active selection. This allows a
  /// flight controller plugged in after launch to appear in the existing menu.
  Future<void> refreshSerialPorts() async {
    if (_portRefreshInProgress ||
        isBusy ||
        _status == ConnectionStatus.connected) {
      return;
    }

    _portRefreshInProgress = true;
    try {
      final ports = await backend.listSerialPorts();
      if (_samePorts(_ports, ports)) {
        return;
      }
      _ports = ports;
      notifyListeners();
    } on Object {
      // Keep the last usable list. A transient USB enumeration error should
      // not replace a physical-disconnect notice with an unrelated error.
    } finally {
      _portRefreshInProgress = false;
    }
  }

  bool _samePorts(
    List<ConnectionSerialPort> left,
    List<ConnectionSerialPort> right,
  ) {
    if (left.length != right.length) {
      return false;
    }
    for (var index = 0; index < left.length; index++) {
      final a = left[index];
      final b = right[index];
      if (a.name != b.name ||
          a.path != b.path ||
          a.description != b.description ||
          a.manufacturer != b.manufacturer) {
        return false;
      }
    }
    return true;
  }

  void setSerialPort(String port) {
    _request = ConnectionRequest.serial(
      port: port,
      baudRate: _request.baudRate ?? 115200,
    );
    notifyListeners();
  }

  void setBaudRate(int baudRate) {
    _request = ConnectionRequest.serial(
      port: _request.port ?? '',
      baudRate: baudRate,
    );
    notifyListeners();
  }

  void setNetworkEndpoint(ConnectionTransport transport, String endpoint) {
    if (transport == ConnectionTransport.serial) {
      return;
    }
    _request = ConnectionRequest.network(
      transport: transport,
      endpoint: endpoint.trim(),
    );
    notifyListeners();
  }

  Future<void> connect() => _runSerialized(_connect);

  Future<void> _connect() async {
    _advanceSessionEpoch();
    _setStatus(ConnectionStatus.connecting);
    try {
      final result = await backend.connect(_request);
      if (!result.success) {
        _errorMessage = result.message;
        _setStatus(ConnectionStatus.error);
        return;
      }
      if (!await _refreshSession()) {
        return;
      }
      _errorMessage = null;
      _startTelemetryPolling();
    } on Object catch (error) {
      _snapshot = null;
      _resetBootstrap();
      _errorMessage = '连接失败：$error';
      _setStatus(ConnectionStatus.error);
    }
  }

  Future<void> refreshTelemetry() {
    return _runSerialized(() async {
      if (_snapshot == null) {
        return;
      }
      try {
        await _refreshTelemetrySnapshot();
      } on Object catch (error) {
        _reportTelemetryFailure(error);
      }
    });
  }

  Future<void> disconnect() => _runSerialized(_disconnect);

  Future<void> _disconnect() async {
    _advanceSessionEpoch();
    _telemetryTimer?.cancel();
    _telemetryTimer = null;
    _setStatus(ConnectionStatus.disconnecting);
    try {
      final result = await backend.disconnect();
      if (!result.success) {
        _errorMessage = result.message;
        _setStatus(ConnectionStatus.error);
        return;
      }
      _snapshot = null;
      _errorMessage = null;
      _resetBootstrap();
      _setStatus(ConnectionStatus.idle);
    } on Object catch (error) {
      _snapshot = null;
      _resetBootstrap();
      _errorMessage = '断开连接失败：$error';
      _setStatus(ConnectionStatus.error);
    }
  }

  Future<void> _runSerialized(Future<void> Function() operation) {
    final next = _operationTail.then<void>(
      (_) => operation(),
      onError: (Object error, StackTrace stackTrace) => operation(),
    );
    _operationTail = next.then<void>(
      (_) {},
      onError: (Object error, StackTrace stackTrace) {},
    );
    return next;
  }

  void _setStatus(ConnectionStatus status) {
    _status = status;
    notifyListeners();
  }

  void _advanceSessionEpoch() {
    _sessionEpoch++;
  }

  Future<bool> _refreshSession() async {
    final snapshot = await backend.loadSessionSnapshot(_request);
    _snapshot = snapshot;
    _lastConnectionRefresh = DateTime.now();
    final state = snapshot.connection.detail;
    if (snapshot.connection.status == ConnectionStatus.connected) {
      _status = ConnectionStatus.connected;
      _errorMessage = null;
      await _startParameterSyncIfNeeded();
    } else if (state == 'waiting_heartbeat') {
      _status = ConnectionStatus.connecting;
      _errorMessage = null;
    } else if (state == 'heartbeat_lost' || state == 'disconnected') {
      await _finalizeDisconnectedSession();
      return false;
    }
    notifyListeners();
    return true;
  }

  Future<void> _finalizeDisconnectedSession() async {
    _advanceSessionEpoch();
    _telemetryTimer?.cancel();
    _telemetryTimer = null;
    _setStatus(ConnectionStatus.disconnecting);
    late final ConnectionOperationResult result;
    try {
      result = await backend.disconnect();
    } on Object catch (error) {
      result = ConnectionOperationResult(
        success: false,
        message: error.toString(),
      );
    }
    _snapshot = null;
    _lastConnectionRefresh = null;
    _resetBootstrap();
    _messages.clear();
    _errorMessage = result.success
        ? '飞控连接已断开'
        : '飞控连接已断开，清理失败：${result.message}';
    _setStatus(ConnectionStatus.idle);
  }

  void _startTelemetryPolling() {
    _telemetryTimer?.cancel();
    _telemetryTimer = Timer.periodic(telemetryPollInterval, (_) {
      if (_telemetryPollScheduled) {
        return;
      }
      _telemetryPollScheduled = true;
      unawaited(
        _runSerialized(() async {
          try {
            await _pollTelemetry();
          } finally {
            _telemetryPollScheduled = false;
          }
        }),
      );
    });
  }

  Future<void> _pollTelemetry() async {
    if (_telemetryRefreshInProgress ||
        _status == ConnectionStatus.idle ||
        _status == ConnectionStatus.disconnecting ||
        _status == ConnectionStatus.error) {
      return;
    }

    _telemetryRefreshInProgress = true;
    try {
      try {
        await _refreshTelemetrySnapshot();
      } on Object catch (error) {
        _reportTelemetryFailure(error);
        return;
      }
      await _refreshBootstrapIfDue();
      final lastConnectionRefresh = _lastConnectionRefresh;
      if (lastConnectionRefresh == null ||
          DateTime.now().difference(lastConnectionRefresh) >=
              connectionRefreshInterval) {
        try {
          await _refreshSession();
        } on Object catch (error) {
          _reportConnectionFailure(error);
        }
      }
    } finally {
      _telemetryRefreshInProgress = false;
    }
  }

  Future<void> _refreshTelemetrySnapshot() async {
    final previous = _snapshot;
    if (previous == null) {
      return;
    }

    final dashboard = await backend.loadTelemetrySnapshot();
    _snapshot = ConnectionSessionSnapshot(
      connection: previous.connection,
      dashboard: dashboard,
      bootstrap: previous.bootstrap,
      rawDiagnostics: previous.rawDiagnostics,
    );
    if (_status == ConnectionStatus.connected) {
      _errorMessage = null;
    }
    notifyListeners();
  }

  void _reportTelemetryFailure(Object error) {
    _errorMessage = '遥测刷新失败：$error';
    notifyListeners();
  }

  void _reportConnectionFailure(Object error) {
    _telemetryTimer?.cancel();
    _telemetryTimer = null;
    _snapshot = null;
    _resetBootstrap();
    _errorMessage = '连接状态刷新失败：$error';
    _setStatus(ConnectionStatus.error);
  }

  Future<void> _startParameterSyncIfNeeded() async {
    await _parameterSync.startIfNeeded();
  }

  Future<void> _refreshBootstrapIfDue() async {
    await _parameterSync.refreshIfDue();
  }

  void _applyBootstrapSnapshot(ConnectionBootstrapSnapshot bootstrap) {
    final previous = _snapshot;
    if (previous == null) {
      return;
    }
    _snapshot = ConnectionSessionSnapshot(
      connection: previous.connection,
      dashboard: previous.dashboard,
      bootstrap: bootstrap,
      rawDiagnostics: previous.rawDiagnostics,
    );
  }

  void _resetBootstrap() => _parameterSync.reset();

  void _recordMessage(FlightControllerMessage message) {
    _messages.add(message);
    if (_messages.length > 200) {
      _messages.removeRange(0, _messages.length - 200);
    }
    notifyListeners();
  }

  @override
  void dispose() {
    _telemetryTimer?.cancel();
    _messageSubscription?.cancel();
    backend.dispose();
    super.dispose();
  }
}
