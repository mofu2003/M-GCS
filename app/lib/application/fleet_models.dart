import 'connection_models.dart';

enum FleetCommandKind { arm, disarm, setMode, rtl, land }

final class FleetVehicleSnapshot {
  const FleetVehicleSnapshot({
    required this.vehicleId,
    required this.systemId,
    required this.name,
    required this.endpoint,
    required this.firmwareType,
    required this.vehicleType,
    required this.connected,
  });

  final int vehicleId;
  final int systemId;
  final String name;
  final String endpoint;
  final String firmwareType;
  final String vehicleType;
  final bool connected;
}

final class FleetCommandRequest {
  const FleetCommandRequest({required this.kind, this.mode});

  final FleetCommandKind kind;
  final String? mode;
}

final class FleetCommandResult {
  const FleetCommandResult({required this.vehicleId, required this.result});

  final int vehicleId;
  final ConnectionOperationResult result;

  bool get success => result.success;
}
