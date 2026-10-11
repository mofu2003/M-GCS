import 'dart:async';

import 'package:flutter/foundation.dart';

import 'native_core.dart';

/// 波形选择器里的两类实例：IMU（加速度计/陀螺仪）和磁力计。
enum WaveformInstanceKind { imu, mag }

/// 后端识别到的一个传感器实例。
final class WaveformInstance {
  const WaveformInstance({
    required this.index,
    required this.active,
    required this.name,
  });

  /// 实例号，0 对应 IMU1 / 磁力计 1，直接作为取数时的 instance 参数。
  final int index;

  /// 最近是否真的收到过这个实例的数据。
  final bool active;

  /// 解码出的传感器型号，参数未下发时是空串。
  final String name;

  /// 选择器上的显示名，例如 `IMU 1`。
  String label(String prefix) => '$prefix ${index + 1}';

  @override
  bool operator ==(Object other) =>
      other is WaveformInstance &&
      other.index == index &&
      other.active == active &&
      other.name == name;

  @override
  int get hashCode => Object.hash(index, active, name);
}

/// 切车离开时需要保存、切回时需要恢复的波形状态。
///
/// 采样点不在其中：波形缓冲是复用的原生内存，切车后重新取数即可，保存副本
/// 只会白占内存，而且没有使用方需要回看上一架飞机的曲线。
final class WaveformSnapshot {
  const WaveformSnapshot({
    required this.windowMs,
    required this.imuInstances,
    required this.magInstances,
    required this.imuInstance,
    required this.magInstance,
  });

  final int windowMs;
  final List<WaveformInstance> imuInstances;
  final List<WaveformInstance> magInstances;
  final int imuInstance;
  final int magInstance;
}

/// 实时波形（加速度计 / 陀螺仪 / 磁力计）的取数、缓冲与流速率协商。
///
/// 原先这一整块（约 320 行）长在 `ConnectionController` 里，导致每次切车都要在
/// 控制器里逐字段搬运波形状态。它真正需要的上下文只有原生核心、当前会话号、
/// 连接状态和初始化状态，全部通过构造回调注入；切车和断连时由控制器调用
/// [captureSnapshot] / [applySnapshot] / [reset] 归还状态，控制器不再直接读写
/// 波形的字段。
class WaveformController {
  WaveformController({
    required NativeCore? Function() native,
    required bool Function() ready,
    required bool Function() connected,
    required int? Function() sessionId,
  }) {
    _native = native;
    _ready = ready;
    _connected = connected;
    _sessionId = sessionId;
    imuPoints = Float32List(maxPoints * floatsPerPoint);
    magPoints = Float32List(maxPoints * floatsPerPoint);
  }

  /// 构造回调：控制器只接线，波形的所有原生访问都留在这个类内部。
  late final NativeCore? Function() _native;
  late final bool Function() _ready;
  late final bool Function() _connected;
  late final int? Function() _sessionId;

  /// 波形缓冲区上限。50s 窗口 @10Hz 约 500 帧，取 600 留余量，
  /// 正常就是逐帧原始点，抽稀只在采样率异常高时才兜底。
  /// 点数直接决定每帧的绘制和分配开销，这个值不要为了"看起来更细"随意调大。
  static const int maxPoints = 600;

  /// 每个采样点：相对毫秒 + 加速度 3 轴 + 角速度 3 轴 + 磁力 3 轴。
  static const int floatsPerPoint = 10;

  /// 波形页面默认显示的时间窗口（毫秒）。
  ///
  /// 这个值同时决定曲线的横坐标范围：后端按它裁出返回的点，图表用它算 maxX。
  static const int _defaultWindowMs = 40000;

  /// 波形页面取数间隔。
  ///
  /// 必须跟后端的 10Hz 采样对齐：这里曾经是 1 秒一次，结果飞控在持续采集，
  /// 界面却每秒才读一次缓冲，读数会在动作结束后成批跳出，表现为"摇完才变"。
  /// 波形与曲线统一按 10Hz 刷新，不要降到秒级。
  static const Duration _pollInterval = Duration(milliseconds: 100);

  /// 波形页面可见时请求的采样间隔（10Hz）。SCALED_IMU/2/3 各请求一次，
  /// 按显示需要取数，把串口带宽和缓冲写入量压到常规链路水平。
  static const int _streamIntervalUs = 100000;
  static const int _idleStreamIntervalUs = 500000;

  /// 波形页面按采样间隔请求的消息 ID：SCALED_IMU / SCALED_IMU2 / SCALED_IMU3。
  static const List<int> _messageIds = [26, 116, 129];

  /// 后端还没上报任何实例时的占位：只有 IMU1 / 磁力计 1，且未激活。
  static const WaveformInstance _defaultInstance = WaveformInstance(
    index: 0,
    active: false,
    name: '',
  );

  /// 波形采样点，由后端环形缓冲直接写入：每点 10 个 float
  /// （相对毫秒 + accel XYZ + gyro XYZ + mag XYZ）。
  /// IMU 和磁力计各自一份，因为两个面板会选不同的实例；
  /// 这些都是复用的缓冲区，不是每次轮询新建的列表。
  late final Float32List imuPoints;
  late final Float32List magPoints;

