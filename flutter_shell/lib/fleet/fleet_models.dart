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
    this.positionValid = false,
    this.latitude = 0,
    this.longitude = 0,
    this.altitudeMeters,
    this.headingDegrees,
  });

  final int id;
  final int systemId;
  final String endpoint;
  final String name;
  final String firmwareType;
  final String vehicleType;
  final bool connected;
  final String state;
  final bool positionValid;
  final double latitude;
  final double longitude;
  final double? altitudeMeters;
  final double? headingDegrees;

  bool get identified => systemId > 0;

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
      positionValid == other.positionValid &&
      latitude == other.latitude &&
      longitude == other.longitude &&
      altitudeMeters == other.altitudeMeters &&
      headingDegrees == other.headingDegrees;

  @override
  int get hashCode => Object.hash(
    id,
    systemId,
    endpoint,
    name,
    firmwareType,
    vehicleType,
    connected,
    state,
    positionValid,
    latitude,
    longitude,
    altitudeMeters,
    headingDegrees,
  );
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
