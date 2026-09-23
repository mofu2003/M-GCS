final class FencePointSnapshot {
  const FencePointSnapshot({
    required this.latitude,
    required this.longitude,
    this.altitudeMeters = 0,
  });
  final double latitude;
  final double longitude;
  final double altitudeMeters;
}

final class GeofenceConfigSnapshot {
  const GeofenceConfigSnapshot({
    this.enabled = false,
    this.maxAltitudeMeters = 120,
    this.minAltitudeMeters = 0,
    this.action = 0,
    this.returnAtMinimum = false,
    this.breachDistanceMeters = 10,
  });
  final bool enabled;
  final double maxAltitudeMeters;
  final double minAltitudeMeters;
  final int action;
  final bool returnAtMinimum;
  final double breachDistanceMeters;

  GeofenceConfigSnapshot copyWith({
    bool? enabled,
    double? maxAltitudeMeters,
    double? minAltitudeMeters,
    int? action,
    bool? returnAtMinimum,
    double? breachDistanceMeters,
  }) => GeofenceConfigSnapshot(
    enabled: enabled ?? this.enabled,
    maxAltitudeMeters: maxAltitudeMeters ?? this.maxAltitudeMeters,
    minAltitudeMeters: minAltitudeMeters ?? this.minAltitudeMeters,
    action: action ?? this.action,
    returnAtMinimum: returnAtMinimum ?? this.returnAtMinimum,
    breachDistanceMeters: breachDistanceMeters ?? this.breachDistanceMeters,
  );
}

final class GeofenceValidationSnapshot {
  const GeofenceValidationSnapshot({
    required this.valid,
    this.error = '',
    this.warnings = const [],
    this.areaSquareMeters = 0,
    this.perimeterMeters = 0,
  });
  final bool valid;
  final String error;
  final List<String> warnings;
  final double areaSquareMeters;
  final double perimeterMeters;
}
