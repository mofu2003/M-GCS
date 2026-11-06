import 'package:flutter/foundation.dart';

/// MV1 的连接配置。一个独立端点对应一个 native session；不是 MAVLink sysid。
/// 同端点多 sysid 的共享链路模型属于 MV2，不能用重复打开 socket 来替代。
@immutable
class FleetConnectionSpec {
  FleetConnectionSpec(String uri) : uri = _validate(uri);

  final String uri;

  /// Resource identity excludes serial baud rate; the device remains exclusive.
  String get resourceKey {
    final parsed = Uri.parse(uri);
    if (parsed.scheme == 'serial') {
      final path = parsed.path.isEmpty
          ? parsed.host.toUpperCase()
          : parsed.path;
      return 'serial:$path';
    }
    return '${parsed.scheme}:${parsed.host.toLowerCase()}:${parsed.port}';
  }

  static String _validate(String value) {
    final uri = Uri.tryParse(value.trim());
    if (uri == null || !const {'serial', 'udp', 'tcp'}.contains(uri.scheme)) {
      throw ArgumentError('连接地址必须使用 serial、udp 或 tcp');
    }
    if (uri.scheme == 'serial') {
      if (uri.host.isEmpty && (uri.path.isEmpty || uri.path == '/')) {
        throw ArgumentError('串口设备路径不能为空');
      }
      final baud = uri.queryParameters['baud'];
      if (baud != null && (int.tryParse(baud) ?? 0) <= 0) {
        throw ArgumentError('波特率无效');
      }
    } else if (uri.host.isEmpty ||
        !uri.hasPort ||
        uri.port <= 0 ||
        uri.port > 65535) {
      throw ArgumentError('网络连接必须指定有效的地址和端口');
    }
    // Native serial endpoint syntax is serial:///absolute/path, not a file URI.
    // Preserve its spelling rather than Uri's file-style serial:/path rendering.
    return value.trim();
  }
}

/// MV6 批量飞行动作。目标车辆由 [FleetBatchTarget] 显式携带，不能回退到
/// 当前 active vehicle，避免确认后切换焦点导致指令误发。
enum FleetBatchAction { arm, disarm, returnToLaunch, startMission }

extension FleetBatchActionMetadata on FleetBatchAction {
  String get label => switch (this) {
    FleetBatchAction.arm => '批量解锁',
    FleetBatchAction.disarm => '批量上锁',
    FleetBatchAction.returnToLaunch => '批量返航',
    FleetBatchAction.startMission => '批量开始任务',
  };
}

enum FleetBatchCommandStatus { success, rejected, timeout, error }

extension FleetBatchCommandStatusMetadata on FleetBatchCommandStatus {
  String get label => switch (this) {
    FleetBatchCommandStatus.success => '成功',
    FleetBatchCommandStatus.rejected => '被拒绝',
    FleetBatchCommandStatus.timeout => '超时',
    FleetBatchCommandStatus.error => '失败',
  };
}

/// 一架飞机在一次批量指令中的结果。执行器必须保留每架飞机的独立结果，
/// 不能把部分成功折叠成整体成功。
@immutable
class FleetBatchCommandResult {
  const FleetBatchCommandResult({
    required this.vehicleId,
    required this.systemId,
    required this.status,
    required this.message,
  });

  const FleetBatchCommandResult.success({
    required this.vehicleId,
    required this.systemId,
    this.message = '指令已接受',
  }) : status = FleetBatchCommandStatus.success;

  const FleetBatchCommandResult.failure({
    required this.vehicleId,
    required this.systemId,
    required this.status,
    required this.message,
  });

  final int vehicleId;
  final int systemId;
  final FleetBatchCommandStatus status;
  final String message;

  bool get success => status == FleetBatchCommandStatus.success;
}

/// 批量确认时冻结的目标集合。确认条显示的系统 ID 与真正执行的目标来自同一
/// 快照，执行期间即使 selection 改变也不会把指令发给另一架飞机。
@immutable
class FleetBatchTarget {
  FleetBatchTarget({
    required this.action,
    required Iterable<int> vehicleIds,
    required Iterable<int> systemIds,
  }) : vehicleIds = List.unmodifiable(vehicleIds),
       systemIds = List.unmodifiable(systemIds);

  final FleetBatchAction action;
  final List<int> vehicleIds;
  final List<int> systemIds;

