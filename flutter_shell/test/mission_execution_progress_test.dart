import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/mission/mission_document_models.dart';

void main() {
  test(
    'flight ending is distinct from mission completion in the FFI snapshot',
    () {
      final progress = MissionExecutionProgress.fromJson({
        'valid': true,
        'total': 7,
        'current_index': 3,
        'reached_count': 3,
        'ended': true,
        'finished': false,
      });
      expect(progress.ended, isTrue);
      expect(progress.finished, isFalse);
      expect(progress.fraction, 3 / 7);
    },
  );

  test(
    'returning to an earlier target is not pinned by old reached counts',
    () {
      expect(
        const MissionExecutionProgress(
          valid: true,
          total: 8,
          currentIndex: 3,
          reachedCount: 6,
        ).fraction,
        3 / 8,
      );
    },
  );

  test(
    'progress follows the current target even if reached events are lost',
    () {
      expect(
        const MissionExecutionProgress(
          valid: true,
          total: 4,
          currentIndex: 2,
        ).fraction,
        0.5,
      );
      expect(
        const MissionExecutionProgress(
          valid: true,
          total: 4,
          currentIndex: 3,
        ).fraction,
        0.75,
      );
      expect(
        const MissionExecutionProgress(
          valid: true,
          total: 4,
          reachedEnd: true,
        ).fraction,
        1,
      );
      expect(const MissionExecutionProgress().fraction, 0);
    },
  );
}
