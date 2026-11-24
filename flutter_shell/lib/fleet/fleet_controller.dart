import 'package:flutter/foundation.dart';

import 'fleet_backend.dart';
import 'fleet_models.dart';

/// MV1 基础：端点注册、稳定焦点、精确关闭与目标凭证生命周期。
///
/// active 只表示 UI 焦点。这里只管连接/焦点，不负责飞行算法、参数或任务。
/// MV6 增加独立 selectedVehicles 集合和显式批量目标；批量执行期间冻结焦点
/// 变更，但绝不把 active vehicle 当成隐式批量目标。
class FleetController extends ChangeNotifier {
  FleetController(
    this._backend, {
    this.canChangeFocus,
    this.executeBatchCommand,
  });

  /// Host gate while per-vehicle UI rebinding is not yet implemented.
  /// Null is used by standalone fleet tests; the current single-session host
  /// blocks switching away from its attached session.
  final bool Function()? canChangeFocus;

  final FleetBackend _backend;
  final Future<FleetBatchCommandResult> Function(
    FleetVehicleSummary vehicle,
    FleetBatchAction action,
  )?
  executeBatchCommand;
  FleetSnapshot _snapshot = FleetSnapshot(
    vehicles: const [],
    links: const [],
    activeVehicleId: null,
  );
  int _focusRevision = 0;
  int _targetOperations = 0;
  int _batchOperations = 0;
  FleetTarget? _pinnedTarget;
  final Set<int> _selectedVehicleIds = <int>{};
  bool _disposed = false;
  bool _batchStopRequested = false;
  FleetBatchProgress? _batchProgress;

  FleetBatchProgress? get batchProgress => _batchProgress;

  FleetSnapshot get snapshot => _snapshot;
  Set<int> get selectedVehicleIds => Set.unmodifiable(_selectedVehicleIds);
  int get focusRevision => _focusRevision;
  bool get targetOperationActive =>
      _targetOperations > 0 || _batchOperations > 0;

  List<FleetVehicleSummary> get selectedVehicles => [
    for (final vehicle in _snapshot.vehicles)
      if (_selectedVehicleIds.contains(vehicle.id)) vehicle,
  ];

  void refresh() {
    _requireAlive();
    var next = _backend.snapshot();
    // QGC accepts the first discovered vehicle as active. Later links must
    // not steal focus from an already selected vehicle.
    if (next.activeVehicleId == null &&
        next.vehicles.isNotEmpty &&
        !targetOperationActive) {
      _backend.select(next.vehicles.first.id);
      next = _backend.snapshot();
    }
    final selectedBefore = Set<int>.of(_selectedVehicleIds);
    _selectedVehicleIds.removeWhere((id) => next.vehicle(id) == null);
    final selectionChanged =
        selectedBefore.length != _selectedVehicleIds.length ||
        !selectedBefore.containsAll(_selectedVehicleIds);
    final changed =
        next.activeVehicleId != _snapshot.activeVehicleId ||
        !listEquals(next.vehicles, _snapshot.vehicles) ||
        !listEquals(next.links, _snapshot.links) ||
        selectionChanged;
    final previousActive = _snapshot.activeVehicle;
    final nextActive = next.activeVehicle;
    if (next.activeVehicleId != _snapshot.activeVehicleId ||
        previousActive?.connected != nextActive?.connected ||
        previousActive?.systemId != nextActive?.systemId) {
      // Same session disconnect/reconnect or identity replacement invalidates
      // confirmations too, even without an explicit UI switch.
      _focusRevision++;
    }
    _snapshot = next;
    if (changed) notifyListeners();
  }

  /// 首链路可自动选择；添加第二条不能抢走已选车辆的 UI 焦点。
  /// 重复端点直接拒绝，不通过 SO_REUSEADDR 重复绑定同一个 UDP 端口。
  int openLink(FleetConnectionSpec connection, {bool activateIfFirst = true}) {
    _requireAlive();
    refresh();
    if (_snapshot.links.any(
          (link) =>
              FleetConnectionSpec(link.endpoint).resourceKey ==
              connection.resourceKey,
        ) ||
        _snapshot.vehicles.any(
          (v) =>
              FleetConnectionSpec(v.endpoint).resourceKey ==
              connection.resourceKey,
        )) {
      throw StateError('该链路已经打开');
    }
    final id = _backend.open(connection);
    try {
      refresh();
      if (activateIfFirst) {
        final link = _snapshot.link(id);
        final vehicleId = link?.vehicleId;
        if (vehicleId != null &&
            _snapshot.activeVehicleId == null &&
            !targetOperationActive) {
          selectVehicle(vehicleId);
        }
      }
      return id;
    } on Object {
      // 打开成功而后续初始化失败，仅回收本次链路，不关闭已有飞机。
      _backend.close(id);
      refresh();
      rethrow;
    }
  }