  String get detail {
    final systems = systemIds.where((id) => id > 0).toList(growable: false);
    final systemText = systems.isEmpty ? '系统 ID 未知' : '系统 ${systems.join('、')}';
    return '${vehicleIds.length} 架飞机 · $systemText';
  }
}

@immutable
class FleetBatchResult {
  FleetBatchResult({
    required this.action,
    required Iterable<FleetBatchCommandResult> results,
  }) : results = List.unmodifiable(results);

  final FleetBatchAction action;
  final List<FleetBatchCommandResult> results;

  int get successCount => results.where((result) => result.success).length;
  int get failureCount => results.length - successCount;
  bool get allSucceeded => results.isNotEmpty && failureCount == 0;
}

@immutable
class FleetMissionPoint {
  const FleetMissionPoint({
    required this.sequence,
    required this.command,
    required this.frame,
    required this.latitude,
    required this.longitude,
    required this.altitudeMeters,
    this.current = false,
  });

  final int sequence;
  final int command;
  final int frame;
  final double latitude;
  final double longitude;
  final double altitudeMeters;
  final bool current;

  @override
  bool operator ==(Object other) =>
      other is FleetMissionPoint &&
      sequence == other.sequence &&
      command == other.command &&
      frame == other.frame &&
      latitude == other.latitude &&
      longitude == other.longitude &&
      altitudeMeters == other.altitudeMeters &&
      current == other.current;

  @override
  int get hashCode => Object.hash(
    sequence,
    command,
    frame,
    latitude,
    longitude,
    altitudeMeters,
    current,
  );
}

@immutable
class FleetVehicleSummary {
  const FleetVehicleSummary({
    required this.id,
    required this.systemId,
    required this.endpoint,
    required this.connected,
    this.name = '',
    this.firmwareType = '',
    this.vehicleType = '',
    this.state = 'waiting_heartbeat',
    this.armed = false,
    this.heartbeatFresh = true,
    this.flightMode = '',
    this.batteryRemainingPercent,
    this.gpsFixType,
    this.satellitesVisible,
    this.positionValid = false,
    this.latitude = 0,
    this.longitude = 0,
    this.altitudeMeters,
    this.relativeAltitudeMeters,
    this.headingDegrees,
    this.groundSpeedMps,
    this.homePositionValid = false,
    this.homeLatitude = 0,
    this.homeLongitude = 0,
    this.homeAltitudeMeters,
    this.missionValid = false,
    this.missionTotal = 0,
    this.missionCurrentIndex = 0,
    this.linkCount = 0,
    this.healthyLinkCount = 0,
    this.communicationLost = false,
    this.primaryHeartbeatAgeMs = 0,
    this.missionRevision = 0,
    this.missionPoints = const [],
  });

  final int id;
  final int systemId;
  final String endpoint;
  final String name;
  final String firmwareType;
  final String vehicleType;
  final bool connected;
  final String state;
  final bool armed;
  final bool heartbeatFresh;
  final String flightMode;
  final int? batteryRemainingPercent;
  final int? gpsFixType;
  final int? satellitesVisible;
  final bool positionValid;
  final double latitude;
  final double longitude;

  /// 海拔高度（MSL），用于地理位置。
  final double? altitudeMeters;

  /// 飞控上报的相对 Home 高度。
  final double? relativeAltitudeMeters;
  final double? headingDegrees;
  final double? groundSpeedMps;
  final bool homePositionValid;
  final double homeLatitude;
  final double homeLongitude;
  final double? homeAltitudeMeters;
  final bool missionValid;
  final int missionTotal;
  final int missionCurrentIndex;
  final int linkCount;
  final int healthyLinkCount;
  final bool communicationLost;
  final int primaryHeartbeatAgeMs;
  final int missionRevision;
  final List<FleetMissionPoint> missionPoints;

  bool get identified => systemId > 0;
  bool get stateFresh => connected && heartbeatFresh;

