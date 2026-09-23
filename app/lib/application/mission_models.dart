final class MissionItemSnapshot {
  const MissionItemSnapshot({
    required this.sequence,
    required this.command,
    required this.commandName,
    required this.frame,
    required this.frameName,
    required this.autoContinue,
    this.latitude,
    this.longitude,
    this.altitudeMeters,
    this.holdTimeSeconds = 0,
    this.speedMetersPerSecond,
  });

  final int sequence;
  final int command;
  final String commandName;
  final int frame;
  final String frameName;
  final bool autoContinue;
  final double? latitude;
  final double? longitude;
  final double? altitudeMeters;
  final double holdTimeSeconds;
  final double? speedMetersPerSecond;

  MissionItemSnapshot copyWith({
    int? sequence,
    double? latitude,
    double? longitude,
    double? altitudeMeters,
    double? holdTimeSeconds,
    double? speedMetersPerSecond,
  }) => MissionItemSnapshot(
    sequence: sequence ?? this.sequence,
    command: command,
    commandName: commandName,
    frame: frame,
    frameName: frameName,
    autoContinue: autoContinue,
    latitude: latitude ?? this.latitude,
    longitude: longitude ?? this.longitude,
    altitudeMeters: altitudeMeters ?? this.altitudeMeters,
    holdTimeSeconds: holdTimeSeconds ?? this.holdTimeSeconds,
    speedMetersPerSecond: speedMetersPerSecond ?? this.speedMetersPerSecond,
  );

  Map<String, Object?> toTransferJson() => <String, Object?>{
    'seq': sequence,
    'command': command,
    'frame': frame,
    'autocontinue': autoContinue ? 1 : 0,
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
    'hold_time': holdTimeSeconds,
    if (speedMetersPerSecond != null) 'speed': speedMetersPerSecond,
  };
}

final class MissionPlanSnapshot {
  const MissionPlanSnapshot({
    this.items = const [],
    this.currentIndex = 0,
    this.valid = false,
  });

  final List<MissionItemSnapshot> items;
  final int currentIndex;
  final bool valid;
}

enum MissionTransferState { idle, downloading, uploading, waitingAck }

final class MissionProgressSnapshot {
  const MissionProgressSnapshot({
    this.state = MissionTransferState.idle,
    this.total = 0,
    this.received = 0,
    this.sent = 0,
    this.result = 0,
    this.error = '',
    this.progressPercent = 0,
  });

  final MissionTransferState state;
  final int total;
  final int received;
  final int sent;
  final int result;
  final String error;
  final int progressPercent;

  bool get active => state != MissionTransferState.idle;

  MissionProgressSnapshot copyWith({
    MissionTransferState? state,
    int? total,
    int? received,
    int? sent,
    int? result,
    String? error,
    int? progressPercent,
  }) => MissionProgressSnapshot(
    state: state ?? this.state,
    total: total ?? this.total,
    received: received ?? this.received,
    sent: sent ?? this.sent,
    result: result ?? this.result,
    error: error ?? this.error,
    progressPercent: progressPercent ?? this.progressPercent,
  );
}

final class SurveyConfig {
  const SurveyConfig({
    required this.centerLatitude,
    required this.centerLongitude,
    this.widthMeters = 100,
    this.lengthMeters = 100,
    this.altitudeMeters = 50,
    this.speedMetersPerSecond = 5,
    this.angleDegrees = 0,
    this.crossGrid = false,
    this.returnToStart = true,
  });

  final double centerLatitude;
  final double centerLongitude;
  final double widthMeters;
  final double lengthMeters;
  final double altitudeMeters;
  final double speedMetersPerSecond;
  final double angleDegrees;
  final bool crossGrid;
  final bool returnToStart;

  Map<String, Object?> toJson() => {
    'center_lat': centerLatitude,
    'center_lon': centerLongitude,
    'width': widthMeters,
    'length': lengthMeters,
    'altitude': altitudeMeters,
    'speed': speedMetersPerSecond,
    'angle': angleDegrees,
    'cross_grid': crossGrid,
    'return_to_start': returnToStart,
  };
}

final class CorridorConfig {
  const CorridorConfig({
    required this.startLatitude,
    required this.startLongitude,
    required this.endLatitude,
    required this.endLongitude,
    this.widthMeters = 50,
    this.altitudeMeters = 50,
    this.speedMetersPerSecond = 5,
    this.sideOverlapPercent = 60,
    this.returnToStart = true,
  });

  final double startLatitude;
  final double startLongitude;
  final double endLatitude;
  final double endLongitude;
  final double widthMeters;
  final double altitudeMeters;
  final double speedMetersPerSecond;
  final double sideOverlapPercent;
  final bool returnToStart;

  Map<String, Object?> toJson() => {
    'start_lat': startLatitude,
    'start_lon': startLongitude,
    'end_lat': endLatitude,
    'end_lon': endLongitude,
    'width': widthMeters,
    'altitude': altitudeMeters,
    'speed': speedMetersPerSecond,
    'side_overlap': sideOverlapPercent,
    'return_to_start': returnToStart,
  };
}

final class MissionGenerationResult {
  const MissionGenerationResult({
    required this.valid,
    this.items = const [],
    this.waypointCount = 0,
    this.flightLineCount = 0,
    this.totalDistanceMeters = 0,
    this.estimatedTimeSeconds = 0,
    this.areaSquareMeters = 0,
    this.photoCount = 0,
    this.error = '',
  });

  final bool valid;
  final List<MissionItemSnapshot> items;
  final int waypointCount;
  final int flightLineCount;
  final double totalDistanceMeters;
  final double estimatedTimeSeconds;
  final double areaSquareMeters;
  final int photoCount;
  final String error;
}
