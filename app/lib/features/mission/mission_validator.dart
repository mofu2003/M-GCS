import '../../application/mission_models.dart';

final class MissionValidationIssue {
  const MissionValidationIssue({required this.message, this.index});

  final String message;
  final int? index;
}

final class MissionValidationResult {
  const MissionValidationResult(this.issues);

  final List<MissionValidationIssue> issues;

  bool get isValid => issues.isEmpty;
}

/// Pure validation for the editable, vehicle-independent mission draft.
final class MissionValidator {
  const MissionValidator();

  MissionValidationResult validate(Iterable<MissionItemSnapshot> items) {
    final issues = <MissionValidationIssue>[];
    var expectedSequence = 0;
    for (final item in items) {
      if (item.sequence != expectedSequence) {
        issues.add(
          MissionValidationIssue(index: expectedSequence, message: '任务序号必须连续'),
        );
      }
      expectedSequence++;
      final lat = item.latitude;
      final lon = item.longitude;
      final alt = item.altitudeMeters;
      if (lat == null || lat < -90 || lat > 90) {
        issues.add(
          MissionValidationIssue(
            index: item.sequence,
            message: '纬度必须在 -90 到 90 之间',
          ),
        );
      }
      if (lon == null || lon < -180 || lon > 180) {
        issues.add(
          MissionValidationIssue(
            index: item.sequence,
            message: '经度必须在 -180 到 180 之间',
          ),
        );
      }
      if (alt == null || !alt.isFinite || alt < 0) {
        issues.add(
          MissionValidationIssue(index: item.sequence, message: '高度必须是非负数'),
        );
      }
      final speed = item.speedMetersPerSecond;
      if (speed != null && (!speed.isFinite || speed <= 0)) {
        issues.add(
          MissionValidationIssue(index: item.sequence, message: '速度必须大于 0'),
        );
      }
    }
    return MissionValidationResult(List.unmodifiable(issues));
  }
}
