enum VehicleFileDownloadState {
  idle,
  downloading,
  completed,
  cancelled,
  failed,
}

enum VehicleFileUploadState { idle, uploading, completed, cancelled, failed }

final class VehicleFileEntry {
  const VehicleFileEntry({
    required this.name,
    required this.path,
    required this.isDirectory,
    required this.sizeBytes,
  });

  final String name;
  final String path;
  final bool isDirectory;
  final int sizeBytes;

  String get sizeLabel {
    if (isDirectory) return '文件夹';
    if (sizeBytes < 1024) return '$sizeBytes B';
    if (sizeBytes < 1024 * 1024) {
      return '${(sizeBytes / 1024).toStringAsFixed(1)} KB';
    }
    if (sizeBytes < 1024 * 1024 * 1024) {
      return '${(sizeBytes / (1024 * 1024)).toStringAsFixed(1)} MB';
    }
    return '${(sizeBytes / (1024 * 1024 * 1024)).toStringAsFixed(1)} GB';
  }
}

final class VehicleFileDownloadProgress {
  const VehicleFileDownloadProgress({
    required this.file,
    required this.state,
    required this.receivedBytes,
    required this.totalBytes,
    this.destinationPath,
    this.message = '',
  });

  final VehicleFileEntry file;
  final VehicleFileDownloadState state;
  final int receivedBytes;
  final int totalBytes;
  final String? destinationPath;
  final String message;

  double get fraction =>
      totalBytes <= 0 ? 0 : (receivedBytes / totalBytes).clamp(0.0, 1.0);
}

final class VehicleFileUploadProgress {
  const VehicleFileUploadProgress({
    required this.remotePath,
    required this.localPath,
    required this.state,
    required this.sentBytes,
    required this.totalBytes,
    this.message = '',
  });

  final String remotePath;
  final String localPath;
  final VehicleFileUploadState state;
  final int sentBytes;
  final int totalBytes;
  final String message;

  double get fraction =>
      totalBytes <= 0 ? 0 : (sentBytes / totalBytes).clamp(0.0, 1.0);
}

String joinVehiclePath(String base, String name) {
  final normalizedBase = base.trim().isEmpty ? '/' : base.trim();
  if (normalizedBase == '/') return '/$name';
  return '${normalizedBase.replaceFirst(RegExp(r'/+$'), '')}/$name';
}

String parentVehiclePath(String path) {
  final normalized = path.trim().isEmpty ? '/' : path.trim();
  if (normalized == '/') return '/';
  final separator = normalized.lastIndexOf('/');
  if (separator <= 0) return '/';
  return normalized.substring(0, separator);
}
