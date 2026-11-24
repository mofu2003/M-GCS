import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/fleet/fleet_controller.dart';
import 'package:mgcs_frontend/fleet/fleet_models.dart';
import 'package:mgcs_frontend/fleet/squad_controller.dart';
import 'package:mgcs_frontend/fleet/squad_models.dart';
import 'package:mgcs_frontend/fleet/squad_store.dart';

import 'fleet_controller_test.dart' show MemoryFleetBackend;

class _MemorySquadStore implements SquadStore {
  List<Squad> saved = const [];
  bool failSave = false;

  @override
  Future<List<Squad>> load() async => saved;

  @override
  Future<void> save(List<Squad> squads) async {
    if (failSave) throw StateError('磁盘写入失败');
    saved = List.unmodifiable(squads);
  }
}

FleetVehicleSummary _vehicle({
  required int id,
  required int systemId,
  String endpoint = 'udp://127.0.0.1:14550',
  String hardwareUid = '',
  bool connected = true,
}) => FleetVehicleSummary(
  id: id,
  systemId: systemId,
  endpoint: endpoint,
  connected: connected,
  hardwareUid: hardwareUid,
);

void main() {
  group('身份', () {
    test('硬件 UID 优先，缺失时退回端点 + sysid', () {
      expect(
        squadIdentityKey(_vehicle(id: 1, systemId: 3, hardwareUid: 'uid2:ab')),
        'uid2:ab',
      );
      expect(
        squadIdentityKey(
          _vehicle(
            id: 2,
            systemId: 3,
            endpoint: 'udp://127.0.0.1:14550',
          ),
        ),
        'ep:udp://127.0.0.1:14550|sys:3',
      );
      expect(squadIdentityIsTemporary('uid2:ab'), isFalse);
      expect(squadIdentityIsTemporary('uid:0011'), isFalse);
      expect(squadIdentityIsTemporary('ep:udp://x|sys:1'), isTrue);
    });
  });

  group('认领', () {
    test('在线、离线、临时身份与冲突', () {
      final hardware = _vehicle(id: 1, systemId: 1, hardwareUid: 'uid2:aa');
      final temporary = _vehicle(id: 2, systemId: 2);
      final squad = Squad(
        id: 's1',
        name: '一队',
        members: [
          const SquadMember(identityKey: 'uid2:aa', label: '飞机 1'),
          SquadMember(
            identityKey: squadIdentityKey(temporary),
            label: '飞机 2',
          ),
          const SquadMember(identityKey: 'uid2:missing', label: '飞机 9'),
        ],
      );
      final claim = resolveSquadClaim(squad, [hardware, temporary]);
      expect(claim.onlineCount, 2);
      expect(claim.memberCount, 3);
      expect(claim.hasConflict, isFalse);
      // 临时身份认领到了，但在人工确认前不参与整队执行。
      expect(claim.pendingCount, 1);
      expect(claim.vehicleIds, [1]);
      expect(claim.members[1].temporary, isTrue);
      expect(claim.members[1].needsConfirmation, isTrue);
      expect(claim.members[2].state, SquadMemberState.offline);

      final confirmed = resolveSquadClaim(
        squad,
        [hardware, temporary],
        confirmedTemporary: {squadIdentityKey(temporary)},
      );
      expect(confirmed.pendingCount, 0);
      expect(confirmed.vehicleIds, [1, 2]);
    });

    test('同一硬件身份出现在两架飞机上时标记冲突', () {
      final squad = Squad(
        id: 's1',
        name: '一队',
        members: const [
          SquadMember(identityKey: 'uid2:dup', label: '飞机 1'),
        ],
      );
      final claim = resolveSquadClaim(squad, [
        _vehicle(id: 1, systemId: 1, hardwareUid: 'uid2:dup'),
        _vehicle(
          id: 2,
          systemId: 2,
          endpoint: 'udp://127.0.0.1:14551',
          hardwareUid: 'uid2:dup',
        ),
      ]);
      expect(claim.members.single.state, SquadMemberState.conflict);
      expect(claim.hasConflict, isTrue);
      expect(claim.vehicleIds, isEmpty);
    });
  });

  group('就绪检查', () {
    Squad claimedFor(List<FleetVehicleSummary> vehicles) {
      final squad = Squad(
        id: 's1',
        name: '一队',
        members: [
          for (final vehicle in vehicles)
            SquadMember(
              identityKey: squadIdentityKey(vehicle),
              label: vehicle.displayName,
            ),
        ],
      );
      return squad;
    }

    test('健康成员显示就绪，缺定位为不可用，低电量为注意', () {
      final healthy = FleetVehicleSummary(
        id: 1,
        systemId: 1,
        endpoint: 'udp://127.0.0.1:14550',
        connected: true,
        hardwareUid: 'uid2:aa',
        positionValid: true,
        gpsFixType: 3,
        satellitesVisible: 12,
        batteryRemainingPercent: 80,
        homePositionValid: true,
        missionValid: true,
        missionTotal: 3,
      );
      final lowBattery = FleetVehicleSummary(
        id: 2,
        systemId: 2,
        endpoint: 'udp://127.0.0.1:14551',
        connected: true,
        hardwareUid: 'uid2:bb',
        positionValid: true,
        gpsFixType: 3,
        satellitesVisible: 12,
        batteryRemainingPercent: 15,
        homePositionValid: true,
        missionValid: true,
        missionTotal: 3,
      );
      final noFix = FleetVehicleSummary(
        id: 3,
        systemId: 3,
        endpoint: 'udp://127.0.0.1:14552',
        connected: true,
        hardwareUid: 'uid2:cc',
        positionValid: false,
        gpsFixType: 1,
        satellitesVisible: 4,
      );

      final claim = resolveSquadClaim(
        claimedFor([healthy, lowBattery, noFix]),
        [healthy, lowBattery, noFix],
      );
      final report = evaluateSquadReadiness(claim);
      expect(report.readyCount, 1);
      expect(report.warningCount, 1);
      expect(report.blockedCount, 1);
      expect(report.members[0].status, SquadReadiness.ready);
      expect(report.members[1].status, SquadReadiness.warning);
      expect(report.members[1].warnings.join(), contains('电量 15%'));
      expect(report.members[2].status, SquadReadiness.blocked);
      expect(report.members[2].blockers, contains('没有位置'));
      expect(report.members[2].blockers, contains('GPS 未定位'));
    });

    test('临时身份未确认时不可用，离线成员标记未连接', () {
      final temporary = _vehicle(id: 1, systemId: 1);
      final claim = resolveSquadClaim(claimedFor([temporary]), [temporary]);
      final report = evaluateSquadReadiness(claim);
      expect(report.members.single.blockers, contains('临时身份待确认'));

      final offline = evaluateSquadReadiness(
        resolveSquadClaim(claimedFor([temporary]), const []),
      );
      expect(offline.members.single.blockers, contains('未连接'));
    });

    test('整队执行拦截：身份冲突、没有可执行成员、启动任务缺任务', () {
      final a = FleetVehicleSummary(
        id: 1,
        systemId: 1,
        endpoint: 'udp://127.0.0.1:14550',
        connected: true,
        hardwareUid: 'uid2:aa',
        missionValid: true,
        missionTotal: 3,
      );
      final b = FleetVehicleSummary(
        id: 2,
        systemId: 2,
        endpoint: 'udp://127.0.0.1:14551',
        connected: true,
        hardwareUid: 'uid2:bb',
      );
      final claim = resolveSquadClaim(claimedFor([a, b]), [a, b]);
      expect(
        squadExecutionBlockers(claim, FleetBatchAction.returnToLaunch),
        isEmpty,
      );
      expect(
        squadExecutionBlockers(claim, FleetBatchAction.startMission).join(),
        contains('1 架未回读到任务'),
      );

      // 两架报同一个 UID：冲突时禁止整队执行。
      final clone = FleetVehicleSummary(
        id: 2,
        systemId: 2,
        endpoint: 'udp://127.0.0.1:14551',
        connected: true,
        hardwareUid: 'uid2:aa',
      );
      final conflicted = resolveSquadClaim(claimedFor([a, clone]), [a, clone]);
      expect(
        squadExecutionBlockers(
          conflicted,
          FleetBatchAction.returnToLaunch,
        ).join(),
        contains('身份冲突'),
      );

      // 全部离线。
      final offline = resolveSquadClaim(claimedFor([a]), const []);
      expect(
        squadExecutionBlockers(
          offline,
          FleetBatchAction.returnToLaunch,
        ).join(),
        contains('没有可执行的成员'),
      );
    });
  });

  group('存储', () {
    late Directory dir;
    setUp(() async {
      dir = await Directory.systemTemp.createTemp('mgcs-squad-test');
    });
    tearDown(() async {
      if (await dir.exists()) await dir.delete(recursive: true);
    });

    test('保存后能原样读回', () async {
      final store = FileSquadStore(directoryProvider: () async => dir);
      const squad = Squad(
        id: 's1',
        name: '一队',
        revision: 3,
        members: [
          SquadMember(identityKey: 'uid2:aa', label: '飞机 1'),
          SquadMember(identityKey: 'ep:udp://x|sys:2', label: '飞机 2'),
        ],
      );
      await store.save([squad]);
      final loaded = await store.load();
      expect(loaded, hasLength(1));
      expect(loaded.single.id, 's1');
      expect(loaded.single.name, '一队');
      expect(loaded.single.revision, 3);
      expect(loaded.single.members.map((m) => m.identityKey), [
        'uid2:aa',
        'ep:udp://x|sys:2',
      ]);
      expect(loaded.single.members.last.temporary, isTrue);
    });

    test('损坏或不认识的版本回退空列表且不覆盖原文件', () async {
      final file = File('${dir.path}/squads.json');
      await file.writeAsString('{"schema":1,"squads":[');
      final store = FileSquadStore(directoryProvider: () async => dir);
      expect(await store.load(), isEmpty);
      expect(await file.readAsString(), '{"schema":1,"squads":[');

      await file.writeAsString(
        jsonEncode({'schema': 99, 'squads': []}),
      );
      expect(await store.load(), isEmpty);
      expect(await file.readAsString(), contains('"schema":99'));
    });
  });

  group('控制器', () {
    test('从当前选择创建小队并落盘，重命名/删除保留 revision', () async {
      final backend = MemoryFleetBackend();
      final store = _MemorySquadStore();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final first = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14550'),
      );
      final second = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14551'),
      );
      backend.vehicles[first] = FleetVehicleSummary(
        id: first,
        systemId: 1,
        endpoint: 'udp://127.0.0.1:14550',
        connected: true,
        hardwareUid: 'uid2:aa',
      );
      backend.vehicles[second] = FleetVehicleSummary(
        id: second,
        systemId: 2,
        endpoint: 'udp://127.0.0.1:14551',
        connected: true,
        hardwareUid: 'uid2:bb',
      );
      final squads = SquadController(fleet, store: store);
      addTearDown(squads.dispose);
      await squads.load();
      expect(squads.squads, isEmpty);

      fleet.selectAllVehicles();
      final squad = await squads.createFromSelection('一队');
      expect(squad.members.map((m) => m.identityKey), ['uid2:aa', 'uid2:bb']);
      expect(squads.squads, hasLength(1));
      expect(store.saved, hasLength(1));

      await squads.rename(squad.id, '二队');
      expect(squads.squads.single.name, '二队');
      expect(squads.squads.single.revision, 2);

      await squads.delete(squad.id);
      expect(squads.squads, isEmpty);
      expect(store.saved, isEmpty);
    });

    test('可追加当前选择中的新成员，也能移除成员', () async {
      final backend = MemoryFleetBackend();
      final store = _MemorySquadStore();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final first = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14550'),
      );
      final second = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14551'),
      );
      final third = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14552'),
      );
      for (final (index, id) in [first, second, third].indexed) {
        backend.vehicles[id] = FleetVehicleSummary(
          id: id,
          systemId: index + 1,
          endpoint: 'udp://127.0.0.1:${14550 + index}',
          connected: true,
          hardwareUid: 'uid2:0$index',
        );
      }
      fleet.refresh();
      final squads = SquadController(fleet, store: store);
      addTearDown(squads.dispose);
      await squads.load();

      fleet.selectVehicles([first, second]);
      final squad = await squads.createFromSelection('一队');
      expect(squad.members.map((member) => member.identityKey), [
        'uid2:00',
        'uid2:01',
      ]);

      fleet.selectVehicles([second, third]);
      await squads.addSelectedMembers(squad.id);
      expect(squads.squads.single.members.map((member) => member.identityKey), [
        'uid2:00',
        'uid2:01',
        'uid2:02',
      ]);
      expect(squads.squads.single.revision, 2);

      await squads.removeMember(squad.id, 'uid2:01');
      expect(squads.squads.single.members.map((member) => member.identityKey), [
        'uid2:00',
        'uid2:02',
      ]);
      expect(squads.squads.single.revision, 3);
      expect(store.saved.single.revision, 3);

      fleet.selectVehicles([third]);
      await expectLater(
        squads.addSelectedMembers(squad.id),
        throwsA(isA<StateError>()),
      );
      expect(squads.squads.single.revision, 3);
    });

    test('空小队不会被误判为就绪', () {
      final empty = Squad(id: 'empty', name: '空队');
      final report = evaluateSquadReadiness(resolveSquadClaim(empty, const []));
      expect(report.overall, SquadReadiness.blocked);
      expect(report.summary, '没有成员');
    });

    test('写盘失败时不改变已加载的小队', () async {
      final backend = MemoryFleetBackend();
      final store = _MemorySquadStore();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14550'));
      fleet.selectAllVehicles();
      final squads = SquadController(fleet, store: store);
      addTearDown(squads.dispose);
      await squads.load();
      store.failSave = true;
      await expectLater(
        squads.createFromSelection('一队'),
        throwsStateError,
      );
      expect(squads.squads, isEmpty);
    });

    test('相同身份的飞机不能放进同一个小队', () async {
      final backend = MemoryFleetBackend();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final first = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14550'),
      );
      final second = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14551'),
      );
      // 克隆配置：两架飞机报同一个 UID。
      for (final id in [first, second]) {
        backend.vehicles[id] = FleetVehicleSummary(
          id: id,
          systemId: id,
          endpoint: 'udp://127.0.0.1:${14549 + id}',
          connected: true,
          hardwareUid: 'uid2:dup',
        );
      }
      fleet.selectAllVehicles();
      final squads = SquadController(fleet, store: _MemorySquadStore());
      addTearDown(squads.dispose);
      await squads.load();
      await expectLater(
        squads.createFromSelection('一队'),
        throwsA(isA<StateError>()),
      );
    });

    test('临时身份需要人工确认后才能整队选中', () async {
      final backend = MemoryFleetBackend();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final first = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14550'),
      );
      final second = fleet.openLink(
        FleetConnectionSpec('udp://127.0.0.1:14551'),
      );
      fleet.selectAllVehicles();
      final squads = SquadController(fleet, store: _MemorySquadStore());
      addTearDown(squads.dispose);
      await squads.load();
      final squad = await squads.createFromSelection('一队');

      // 仿真机没有硬件 UID：认领到了也要等人工确认。
      var claim = squads.claimFor(squad);
      expect(claim.pendingCount, 2);
      expect(claim.vehicleIds, isEmpty);

      squads.confirmTemporaryMembers(squad);
      claim = squads.claimFor(squad);
      expect(claim.pendingCount, 0);
      expect(claim.vehicleIds, [first, second]);
    });

    test('断连后成员显示离线，重连后按硬件 UID 认回', () async {
      final backend = MemoryFleetBackend();
      final store = _MemorySquadStore();
      final fleet = FleetController(backend);
      addTearDown(fleet.dispose);
      final id = fleet.openLink(FleetConnectionSpec('udp://127.0.0.1:14550'));
      backend.vehicles[id] = FleetVehicleSummary(
        id: id,
        systemId: 1,
        endpoint: 'udp://127.0.0.1:14550',
        connected: true,
        hardwareUid: 'uid2:aa',
      );
      fleet.selectAllVehicles();
      final squads = SquadController(fleet, store: store);
      addTearDown(squads.dispose);
      await squads.load();
      final squad = await squads.createFromSelection('一队');
      expect(squads.claimFor(squad).onlineCount, 1);

      fleet.clearVehicleSelection();
      backend.vehicles.remove(id);
      fleet.refresh();
      expect(squads.claimFor(squad).onlineCount, 0);
      expect(squads.claimFor(squad).members.single.state, SquadMemberState.offline);
    });
  });
}
