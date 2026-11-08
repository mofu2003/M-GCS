import 'package:flutter/foundation.dart';

import 'fleet_models.dart';

/// 小队成员的稳定身份。
///
/// 运行期 `vehicle_id` 每次启动都会变，不能写进小队。硬件 UID 存在时用它
/// （换链路、换 USB 口都不变）；否则退回“端点 + sysid”，并在界面上标为临时身份。
String squadIdentityKey(FleetVehicleSummary vehicle) {
  final uid = vehicle.hardwareUid.trim();
  if (uid.isNotEmpty) return uid;
  return 'ep:${vehicle.endpoint}|sys:${vehicle.systemId}';
}

/// 没有硬件 UID 的身份：重启/换端口后需要人工确认。
bool squadIdentityIsTemporary(String identityKey) =>
    !identityKey.startsWith('uid2:') && !identityKey.startsWith('uid:');

@immutable
class SquadMember {
  const SquadMember({required this.identityKey, required this.label});

  final String identityKey;

  /// 保存时的显示名，成员离线时仍可读。
  final String label;

  bool get temporary => squadIdentityIsTemporary(identityKey);

  Map<String, Object?> toJson() => {
    'identityKey': identityKey,
    'label': label,
  };

  static SquadMember? fromJson(Object? value) {
    if (value is! Map) return null;
    final identityKey = value['identityKey'];
    if (identityKey is! String || identityKey.trim().isEmpty) return null;
    final rawLabel = value['label'];
    final label = rawLabel is String && rawLabel.trim().isNotEmpty
        ? rawLabel.trim()
        : '飞机';
    return SquadMember(identityKey: identityKey.trim(), label: label);
  }

  @override
  bool operator ==(Object other) =>
      other is SquadMember &&
      identityKey == other.identityKey &&
      label == other.label;

  @override
  int get hashCode => Object.hash(identityKey, label);
}

@immutable
class Squad {
  const Squad({
    required this.id,
    required this.name,
    this.revision = 1,
    this.members = const [],
  });

  final String id;
  final String name;

  /// 成员变更时 +1；执行快照用它判断小队是否在确认后被改过。
  final int revision;
  final List<SquadMember> members;

  Map<String, Object?> toJson() => {
    'id': id,
    'name': name,
    'revision': revision,
    'members': [for (final member in members) member.toJson()],
  };

  static Squad? fromJson(Object? value) {
    if (value is! Map) return null;
    final id = value['id'];
    final name = value['name'];
    if (id is! String || id.trim().isEmpty) return null;
    if (name is! String || name.trim().isEmpty) return null;
    final members = <SquadMember>[];
    final seen = <String>{};
    final rawMembers = value['members'];
    if (rawMembers is List) {
      for (final item in rawMembers) {
        final member = SquadMember.fromJson(item);
        if (member == null) continue;
        if (!seen.add(member.identityKey)) continue;
        members.add(member);
      }
    }
    final rawRevision = value['revision'];
    final revision = rawRevision is num && rawRevision.toInt() > 0
        ? rawRevision.toInt()
        : 1;
    return Squad(
      id: id.trim(),
      name: name.trim(),
      revision: revision,
      members: List.unmodifiable(members),
    );
  }
}

enum SquadMemberState { online, offline, conflict }

@immutable
class SquadMemberStatus {
  const SquadMemberStatus({
    required this.member,
    required this.state,
    this.vehicle,
    this.confirmed = true,
  });

  final SquadMember member;
  final SquadMemberState state;

  /// 认领到的当前会话；离线或冲突时为 null。
  final FleetVehicleSummary? vehicle;

  /// 临时身份（端点 + sysid）在本次运行中是否已被人工确认。
  final bool confirmed;

  bool get temporary => member.temporary;

  /// 临时身份认领到了飞机，但用户还没确认：不能参与整队执行。
  bool get needsConfirmation =>
      state == SquadMemberState.online && temporary && !confirmed;
}

/// 一个小队在当前连接快照下的认领结果。
@immutable
class SquadClaim {
  SquadClaim({required this.squad, required Iterable<SquadMemberStatus> members})
    : members = List.unmodifiable(members);

  final Squad squad;
  final List<SquadMemberStatus> members;

  int get memberCount => members.length;
  int get onlineCount =>
      members.where((item) => item.state == SquadMemberState.online).length;
  int get pendingCount =>
      members.where((item) => item.needsConfirmation).length;
  bool get hasConflict =>
      members.any((item) => item.state == SquadMemberState.conflict);
  bool get allOnline =>
      members.isNotEmpty &&
      onlineCount == members.length &&
      pendingCount == 0;

  /// 当前可参与执行的车辆 ID（保持小队成员顺序），未确认的临时成员不算。
  List<int> get vehicleIds => [
    for (final item in members)
      if (item.state == SquadMemberState.online &&
          item.vehicle != null &&
          !item.needsConfirmation)
        item.vehicle!.id,
  ];

  String get summary {
    if (members.isEmpty) return '没有成员';
    final text = '$onlineCount/$memberCount 在线';
    if (pendingCount > 0) return '$text · $pendingCount 待确认';
    return hasConflict ? '$text · 身份冲突' : text;
  }
}

