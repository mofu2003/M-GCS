import 'dart:async';

/// Runs an asynchronous refresh on a fixed cadence without overlapping ticks.
final class PollingTask {
  PollingTask({required this.interval, required this.onTick});

  final Duration interval;
  final Future<void> Function() onTick;

  Timer? _timer;
  bool _inFlight = false;
  bool _disposed = false;
  bool _paused = false;

  void start({bool runImmediately = true}) {
    if (_disposed || _paused || _timer != null) return;
    if (runImmediately) unawaited(runOnce());
    _timer = Timer.periodic(interval, (_) => unawaited(runOnce()));
  }

  void pause() {
    if (_disposed) return;
    _paused = true;
    _timer?.cancel();
    _timer = null;
  }

  void resume({bool runImmediately = true}) {
    if (_disposed) return;
    _paused = false;
    start(runImmediately: runImmediately);
  }

  Future<void> runOnce() async {
    if (_disposed || _inFlight) return;
    _inFlight = true;
    try {
      await onTick();
    } finally {
      _inFlight = false;
    }
  }

  void dispose() {
    _disposed = true;
    _timer?.cancel();
    _timer = null;
  }
}
