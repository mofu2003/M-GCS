import 'dart:math' as math;

enum SensorCalibrationType { accelerometer, magnetometer, airspeed }

enum AccelerometerCalibrationMode {
  sixFace(1),
  level(2);

  const AccelerometerCalibrationMode(this.nativeValue);

  final int nativeValue;
}

final class SensorVector3 {
  const SensorVector3({this.x = 0, this.y = 0, this.z = 0});

  final double x;
  final double y;
  final double z;

  double get magnitude => math.sqrt(x * x + y * y + z * z);
}

final class SensorTelemetrySnapshot {
  const SensorTelemetrySnapshot({
    this.acceleration = const SensorVector3(),
    this.gyroscope = const SensorVector3(),
    this.magnetometer = const SensorVector3(),
    this.temperatureCelsius,
    this.fieldsUpdated = 0,
  });

  final SensorVector3 acceleration;
  final SensorVector3 gyroscope;
  final SensorVector3 magnetometer;
  final double? temperatureCelsius;
  final int fieldsUpdated;

  bool get hasData => fieldsUpdated != 0;
}

final class CalibrationStatusSnapshot {
  const CalibrationStatusSnapshot({
    this.active = false,
    this.type = '',
    this.stage = 'idle',
    this.progress = 0,
    this.currentStep = 0,
    this.totalSteps = 0,
    this.message = '',
    this.failureReason = '',
    this.retryCount = 0,
    this.maxRetries = 0,
    this.accelerometerMode = '',
  });

  final bool active;
  final String type;
  final String stage;
  final int progress;
  final int currentStep;
  final int totalSteps;
  final String message;
  final String failureReason;
  final int retryCount;
  final int maxRetries;
  final String accelerometerMode;

  bool get isTerminal =>
      const {'success', 'failed', 'cancelled'}.contains(stage);
}

final class CalibrationResultSnapshot {
  const CalibrationResultSnapshot({
    this.type = '',
    this.stage = 'idle',
    this.quality = 'poor',
    this.timestampMilliseconds = 0,
    this.accelerometerMode = '',
    this.offsets = const SensorVector3(),
    this.previousOffsets = const SensorVector3(),
    this.scale,
    this.diagonals,
    this.offDiagonals,
    this.fitness,
    this.progress = 0,
    this.currentStep = 0,
    this.totalSteps = 0,
    this.failureReason = '',
  });

  final String type;
  final String stage;
  final String quality;
  final int timestampMilliseconds;
  final String accelerometerMode;
  final SensorVector3 offsets;
  final SensorVector3 previousOffsets;
  final SensorVector3? scale;
  final SensorVector3? diagonals;
  final SensorVector3? offDiagonals;
  final double? fitness;
  final int progress;
  final int currentStep;
  final int totalSteps;
  final String failureReason;

  bool get succeeded => stage == 'success' && failureReason.isEmpty;
}

final class CalibrationRetrySnapshot {
  const CalibrationRetrySnapshot({
    this.retryCount = 0,
    this.maxRetries = 0,
    this.backoffMilliseconds = 0,
    this.lastFailure = '',
  });

  final int retryCount;
  final int maxRetries;
  final int backoffMilliseconds;
  final String lastFailure;

  bool get hasFailure => lastFailure.isNotEmpty;
}
