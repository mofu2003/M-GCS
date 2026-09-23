import 'connection_models.dart';

typedef ParameterSyncStarter = Future<ConnectionOperationResult> Function();
typedef ParameterSyncSnapshotLoader =
    Future<ConnectionBootstrapSnapshot> Function();
typedef ParameterSyncSnapshotHandler =
    void Function(ConnectionBootstrapSnapshot snapshot);

/// Owns the lifecycle of the initial flight-controller parameter download.
///
/// The connection controller only coordinates when this operation starts and
/// when it should be polled. Progress tracking, stall detection, retries, and
/// terminal state are kept behind this small interface so they can evolve
/// without increasing connection lifecycle complexity.
final class ParameterSyncOperation {
  ParameterSyncOperation({
    required this.start,
    required this.loadSnapshot,
    required this.onSnapshot,
    required this.onChanged,
    this.stallTimeout = const Duration(seconds: 8),
    this.maxRetries = 2,
  });

  final ParameterSyncStarter start;
  final ParameterSyncSnapshotLoader loadSnapshot;
  final ParameterSyncSnapshotHandler onSnapshot;
  final void Function() onChanged;
  final Duration stallTimeout;
  final int maxRetries;

  ConnectionBootstrapState _state = ConnectionBootstrapState.idle;
  String? _error;
  int _retryCount = 0;
  int? _lastReceived;
  int? _lastTotal;
  DateTime? _lastProgressAt;
  DateTime? _lastRefreshAt;
  int _generation = 0;

  ConnectionBootstrapState get state => _state;
  String? get error => _error;

  Future<void> startIfNeeded() async {
    if (_state != ConnectionBootstrapState.idle) return;
    _retryCount = 0;
    _state = ConnectionBootstrapState.downloading;
    _error = null;
    onChanged();
    await _request(_generation);
  }

  Future<void> refreshIfDue() async {
    if (_state == ConnectionBootstrapState.idle ||
        _state == ConnectionBootstrapState.failed) {
      return;
    }

    final refreshInterval = _state == ConnectionBootstrapState.downloading
        ? const Duration(milliseconds: 250)
        : const Duration(seconds: 1);
    final lastRefresh = _lastRefreshAt;
    if (lastRefresh != null &&
        DateTime.now().difference(lastRefresh) < refreshInterval) {
      return;
    }
    await _refreshSnapshot(_generation);
  }

  void reset() {
    _generation++;
    _state = ConnectionBootstrapState.idle;
    _error = null;
    _retryCount = 0;
    _lastReceived = null;
    _lastTotal = null;
    _lastProgressAt = null;
    _lastRefreshAt = null;
  }

  Future<void> _request(int generation) async {
    _lastReceived = null;
    _lastTotal = null;
    _lastProgressAt = DateTime.now();
    try {
      final result = await start();
      if (!_isCurrent(generation)) return;
      if (!result.success) {
        _fail(generation, result.message);
        return;
      }
      await _refreshSnapshot(generation);
    } on Object catch (error) {
      _fail(generation, error.toString());
    }
  }

  Future<void> _refreshSnapshot(int generation) async {
    try {
      final bootstrap = await loadSnapshot();
      if (!_isCurrent(generation)) return;
      final now = DateTime.now();
      _lastRefreshAt = now;
      var retryAfterUpdate = false;

      if (_state == ConnectionBootstrapState.downloading) {
        final progress = bootstrap.parameterProgress;
        if (progress.isComplete) {
          _state = ConnectionBootstrapState.complete;
          _error = null;
        } else {
          final changed =
              _lastReceived != progress.received ||
              _lastTotal != progress.total;
          if (changed) {
            _lastReceived = progress.received;
            _lastTotal = progress.total;
            _lastProgressAt = now;
          } else if (_lastProgressAt != null &&
              now.difference(_lastProgressAt!) >= stallTimeout) {
            retryAfterUpdate = true;
          }
        }
      }

      onSnapshot(bootstrap);
      onChanged();
      if (retryAfterUpdate) await _retryAfterStall(generation);
    } on Object catch (error) {
      _fail(generation, error.toString());
    }
  }

  Future<void> _retryAfterStall(int generation) async {
    if (!_isCurrent(generation) ||
        _state != ConnectionBootstrapState.downloading) {
      return;
    }
    if (_retryCount >= maxRetries) {
      _fail(generation, '参数下载连续停滞，已重试 $maxRetries 次');
      return;
    }

    _retryCount++;
    _error = '参数下载停滞，正在重试 ($_retryCount/$maxRetries)';
    onChanged();
    await _request(generation);
  }

  bool _isCurrent(int generation) => generation == _generation;

  void _fail(int generation, String message) {
    if (!_isCurrent(generation)) return;
    _state = ConnectionBootstrapState.failed;
    _error = message;
    onChanged();
  }
}
