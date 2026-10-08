import 'package:flutter/foundation.dart';

import 'fleet_backend.dart';
import 'fleet_models.dart';

/// MV1 基础：端点注册、稳定焦点、精确关闭与目标凭证生命周期。
///
/// active 只表示 UI 焦点。这里只管连接/焦点，不负责飞行算法、参数或任务。
/// 当前不提供 selectedVehicles/批量指令，避免混淆焦点与操作集合。
class FleetController extends ChangeNotifier {
  FleetController(this._backend, {this.canChangeFocus});

  /// Host gate while per-vehicle UI rebinding is not yet implemented.
  /// Null is used by standalone fleet tests; the current single-session host
  /// blocks switching away from its attached session.
  final bool Function()? canChangeFocus;

  final FleetBackend _backend;
  FleetSnapshot _snapshot = FleetSnapshot(
    vehicles: const [],
    links: const [],
    activeVehicleId: null,
  );
  int _focusRevision = 0;
  int _targetOperations = 0;
  FleetTarget? _pinnedTarget;
  bool _disposed = false;

  FleetSnapshot get snapshot => _snapshot;
  int get focusRevision => _focusRevision;
  bool get targetOperationActive => _targetOperations > 0;

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
    final changed =
        next.activeVehicleId != _snapshot.activeVehicleId ||
        !listEquals(next.vehicles, _snapshot.vehicles) ||
        !listEquals(next.links, _snapshot.links);
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

  /// Revalidate before every legacy active-focus native command, including
  /// the next step of an async takeoff/arm sequence after an await.
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
