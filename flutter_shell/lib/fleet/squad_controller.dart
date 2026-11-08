import 'package:flutter/foundation.dart';

import 'fleet_controller.dart';
import 'squad_models.dart';
import 'squad_store.dart';

/// 小队列表与成员认领。
///
/// 小队只保存稳定身份，不保存运行期 vehicle_id；每次读取当前 fleet 快照时
/// 重新认领。执行语义（冻结快照、逐机 ACK）仍走现有的批量入口。
class SquadController extends ChangeNotifier {
  SquadController(this._fleet, {SquadStore? store})
    : _store = store ?? FileSquadStore() {
    _fleet.addListener(_handleFleetChanged);
  }

  final FleetController _fleet;
  final SquadStore _store;

  List<Squad> _squads = const [];
  bool _loading = true;
  bool _disposed = false;
  String? _loadError;

  /// 本次运行中已人工确认的临时身份（端点 + sysid）。
  ///
  /// 临时身份无法证明是同一架飞机，重启后一律重新确认；只有确认过的成员
  /// 才允许参与整队选中/执行。
  final Set<String> _confirmedTemporary = <String>{};

  List<Squad> get squads => _squads;
  bool get loading => _loading;

  /// 读取失败时给界面看的说明；原文件保留，不覆盖。
  String? get loadError => _loadError;

  Future<void> load() async {
    _loading = true;
    notifyListeners();
    try {
      final loaded = await _store.load();
      if (_disposed) return;
      _squads = List.unmodifiable(loaded);
      _confirmedTemporary.clear();
      _loadError = null;
    } on Object catch (error) {
      if (_disposed) return;
      _loadError = '读取小队失败：$error';
    } finally {
      if (!_disposed) {
        _loading = false;
        notifyListeners();
      }
    }
  }

  SquadClaim claimFor(Squad squad) => resolveSquadClaim(
    squad,
    _fleet.snapshot.vehicles,
    confirmedTemporary: _confirmedTemporary,
  );

  /// 人工确认小队里所有临时身份（本次运行内有效）。
  void confirmTemporaryMembers(Squad squad) {
    var changed = false;
    for (final member in squad.members) {
      if (member.temporary && _confirmedTemporary.add(member.identityKey)) {
        changed = true;
      }
    }
    if (changed && !_disposed) notifyListeners();
  }

  /// 用当前勾选的飞机创建小队。成员顺序 = 勾选顺序。
  Future<Squad> createFromSelection(String name) async {
    final trimmed = name.trim();
    if (trimmed.isEmpty) throw StateError('小队名称不能为空');
    final selected = _fleet.selectedVehicles
        .where((vehicle) => vehicle.stateFresh && vehicle.identified)
        .toList(growable: false);
    if (selected.isEmpty) {
      throw StateError('请先勾选至少一架已连接并完成识别的飞机');
    }
    final seen = <String>{};
    final members = <SquadMember>[];
    for (final vehicle in selected) {
      final identityKey = squadIdentityKey(vehicle);
      if (!seen.add(identityKey)) {
        throw StateError('选中的飞机中有相同身份，不能放进同一个小队');
      }
      members.add(
        SquadMember(identityKey: identityKey, label: vehicle.displayName),
      );
    }
    final squad = Squad(
      id: _newId(),
      name: trimmed,
      members: List.unmodifiable(members),
    );
    await _commit([..._squads, squad]);
    return squad;
  }

  Future<void> rename(String squadId, String name) async {
    final trimmed = name.trim();
    if (trimmed.isEmpty) throw StateError('小队名称不能为空');
    final index = _squads.indexWhere((squad) => squad.id == squadId);
    if (index < 0) throw StateError('小队不存在');
    final next = [..._squads];
    final current = next[index];
    next[index] = Squad(
      id: current.id,
      name: trimmed,
      revision: current.revision + 1,
      members: current.members,
    );
    await _commit(next);
  }

  Future<void> delete(String squadId) async {
    if (!_squads.any((squad) => squad.id == squadId)) {
      throw StateError('小队不存在');
    }
    await _commit([
      for (final squad in _squads)
        if (squad.id != squadId) squad,
    ]);
  }

  /// 保存成功后才替换内存状态：写盘失败不会让界面显示一份没落盘的小队。
  Future<void> _commit(List<Squad> next) async {
    await _store.save(next);
    if (_disposed) return;
    _squads = List.unmodifiable(next);
    notifyListeners();
  }

  String _newId() => 'squad-${DateTime.now().microsecondsSinceEpoch}';

  void _handleFleetChanged() {
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    _fleet.removeListener(_handleFleetChanged);
    super.dispose();
  }
}
