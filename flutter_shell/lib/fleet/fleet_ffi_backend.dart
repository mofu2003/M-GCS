import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi_response.dart';
import 'fleet_backend.dart';
import 'fleet_models.dart';

/// 复用现有 C ABI；所有列表/状态读取与关闭都不依赖 active focus。
/// 支持独立 endpoint 与 UDP 共享 endpoint；车辆摘要同时携带当前 active
/// 之外的 session 位置快照，供多机地图使用。
final class FleetFfiBackend implements FleetBackend {
  FleetFfiBackend(DynamicLibrary library, this._core) {
    _free = library
        .lookupFunction<
          Void Function(Pointer<Utf8>),
          void Function(Pointer<Utf8>)
        >('mg_free_string');
    _open = library
        .lookupFunction<
          Int32 Function(Pointer<Void>, Pointer<Utf8>),
          int Function(Pointer<Void>, Pointer<Utf8>)
        >('mg_connection_open');
    _close = _withId(library, 'mg_connection_close');
    _closeRoute = _withId(library, 'mg_connection_close_route');
    _select = _withId(library, 'mg_vehicle_select');
    _sessionStatus = _withId(library, 'mg_vehicle_get_session_status');
    _sessionMission = _withId(library, 'mg_vehicle_get_session_mission');
    _list = library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>),
          Pointer<Utf8> Function(Pointer<Void>)
        >('mg_vehicle_list');
    _connectionList = library
        .lookupFunction<
          Pointer<Utf8> Function(Pointer<Void>),
          Pointer<Utf8> Function(Pointer<Void>)
        >('mg_connection_list');
    _active = library
        .lookupFunction<
          Int32 Function(Pointer<Void>),
          int Function(Pointer<Void>)
        >('mg_vehicle_get_active');
  }

  final Pointer<Void> _core;
  late final void Function(Pointer<Utf8>) _free;
  late final int Function(Pointer<Void>, Pointer<Utf8>) _open;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _close,
      _closeRoute,
      _select,
      _sessionStatus,
      _sessionMission;
  late final Pointer<Utf8> Function(Pointer<Void>) _list;
  late final Pointer<Utf8> Function(Pointer<Void>) _connectionList;
  late final int Function(Pointer<Void>) _active;
  final Map<int, _CachedFleetMission> _missionCache = {};

  Pointer<Utf8> Function(Pointer<Void>, int) _withId(
    DynamicLibrary library,
    String symbol,
  ) =>
      library.lookupFunction<
        Pointer<Utf8> Function(Pointer<Void>, Int32),
        Pointer<Utf8> Function(Pointer<Void>, int)
      >(symbol);

  Object? _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('多机后端返回空结果');
    try {
      return jsonDecode(pointer.toDartString());
    } finally {
      _free(pointer);
    }
  }

  void _check(Object? response) {
    if (ffiSucceeded(response)) return;
    throw StateError(ffiErrorText(response, '多机操作失败'));
  }

  @override
  int open(FleetConnectionSpec connection) {
    final uri = connection.uri.toNativeUtf8();
    try {
      final id = _open(_core, uri);
      if (id <= 0) {
        final message = switch (id) {
          -1 => '连接失败：后端核心尚未初始化。',
          -3 => '无法打开链路 ${connection.uri}。请检查设备、网络地址和端口是否可用。',
          -4 => '连接参数无效，请检查端口、地址和波特率。',
          -5 => '当前连接状态不允许执行此操作。',
          -7 => '连接资源正忙，请稍后重试。',
          -14 => '连接会话不存在或已关闭。',
          _ => '连接失败（后端错误码 $id）。',
        };
        throw StateError(message);
      }
      return id;
    } finally {
      malloc.free(uri);
    }
  }

  @override
  void close(int linkId) => _check(_read(_close(_core, linkId)));

  @override
  void closeRoute(int routeId) => _check(_read(_closeRoute(_core, routeId)));

  @override
  void select(int vehicleId) => _check(_read(_select(_core, vehicleId)));

  @override
  FleetSnapshot snapshot() {
    final rawLinks = _read(_connectionList(_core));
    if (rawLinks is! List) {
      throw const FormatException('链路列表格式无效');
    }
    final links = <FleetLinkSummary>[];
    for (final item in rawLinks) {
      if (item is! Map || item['link_id'] is! int) {
        throw const FormatException('链路列表项格式无效');
      }
      final vehicleId = (item['vehicle_id'] as num?)?.toInt() ?? 0;
      links.add(
        FleetLinkSummary(
          id: item['link_id'] as int,
          routeId: (item['route_id'] as num?)?.toInt() ?? -1,
          vehicleId: vehicleId > 0 ? vehicleId : null,
          endpoint: item['endpoint'] as String? ?? '',
          systemId: (item['system_id'] as num?)?.toInt() ?? 0,
          connected: item['connected'] == true,
          state: item['state'] as String? ?? 'waiting_heartbeat',
          activeFocus: item['active_focus'] == true,
          primary: item['primary'] == true,
        ),
      );
    }
    links.sort((a, b) => a.rowKey.compareTo(b.rowKey));

    final raw = _read(_list(_core));
    if (raw is! List) throw const FormatException('飞控列表格式无效');
    final vehicles = <FleetVehicleSummary>[];
    for (final item in raw) {
      if (item is! Map || item['vehicle_id'] is! int) {
        throw const FormatException('飞控列表项格式无效');
      }
      final id = item['vehicle_id'] as int;
      final connected = item['connected'] == true;
      final status = _read(_sessionStatus(_core, id));
      final telemetry = status is Map && status['success'] == true
          ? status['data'] as Map<Object?, Object?>?
          : null;
      final heartbeatFresh =
          item['heartbeat_fresh'] == true ||
          telemetry?['heartbeat_fresh'] == true;
      vehicles.add(
        FleetVehicleSummary(
          id: id,
          systemId: (item['system_id'] as num?)?.toInt() ?? 0,
          endpoint: item['endpoint'] as String? ?? '',
          name: item['name'] as String? ?? '',
          firmwareType: item['firmware_type'] as String? ?? '',
          vehicleType: item['vehicle_type'] as String? ?? '',
          connected: connected,
          state: connected ? 'connected' : 'waiting_heartbeat',
          armed: item['armed'] == true,
          heartbeatFresh: heartbeatFresh,
          flightMode: item['flight_mode'] as String? ?? '',
          batteryRemainingPercent:
              (telemetry?['battery_remaining_pct'] as num?)?.toInt() ??
              (item['battery_remaining_pct'] as num?)?.toInt(),
          gpsFixType:
              (telemetry?['gps_fix_type'] as num?)?.toInt() ??
              (item['gps_fix_type'] as num?)?.toInt(),
          satellitesVisible:
              (telemetry?['gps_satellites'] as num?)?.toInt() ??
              (item['gps_satellites'] as num?)?.toInt(),
          positionValid: telemetry?['position_valid'] == true,
          latitude: (telemetry?['lat_deg'] as num?)?.toDouble() ?? 0,
          longitude: (telemetry?['lon_deg'] as num?)?.toDouble() ?? 0,
          altitudeMeters: (telemetry?['alt_msl_m'] as num?)?.toDouble(),
          headingDegrees: (telemetry?['heading_deg'] as num?)?.toDouble(),
          homePositionValid: telemetry?['home_valid'] == true,
          homeLatitude: (telemetry?['home_lat_deg'] as num?)?.toDouble() ?? 0,
          homeLongitude: (telemetry?['home_lon_deg'] as num?)?.toDouble() ?? 0,
          homeAltitudeMeters: (telemetry?['home_alt_msl_m'] as num?)
              ?.toDouble(),
          missionValid: telemetry?['mission_valid'] == true,
          missionTotal: (telemetry?['mission_total'] as num?)?.toInt() ?? 0,
          missionCurrentIndex:
              (telemetry?['mission_current_index'] as num?)?.toInt() ?? 0,
          missionRevision:
              (telemetry?['mission_revision'] as num?)?.toInt() ?? 0,
          missionPoints: _missionPointsFor(
            id,
            (telemetry?['mission_revision'] as num?)?.toInt() ?? 0,
          ),
          linkCount: (item['link_count'] as num?)?.toInt() ?? 0,
          healthyLinkCount: (item['healthy_link_count'] as num?)?.toInt() ?? 0,
          communicationLost: item['communication_lost'] == true,
          primaryHeartbeatAgeMs:
              (item['primary_heartbeat_age_ms'] as num?)?.toInt() ?? 0,
        ),
      );
    }
    vehicles.sort((a, b) => a.id.compareTo(b.id));
    final liveIds = vehicles.map((vehicle) => vehicle.id).toSet();
    _missionCache.removeWhere((id, _) => !liveIds.contains(id));
    final active = _active(_core);
    return FleetSnapshot(
      vehicles: vehicles,
      links: links,
      activeVehicleId: vehicles.any((v) => v.id == active) ? active : null,
    );
  }

  List<FleetMissionPoint> _missionPointsFor(int vehicleId, int revision) {
    final cached = _missionCache[vehicleId];
    if (cached != null && cached.revision == revision) return cached.points;
    final raw = _read(_sessionMission(_core, vehicleId));
    final points = <FleetMissionPoint>[];
    if (raw is Map && raw['success'] == true && raw['items'] is List) {
      for (final item in raw['items'] as List) {
        if (item is! Map) continue;
        final latitude = (item['lat_deg'] as num?)?.toDouble();
        final longitude = (item['lon_deg'] as num?)?.toDouble();
        if (latitude == null || longitude == null) continue;
        points.add(
          FleetMissionPoint(
            sequence: (item['seq'] as num?)?.toInt() ?? 0,
            command: (item['command'] as num?)?.toInt() ?? 0,
            frame: (item['frame'] as num?)?.toInt() ?? 0,
            latitude: latitude,
            longitude: longitude,
            altitudeMeters: (item['alt_m'] as num?)?.toDouble() ?? 0,
            current: item['current'] == true,
          ),
        );
      }
    }
    final stable = List<FleetMissionPoint>.unmodifiable(points);
    _missionCache[vehicleId] = _CachedFleetMission(revision, stable);
    return stable;
  }
}

final class _CachedFleetMission {
  const _CachedFleetMission(this.revision, this.points);

  final int revision;
  final List<FleetMissionPoint> points;
}