  @override
  bool operator ==(Object other) =>
      other is FleetVehicleSummary &&
      id == other.id &&
      systemId == other.systemId &&
      endpoint == other.endpoint &&
      name == other.name &&
      firmwareType == other.firmwareType &&
      vehicleType == other.vehicleType &&
      connected == other.connected &&
      state == other.state &&
      armed == other.armed &&
      heartbeatFresh == other.heartbeatFresh &&
      flightMode == other.flightMode &&
      batteryRemainingPercent == other.batteryRemainingPercent &&
      gpsFixType == other.gpsFixType &&
      satellitesVisible == other.satellitesVisible &&
      positionValid == other.positionValid &&
      latitude == other.latitude &&
      longitude == other.longitude &&
      altitudeMeters == other.altitudeMeters &&
      relativeAltitudeMeters == other.relativeAltitudeMeters &&
      headingDegrees == other.headingDegrees &&
      groundSpeedMps == other.groundSpeedMps &&
      homePositionValid == other.homePositionValid &&
      homeLatitude == other.homeLatitude &&
      homeLongitude == other.homeLongitude &&
      homeAltitudeMeters == other.homeAltitudeMeters &&
      missionValid == other.missionValid &&
      missionTotal == other.missionTotal &&
      missionCurrentIndex == other.missionCurrentIndex &&
      linkCount == other.linkCount &&
      healthyLinkCount == other.healthyLinkCount &&
      communicationLost == other.communicationLost &&
      primaryHeartbeatAgeMs == other.primaryHeartbeatAgeMs &&
      missionRevision == other.missionRevision &&
      listEquals(missionPoints, other.missionPoints);

  @override
  int get hashCode => Object.hashAll([
    id,
    systemId,
    endpoint,
    name,
    firmwareType,
    vehicleType,
    connected,
    state,
    armed,
    heartbeatFresh,
    flightMode,
    batteryRemainingPercent,
    gpsFixType,
    satellitesVisible,
    positionValid,
    latitude,
    longitude,
    altitudeMeters,
    relativeAltitudeMeters,
    headingDegrees,
    groundSpeedMps,
    homePositionValid,
    homeLatitude,
    homeLongitude,
    homeAltitudeMeters,
    missionValid,
    missionTotal,
    missionCurrentIndex,
    linkCount,
    healthyLinkCount,
    communicationLost,
    primaryHeartbeatAgeMs,
    missionRevision,
    Object.hashAll(missionPoints),
  ]);
}

/// 不可变 fleet 快照；UI 焦点与链路列表不是同一个概念。
@immutable
class FleetSnapshot {
  FleetSnapshot({
    required Iterable<FleetVehicleSummary> vehicles,
    Iterable<FleetLinkSummary> links = const [],
    required this.activeVehicleId,
  }) : vehicles = List.unmodifiable(vehicles),
       links = List.unmodifiable(links);

  final List<FleetVehicleSummary> vehicles;
  final List<FleetLinkSummary> links;
  final int? activeVehicleId;

  FleetVehicleSummary? vehicle(int id) {
    for (final vehicle in vehicles) {
      if (vehicle.id == id) return vehicle;
    }
    return null;
  }

  FleetVehicleSummary? get activeVehicle =>
      activeVehicleId == null ? null : vehicle(activeVehicleId!);

  FleetLinkSummary? link(int id) {
    for (final link in links) {
      if (link.id == id) return link;
    }
    return null;
  }
}

/// 一条物理链路和其中一个已发现 Vehicle route 的摘要。
@immutable
class FleetLinkSummary {
  const FleetLinkSummary({
    required this.id,
    required this.routeId,
    required this.endpoint,
    required this.connected,
    this.vehicleId,
    this.systemId = 0,
    this.state = 'waiting_heartbeat',
    this.activeFocus = false,
    this.primary = false,
  });

  final int id;
  final int routeId;
  final int? vehicleId;
  final String endpoint;
  final int systemId;
  final bool connected;
  final String state;
  final bool activeFocus;
  final bool primary;

  String get rowKey => '$id:$routeId';

  @override
  bool operator ==(Object other) =>
      other is FleetLinkSummary &&
      id == other.id &&
      routeId == other.routeId &&
      vehicleId == other.vehicleId &&
      endpoint == other.endpoint &&
      systemId == other.systemId &&
      connected == other.connected &&
      state == other.state &&
      activeFocus == other.activeFocus &&
      primary == other.primary;

  @override
  int get hashCode => Object.hash(
    id,
    routeId,
    vehicleId,
    endpoint,
    systemId,
    connected,
    state,
    activeFocus,
    primary,
  );
}

/// 待确认/异步命令的目标凭证。A -> B -> A 后，旧 A 凭证也失效。
@immutable
class FleetTarget {
  const FleetTarget({required this.vehicleId, required this.focusRevision});

  final int vehicleId;
  final int focusRevision;
}
