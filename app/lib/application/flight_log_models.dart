import 'dart:typed_data';

enum FlightLogDownloadState { idle, downloading, completed, cancelled, failed }

final class FlightLogEntry {
  const FlightLogEntry({
    required this.id,
    required this.sizeBytes,
    this.timestamp,
  });

  final int id;
  final int sizeBytes;
  final DateTime? timestamp;

  String get sizeLabel {
    if (sizeBytes < 1024) return '$sizeBytes B';
    if (sizeBytes < 1024 * 1024) {
      return '${(sizeBytes / 1024).toStringAsFixed(1)} KB';
    }
    return '${(sizeBytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }
}

final class FlightLogDownloadProgress {
  const FlightLogDownloadProgress({
    required this.logId,
    required this.receivedBytes,
    required this.totalBytes,
    required this.state,
    this.destinationPath,
    this.message = '',
  });

  final int logId;
  final int receivedBytes;
  final int totalBytes;
  final FlightLogDownloadState state;
  final String? destinationPath;
  final String message;

  double get fraction =>
      totalBytes <= 0 ? 0 : (receivedBytes / totalBytes).clamp(0.0, 1.0);
}

DateTime? flightLogTimestampFromUtcSeconds(int seconds) {
  if (seconds <= 0) return null;
  return DateTime.fromMillisecondsSinceEpoch(
    seconds * 1000,
    isUtc: true,
  ).toLocal();
}

// Kept here to make future binary log readers able to share a byte container
// without leaking native buffers into the UI layer.
Uint8List copyFlightLogBytes(List<int> bytes) => Uint8List.fromList(bytes);