  /// 最近一次取数写入的点数。由取数路径写入，界面和测试读取。
  int imuPointCount = 0;
  int magPointCount = 0;

  final Map<int, List<double>> _magLatestByInstance = {};
  int _windowMs = _defaultWindowMs;
  List<WaveformInstance> _imuInstances = const [_defaultInstance];
  List<WaveformInstance> _magInstances = const [_defaultInstance];
  int _imuInstance = 0;
  int _magInstance = 0;

  Timer? _timer;
  bool _active = false;
  bool _disposed = false;

  /// 波形数据版本号：每次取数新数据入缓冲后自增，界面据此判断要不要重画。
  final ValueNotifier<int> changes = ValueNotifier(0);

  void dispose() {
    _disposed = true;
    _timer?.cancel();
    _timer = null;
    changes.dispose();
  }

  /// 波形页面显示的时间窗口（毫秒）。
  int get windowMs => _windowMs;

  /// 某一类实例的清单，按实例号升序。
  List<WaveformInstance> instances(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag ? _magInstances : _imuInstances;

  /// 当前选中的实例号。
  int instanceIndex(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag ? _magInstance : _imuInstance;

  /// 某一类实例最近一次取数写入的点，画图时直接读这里。
  Float32List points(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag ? magPoints : imuPoints;

  /// 某一类实例最近一次取数的点数。
  int pointCount(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag ? magPointCount : imuPointCount;

  List<double> magLatest(int index) =>
      _magLatestByInstance[index] ?? const [double.nan, double.nan, double.nan];

  /// 波形只在页面可见时轮询，不可见就完全停掉，省掉无谓的取数和重绘。
  void setActive(bool active) {
    if (_active == active) return;
    _active = active;
    _restartTimer();
    _applyStreamRate();
  }

  /// 选择波形页面显示的实例。取数定时器共用，所以换实例只是换取样区间，
  /// 立刻补一次取数，不用等下一个周期。
  void setInstance(WaveformInstanceKind kind, int index) {
    if (index < 0) return;
    if (kind == WaveformInstanceKind.mag) {
      if (index == _magInstance) return;
      _magInstance = index;
      magPointCount = 0;
    } else {
      if (index == _imuInstance) return;
      _imuInstance = index;
      imuPointCount = 0;
    }
    final id = _sessionId();
    if (_active && id != null && _connected()) {
      _poll(id);
    } else {
      _notify();
    }
  }

  /// 会话链路中断时停掉取数，但保留页面可见状态：重新连上后要接着轮询。
  void stopPolling() {
    _timer?.cancel();
    _timer = null;
  }

  /// 会话建立后按当前可见状态恢复取数。
  void restartPolling() => _restartTimer();

  /// 链路刚建立：如果波形页面正开着，把采样率提上去。
  void onLinkEstablished() {
    if (!_active) return;
    _applyStreamRate();
    _scheduleStreamRateRetry();
  }

  /// 保存切换车辆前的波形状态。
  WaveformSnapshot captureSnapshot() => WaveformSnapshot(
    windowMs: _windowMs,
    imuInstances: List.of(_imuInstances),
    magInstances: List.of(_magInstances),
    imuInstance: _imuInstance,
    magInstance: _magInstance,
  );

  /// 恢复切换回来那架飞机的波形状态。采样点不恢复，重新取数即可。
  void applySnapshot(WaveformSnapshot snapshot) {
    _windowMs = snapshot.windowMs;
    _imuInstances = List.of(snapshot.imuInstances);
    _magInstances = List.of(snapshot.magInstances);
    _imuInstance = snapshot.imuInstance;
    _magInstance = snapshot.magInstance;
  }

  /// 清空当前飞机的波形状态。切车和断连都走这里。
  void reset() {
    imuPointCount = 0;
    magPointCount = 0;
    _magLatestByInstance.clear();
    _imuInstances = const [_defaultInstance];
    _magInstances = const [_defaultInstance];
    _imuInstance = 0;
    _magInstance = 0;
  }

  /// 波形页面可见时把 SCALED_IMU/2/3 都提到 10Hz，离开时恢复常规速率。
  /// 复用通用的 mg_stream_set_interval，不新增后端专用接口。
  void _applyStreamRate() {
    if (!_ready() || !_connected()) return;
    final intervalUs = _active ? _streamIntervalUs : _idleStreamIntervalUs;
    for (final messageId in _messageIds) {
      try {
        _requireNative().setStreamInterval(messageId, intervalUs);
      } on Object {
        // 速率只是画质优化，失败不影响波形页面本身。
      }
    }
  }

  /// 后端识别到固件时会重新下发常规流速率，识别完成后再补一次高速率。
  void _scheduleStreamRateRetry() {
    Timer(const Duration(milliseconds: 1500), () {
      if (!_disposed && _active) _applyStreamRate();
    });
  }

  void _restartTimer() {
    _timer?.cancel();
    _timer = null;
    if (!_active || _disposed || !_ready()) return;
    _timer = Timer.periodic(_pollInterval, (_) {
      final id = _sessionId();
      if (id != null) _poll(id);
    });
  }

  void _poll(int id) {
    if (!_connected() || _sessionId() != id || _disposed) return;
    _readKind(WaveformInstanceKind.imu);
    _readKind(WaveformInstanceKind.mag);
    // 磁场强度要同时显示所有罗盘，而整窗取数只覆盖当前选中的实例，
    // 所以给其余实例各补一次"只要最新一点"的取数。
    _refreshMagLatestValues();
    // 实例数量随"最近有没有收到数据"变化，和取数同一个周期刷新，
    // 只有真的变了才通知界面，避免选择器每秒闪一下。
    _refreshInstances();
    _notify();
  }

  /// 刷新所有罗盘实例的最新磁场值，而不只是当前选中的那个。
  ///
  /// 磁场强度卡片是按实例平铺的，只更新选中实例会导致别的罗盘一直空着，
  /// 直到用户手动切过去。
  ///
  /// 这里用**时间窗口**而不是点数来裁剪：`copy_points` 在"可用点数多于
  /// max_points"时会走 min/max 降采样分支，而那个分支要求 `written + 2 <=
  /// max_points`，所以传 `target_points = 1` 会一个点都不返回。改成把窗口压到
  /// 1ms，缓冲里的点数是 1，走的是直通分支，拿到的就是真正的最新一帧。
  void _refreshMagLatestValues() {
    for (final instance in _magInstances) {
      if (instance.index == _magInstance) continue; // 整窗取数已经顺手更新过
      try {
        final native = _requireNative();
        final written = native.readWaveform(
          maxPoints: maxPoints,
          floatsPerPoint: floatsPerPoint,
          windowMs: 1, // 只保留最新一帧
          instance: instance.index,
        );
        if (written <= 0) continue;
        final base = (written - 1) * floatsPerPoint + 7;
        _magLatestByInstance[instance.index] = [
          native.waveformValueAt(base),
          native.waveformValueAt(base + 1),
          native.waveformValueAt(base + 2),
        ];
      } on Object {
        // 单个实例取不到数据不影响其它实例。
      }
    }
  }

  /// 取一类实例的波形。任何一个实例取不到数据都只是把点数清零，
  /// 不影响另一类实例的显示。
  void _readKind(WaveformInstanceKind kind) {
    final instance = instanceIndex(kind);
    var count = 0;
    try {
      final native = _requireNative();
      final written = native.readWaveform(
        maxPoints: maxPoints,
        floatsPerPoint: floatsPerPoint,
        windowMs: _windowMs,
        instance: instance,
      );
      if (written > 0) {
        final view = native.waveformView(written * floatsPerPoint);
        points(kind).setRange(0, view.length, view);
        count = written;
        if (kind == WaveformInstanceKind.mag) {
          final base = (written - 1) * floatsPerPoint + 7;
          _magLatestByInstance[instance] = [
            native.waveformValueAt(base),
            native.waveformValueAt(base + 1),
            native.waveformValueAt(base + 2),
          ];
        }
      }
    } on Object {
      count = 0;
    }

    if (kind == WaveformInstanceKind.mag) {
      magPointCount = count;
    } else {
      imuPointCount = count;
    }
  }

  /// 刷新后端识别到的实例清单，并把选中实例收敛到仍然存在的实例上。
  void _refreshInstances() {
    try {
      final payload =
          _requireNative().waveformInstances() as Map<String, Object?>;
      final imu = _parseInstances(payload['imu']);
      final mag = _parseInstances(payload['mag']);
      if (listEquals(imu, _imuInstances) && listEquals(mag, _magInstances)) {
        return;
      }
      _imuInstances = imu;
      _magInstances = mag;
      _imuInstance = _clampInstance(_imuInstance, imu);
      _magInstance = _clampInstance(_magInstance, mag);
    } on Object {
      // 实例清单只是选择器数据，取不到就保持上一次的结果。
    }
  }

  static List<WaveformInstance> _parseInstances(Object? raw) {
    final entries = raw as List<Object?>? ?? const [];
    final instances = <WaveformInstance>[];
    for (final entry in entries) {
      if (entry is! Map<String, Object?>) continue;
      final index = entry['index'] as int?;
      if (index == null || index < 0) continue;
      instances.add(
        WaveformInstance(
          index: index,
          active: entry['active'] == true,
          name: entry['name'] as String? ?? '',
        ),
      );
    }
    if (instances.isEmpty) {
      return const [_defaultInstance];
    }
    return instances;
  }

  /// 选中的实例消失时退回列表里的第一个，保证选择器始终指向有效实例。
  static int _clampInstance(int current, List<WaveformInstance> instances) =>
      instances.any((instance) => instance.index == current)
      ? current
      : instances.first.index;

  NativeCore _requireNative() {
    final native = _native();
    if (native == null) throw StateError('原生后端未初始化，无法读取波形');
    return native;
  }

  void _notify() {
    if (!_disposed) changes.value++;
  }
}