  void selectVehicle(int id) {
    _requireAlive();
    refresh();
    if (_snapshot.vehicle(id) == null) throw StateError('飞控会话不存在');
    if (_snapshot.activeVehicleId == id) return;
    if (targetOperationActive) throw StateError('指令执行中，暂不能切换飞机');
    if (canChangeFocus?.call() == false) {
      throw StateError('当前飞机有未完成操作，暂不能切换飞机');
    }
    _backend.select(id);
    refresh();
    if (_snapshot.activeVehicleId != id) throw StateError('切换飞机失败');
  }

  /// 选择集合只影响批量动作，不改变 active vehicle。重复点击同一架飞机即可
  /// 取消选择；这样不会要求用户先切焦点再参与批量控制。
  void toggleVehicleSelection(int id) {
    _requireAlive();
    refresh();
    if (_snapshot.vehicle(id) == null) throw StateError('飞控会话不存在');
    if (!_selectedVehicleIds.add(id)) _selectedVehicleIds.remove(id);
    if (!_snapshot.vehicles.any((vehicle) => vehicle.id == id)) {
      _selectedVehicleIds.remove(id);
    }
    notifyListeners();
  }

  void selectAllVehicles() {
    _requireAlive();
    refresh();
    _selectedVehicleIds
      ..clear()
      ..addAll(_snapshot.vehicles.map((vehicle) => vehicle.id));
    notifyListeners();
  }

  /// 用给定集合替换批量选择（小队“选中在线成员”用）。只影响批量动作，
  /// 不改变 active vehicle。
  void selectVehicles(Iterable<int> ids) {
    _requireAlive();
    refresh();
    final next = <int>{
      for (final id in ids)
        if (_snapshot.vehicle(id) != null) id,
    };
    if (setEquals(next, _selectedVehicleIds)) return;
    _selectedVehicleIds
      ..clear()
      ..addAll(next);
    notifyListeners();
  }

  void clearVehicleSelection() {
    _requireAlive();
    if (_selectedVehicleIds.isEmpty) return;
    _selectedVehicleIds.clear();
    notifyListeners();
  }

  void selectLink(int id) {
    _requireAlive();
    refresh();
    FleetLinkSummary? selected;
    for (final link in _snapshot.links) {
      if (link.id == id && link.vehicleId != null) {
        selected = link;
        break;
      }
    }
    if (selected?.vehicleId == null) {
      throw StateError('链路尚未识别到飞控');
    }
    selectVehicle(selected!.vehicleId!);
  }

  void closeLink(int id) {
    _requireAlive();
    refresh();
    final hasLink =
        _snapshot.links.any((link) => link.id == id) ||
        _snapshot.vehicle(id) != null;
    if (!hasLink) throw StateError('链路不存在');
    if (targetOperationActive) throw StateError('指令执行中，暂不能关闭链路');
    _backend.close(id);
    refresh();
  }

  void closeRoute(int routeId) {
    _requireAlive();
    refresh();
    final hasRoute = _snapshot.links.any((link) => link.routeId == routeId);
    if (!hasRoute) throw StateError('链路 route 不存在');
    if (targetOperationActive) throw StateError('指令执行中，暂不能关闭链路');
    _backend.closeRoute(routeId);
    refresh();
  }

  FleetTarget captureTarget({int? expectedVehicleId}) {
    refresh();
    final active = _snapshot.activeVehicle;
    if (active == null || !active.connected || !active.identified) {
      throw StateError('当前没有已连接的目标飞控');
    }
    if (expectedVehicleId != null && active.id != expectedVehicleId) {
      throw StateError('当前飞控尚未完成焦点绑定，请重新选择飞机');
    }
    return FleetTarget(vehicleId: active.id, focusRevision: _focusRevision);
  }

  FleetBatchTarget captureBatchTarget(FleetBatchAction action) {
    _requireAlive();
    refresh();
    var vehicles = selectedVehicles
        .where((vehicle) => vehicle.stateFresh && vehicle.identified)
        .toList(growable: false);
    if (vehicles.isEmpty) {
      throw StateError('请先选择至少一架已连接并完成识别的飞机');
    }
    // 开始任务只面向本地已回读到任务的成员：MISSION_START 会让没有任务的飞机
    // 进入 AUTO，arm_if_needed 还会顺带解锁。确认条与实际目标保持同一集合。
    if (action == FleetBatchAction.startMission) {
      vehicles = vehicles
          .where((vehicle) => vehicle.missionValid && vehicle.missionTotal > 0)
          .toList(growable: false);
      if (vehicles.isEmpty) {
        throw StateError('选中的飞机都没有回读到有效任务');
      }
    }
    final conflict = fleetSystemIdConflict(vehicles);
    if (conflict != null) throw StateError(conflict);
    return FleetBatchTarget(
      action: action,
      vehicleIds: vehicles.map((vehicle) => vehicle.id),
      systemIds: vehicles.map((vehicle) => vehicle.systemId),
    );
  }