/// 按当前 fleet 快照把小队成员认领回车辆。
///
/// 同一身份出现两架飞机时标记冲突（克隆配置/仿真），绝不替用户挑一架。
/// [confirmedTemporary] 是本次运行中被人工确认过的临时身份。
SquadClaim resolveSquadClaim(
  Squad squad,
  Iterable<FleetVehicleSummary> vehicles, {
  Set<String> confirmedTemporary = const {},
}) {
  final byIdentity = <String, List<FleetVehicleSummary>>{};
  for (final vehicle in vehicles) {
    byIdentity.putIfAbsent(squadIdentityKey(vehicle), () => []).add(vehicle);
  }
  final statuses = <SquadMemberStatus>[];
  for (final member in squad.members) {
    final matches = byIdentity[member.identityKey] ?? const [];
    if (matches.length == 1) {
      statuses.add(
        SquadMemberStatus(
          member: member,
          state: SquadMemberState.online,
          vehicle: matches.single,
          confirmed:
              !member.temporary ||
              confirmedTemporary.contains(member.identityKey),
        ),
      );
    } else if (matches.isEmpty) {
      statuses.add(
        SquadMemberStatus(member: member, state: SquadMemberState.offline),
      );
    } else {
      statuses.add(
        SquadMemberStatus(member: member, state: SquadMemberState.conflict),
      );
    }
  }
  return SquadClaim(squad: squad, members: statuses);
}

enum SquadReadiness { ready, warning, blocked }

@immutable
class SquadMemberReadiness {
  const SquadMemberReadiness({
    required this.member,
    required this.status,
    this.blockers = const [],
    this.warnings = const [],
  });

  final SquadMemberStatus member;
  final SquadReadiness status;

  /// 不能飞的原因（未连接、GPS 未定位、临时身份待确认……）。
  final List<String> blockers;

  /// 可以飞但值得注意的原因（电量偏低、Home 未设置……）。
  final List<String> warnings;

  String get detail {
    final reasons = [...blockers, ...warnings];
    return reasons.isEmpty ? '就绪' : reasons.join('、');
  }
}

/// 起飞前的整队就绪快照。只读，不向飞机发任何命令。
@immutable
class SquadReadinessReport {
  SquadReadinessReport({
    required this.claim,
    required Iterable<SquadMemberReadiness> members,
  }) : members = List.unmodifiable(members);

  final SquadClaim claim;
  final List<SquadMemberReadiness> members;

  int get readyCount =>
      members.where((item) => item.status == SquadReadiness.ready).length;
  int get warningCount =>
      members.where((item) => item.status == SquadReadiness.warning).length;
  int get blockedCount =>
      members.where((item) => item.status == SquadReadiness.blocked).length;

  SquadReadiness get overall => blockedCount > 0
      ? SquadReadiness.blocked
      : warningCount > 0
      ? SquadReadiness.warning
      : SquadReadiness.ready;

  String get summary =>
      '$readyCount/${members.length} 就绪'
      '${warningCount > 0 ? ' · $warningCount 注意' : ''}'
      '${blockedCount > 0 ? ' · $blockedCount 不可用' : ''}';
}

SquadReadinessReport evaluateSquadReadiness(SquadClaim claim) {
  final entries = <SquadMemberReadiness>[];
  for (final status in claim.members) {
    final blockers = <String>[];
    final warnings = <String>[];
    switch (status.state) {
      case SquadMemberState.offline:
        blockers.add('未连接');
      case SquadMemberState.conflict:
        blockers.add('身份冲突');
      case SquadMemberState.online:
        if (status.needsConfirmation) blockers.add('临时身份待确认');
        final vehicle = status.vehicle!;
        if (!vehicle.stateFresh) blockers.add('遥测过期');
        if (!vehicle.positionValid) blockers.add('没有位置');
        if ((vehicle.gpsFixType ?? 0) < 3) blockers.add('GPS 未定位');
        final battery = vehicle.batteryRemainingPercent;
        if (battery == null) {
          warnings.add('电量未知');
        } else if (battery <= 20) {
          warnings.add('电量 $battery%');
        } else if (battery <= 35) {
          warnings.add('电量偏低 $battery%');
        }
        if (vehicle.satellitesVisible != null &&
            vehicle.satellitesVisible! < 8) {
          warnings.add('卫星 ${vehicle.satellitesVisible} 颗');
        }
        if (!vehicle.homePositionValid) warnings.add('Home 未设置');
        if (!(vehicle.missionValid && vehicle.missionTotal > 0)) {
          warnings.add('未回读到任务');
        }
    }
    entries.add(
      SquadMemberReadiness(
        member: status,
        status: blockers.isNotEmpty
            ? SquadReadiness.blocked
            : warnings.isNotEmpty
            ? SquadReadiness.warning
            : SquadReadiness.ready,
        blockers: List.unmodifiable(blockers),
        warnings: List.unmodifiable(warnings),
      ),
    );
  }
  return SquadReadinessReport(claim: claim, members: entries);
}

/// 整队执行前的硬性拦截。空列表表示可以下发。
///
/// 身份冲突、没有可执行成员、启动任务却有人没回读到任务时拒绝整队执行，
/// 不做"部分派发"。
List<String> squadExecutionBlockers(
  SquadClaim claim,
  FleetBatchAction action,
) {
  final reasons = <String>[];
  if (claim.hasConflict) reasons.add('小队内有身份冲突');
  final executable = [
    for (final status in claim.members)
      if (status.state == SquadMemberState.online &&
          !status.needsConfirmation &&
          status.vehicle != null)
        status.vehicle!,
  ];
  if (executable.isEmpty) reasons.add('没有可执行的成员');
  if (action == FleetBatchAction.startMission) {
    final missing = executable
        .where((vehicle) => !(vehicle.missionValid && vehicle.missionTotal > 0))
        .length;
    if (missing > 0) reasons.add('$missing 架未回读到任务');
  }
  return reasons;
}
