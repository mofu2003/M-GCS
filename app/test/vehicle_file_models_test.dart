import 'package:flutter_test/flutter_test.dart';

import 'package:mgcs/application/vehicle_file_models.dart';

void main() {
  group('vehicle file paths', () {
    test('joins paths from the flight controller root', () {
      expect(joinVehiclePath('/', 'APM'), '/APM');
      expect(joinVehiclePath('/APM/', 'LOGS'), '/APM/LOGS');
    });

    test('returns the parent directory without escaping root', () {
      expect(parentVehiclePath('/'), '/');
      expect(parentVehiclePath('/APM'), '/');
      expect(parentVehiclePath('/APM/LOGS'), '/APM');
    });
  });

  group('vehicle file presentation', () {
    test('formats directory and file sizes', () {
      const directory = VehicleFileEntry(
        name: 'LOGS',
        path: '/LOGS',
        isDirectory: true,
        sizeBytes: 0,
      );
      const file = VehicleFileEntry(
        name: '00000001.BIN',
        path: '/APM/LOGS/00000001.BIN',
        isDirectory: false,
        sizeBytes: 1536,
      );

      expect(directory.sizeLabel, '文件夹');
      expect(file.sizeLabel, '1.5 KB');
    });

    test('clamps download progress to the valid range', () {
      const file = VehicleFileEntry(
        name: 'log.bin',
        path: '/log.bin',
        isDirectory: false,
        sizeBytes: 100,
      );
      expect(
        VehicleFileDownloadProgress(
          file: file,
          state: VehicleFileDownloadState.downloading,
          receivedBytes: -10,
          totalBytes: 100,
        ).fraction,
        0,
      );
      expect(
        VehicleFileDownloadProgress(
          file: file,
          state: VehicleFileDownloadState.completed,
          receivedBytes: 150,
          totalBytes: 100,
        ).fraction,
        1,
      );
    });
  });
}
