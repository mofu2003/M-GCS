import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/mission_models.dart';
import 'package:mgcs/application/mission_validator.dart';

MissionItemSnapshot waypoint({
  int sequence = 0,
  double? latitude = 31.2,
  double? longitude = 121.4,
  double? altitude = 50,
}) => MissionItemSnapshot(
  sequence: sequence,
  command: 16,
  commandName: 'NAV_WAYPOINT',
  frame: 3,
  frameName: 'GLOBAL_REL_ALT',
  autoContinue: true,
  latitude: latitude,
  longitude: longitude,
  altitudeMeters: altitude,
);

void main() {
  const validator = MissionValidator();

  test('accepts a normal waypoint plan', () {
    expect(validator.validate([waypoint()]).isValid, isTrue);
  });

  test('rejects invalid coordinates and negative altitude', () {
    final result = validator.validate([
      waypoint(latitude: 91, longitude: -181, altitude: -1),
    ]);
    expect(result.isValid, isFalse);
    expect(result.issues, hasLength(3));
  });

  test('requires continuous sequence numbers', () {
    final result = validator.validate([waypoint(sequence: 2)]);
    expect(result.issues.any((issue) => issue.message.contains('序号')), isTrue);
  });
}