  /// 执行一批已冻结的显式目标。命令逐机下发，失败不会中断其他飞机；返回
  /// 每架飞机的独立结果，不修改 active focus。
  Future<FleetBatchResult> runBatch(FleetBatchTarget target) async {
    _requireAlive();
    if (targetOperationActive) throw StateError('已有飞行指令正在执行');
    if (target.vehicleIds.isEmpty) throw StateError('批量目标为空');
    if (target.vehicleIds.length != target.systemIds.length ||
        target.vehicleIds.toSet().length != target.vehicleIds.length) {
      throw StateError('批量目标身份无效');
    }
    refresh();
    final conflict = fleetSystemIdConflict(
      target.vehicleIds.map(_snapshot.vehicle).whereType<FleetVehicleSummary>(),
    );
    if (conflict != null) throw StateError(conflict);
    final executor = executeBatchCommand;
    if (executor == null) throw StateError('当前后端不支持批量飞行指令');

    _batchOperations++;
    _batchStopRequested = false;
    final results = <FleetBatchCommandResult>[];
    _publishBatchProgress(target, results);
    try {
      for (var index = 0; index < target.vehicleIds.length; index++) {
        final vehicleId = target.vehicleIds[index];
        final systemId = target.systemIds[index];
        if (!_disposed) refresh();
        if (_batchStopRequested || _disposed) {
          results.add(
            FleetBatchCommandResult.failure(
              vehicleId: vehicleId,
              systemId: systemId,
              status: FleetBatchCommandStatus.notSent,
              message: '已停止后续派发',
            ),
          );
          continue;
        }
        final vehicle = _snapshot.vehicle(vehicleId);
        if (vehicle == null ||
            !vehicle.stateFresh ||
            !vehicle.identified ||
            vehicle.systemId != systemId) {
          results.add(
            FleetBatchCommandResult.failure(
              vehicleId: vehicleId,
              systemId: systemId,
              status: FleetBatchCommandStatus.error,
              message: '飞机状态已失效或身份已变化',
            ),
          );
          _publishBatchProgress(target, results);
          continue;
        }
        try {
          final response = executor(vehicle, target.action);
          _publishBatchProgress(target, results, runningVehicleId: vehicleId);
          final result = await response;
          if (result.vehicleId != vehicleId || result.systemId != systemId) {
            results.add(
              FleetBatchCommandResult.failure(
                vehicleId: vehicleId,
                systemId: systemId,
                status: FleetBatchCommandStatus.error,
                message: '批量执行器返回了错误的目标飞机',
              ),
            );
          } else {
            results.add(result);
          }
        } on Object catch (error) {
          results.add(
            FleetBatchCommandResult.failure(
              vehicleId: vehicleId,
              systemId: systemId,
              status: FleetBatchCommandStatus.error,
              message: error.toString().replaceFirst('Bad state: ', ''),
            ),
          );
        }
        _publishBatchProgress(target, results);
      }
    } finally {
      _batchOperations--;
      _publishBatchProgress(target, results);
    }
    return FleetBatchResult(action: target.action, results: results);
  }

  /// 已派发命令继续等待原生结果，停止尚未派发的成员。
  void stopBatchDispatch() {
    _requireAlive();
    if (_batchOperations == 0 || _batchStopRequested) return;
    _batchStopRequested = true;
    final progress = _batchProgress!;
    _publishBatchProgress(
      progress.target,
      progress.results,
      runningVehicleId: progress.runningVehicleId,
    );
  }

  void _publishBatchProgress(
    FleetBatchTarget target,
    List<FleetBatchCommandResult> results, {
    int? runningVehicleId,
  }) {
    _batchProgress = FleetBatchProgress(
      target: target,
      results: results,
      running: _batchOperations > 0,
      stopRequested: _batchStopRequested || _disposed,
      runningVehicleId: runningVehicleId,
    );
    if (!_disposed) notifyListeners();
  }

  void validateTarget(FleetTarget target) {
    refresh();
    if (target.vehicleId != _snapshot.activeVehicleId ||
        target.focusRevision != _focusRevision ||
        _snapshot.activeVehicle?.connected != true) {
      throw StateError('目标飞机已切换或断开，请重新确认指令');
    }
  }

  /// 保持显式目标凭证直到异步操作完成；期间拒绝 UI 切机和关闭链路。
  /// native 意外断链导致焦点变化时凭证失效，不自动重定向命令到替代车辆。
  Future<T> runForTarget<T>(
    FleetTarget target,
    Future<T> Function() execute,
  ) async {
    validateTarget(target);
    _pinnedTarget = target;
    _targetOperations++;
    try {
      final result = await execute();
      validateTarget(target);
      return result;
    } finally {
      _targetOperations--;
      if (_targetOperations == 0) _pinnedTarget = null;
    }
  }

  /// Revalidate before every active-focus native command, including the
  /// next step of an async takeoff/arm sequence after an await.
  void validatePinnedTarget() {
    final target = _pinnedTarget;
    if (target != null) validateTarget(target);
  }

  void _requireAlive() {
    if (_disposed) throw StateError('多机连接框架已关闭');
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }
}
