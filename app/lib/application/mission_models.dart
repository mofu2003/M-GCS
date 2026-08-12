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

  MissionItemSnapshot copyWith({
    int? sequence,
    double? latitude,
    double? longitude,
    double? altitudeMeters,
    double? holdTimeSeconds,
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
