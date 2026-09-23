part of 'backstage_page.dart';

final class _FlightLogsPage extends StatefulWidget {
  const _FlightLogsPage({required this.backend});

  final VehicleFileSessionBackend? backend;

  @override
  State<_FlightLogsPage> createState() => _FlightLogsPageState();
}

final class _FlightLogsPageState extends State<_FlightLogsPage> {
  String _currentPath = '/';
  List<VehicleFileEntry> _entries = const [];
  VehicleFileEntry? _selected;
  VehicleFileDownloadProgress? _progress;
  VehicleFileUploadProgress? _uploadProgress;
  StreamSubscription<VehicleFileDownloadProgress>? _download;
  StreamSubscription<VehicleFileUploadProgress>? _upload;
  bool _loading = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    if (widget.backend != null) _loadDirectory('/');
  }

  @override
  void dispose() {
    _download?.cancel();
    _upload?.cancel();
    super.dispose();
  }

  Future<void> _loadDirectory(String path) async {
    final backend = widget.backend;
    if (backend == null || _loading) return;
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      final entries = await backend.listDirectory(path);
      if (!mounted) return;
      setState(() {
        _currentPath = path;
        _entries = entries;
        _selected = null;
      });
    } on Object catch (error) {
      if (mounted) setState(() => _error = '$error');
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _openEntry(VehicleFileEntry entry) async {
    if (entry.isDirectory) {
      await _loadDirectory(entry.path);
    } else if (mounted) {
      setState(() => _selected = entry);
    }
  }

  Future<void> _downloadSelected() async {
    final backend = widget.backend;
    final file = _selected;
    if (backend == null ||
        file == null ||
        file.isDirectory ||
        _progress?.state == VehicleFileDownloadState.downloading) {
      return;
    }
    final location = await getSaveLocation(
      suggestedName: file.name,
      acceptedTypeGroups: const [XTypeGroup(label: '飞控文件')],
    );
    if (location == null || !mounted) return;
    await _download?.cancel();
    setState(() {
      _error = null;
      _progress = VehicleFileDownloadProgress(
        file: file,
        state: VehicleFileDownloadState.downloading,
        receivedBytes: 0,
        totalBytes: file.sizeBytes,
        destinationPath: location.path,
      );
    });
    _download = backend
        .downloadFile(file, location.path)
        .listen(
          (progress) {
            if (!mounted) return;
            setState(() => _progress = progress);
            if (progress.state == VehicleFileDownloadState.completed) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text('已保存 ${file.name} 到 ${location.path}')),
              );
            }
          },
          onError: (Object error) {
            if (!mounted) return;
            setState(() {
              _progress = VehicleFileDownloadProgress(
                file: file,
                state: VehicleFileDownloadState.failed,
                receivedBytes: 0,
                totalBytes: file.sizeBytes,
                destinationPath: location.path,
                message: '$error',
              );
            });
          },
        );
  }

  Future<void> _cancelDownload() async {
    if (_progress?.state != VehicleFileDownloadState.downloading) return;
    try {
      await widget.backend?.cancelFileDownload();
    } on Object catch (error) {
      if (mounted) setState(() => _error = '$error');
      return;
    }
    await _download?.cancel();
    final progress = _progress;
    if (mounted && progress != null) {
      setState(
        () => _progress = VehicleFileDownloadProgress(
          file: progress.file,
          state: VehicleFileDownloadState.cancelled,
          receivedBytes: progress.receivedBytes,
          totalBytes: progress.totalBytes,
          destinationPath: progress.destinationPath,
          message: '已取消',
        ),
      );
    }
  }

  Future<void> _uploadFile() async {
    final backend = widget.backend;
    if (backend == null ||
        _uploadProgress?.state == VehicleFileUploadState.uploading ||
        _progress?.state == VehicleFileDownloadState.downloading) {
      return;
    }
    final file = await openFile(
      acceptedTypeGroups: const [XTypeGroup(label: '本地文件')],
    );
    if (file == null || !mounted) return;
    final fileName = file.name.trim();
    if (fileName.isEmpty || fileName.contains('/') || fileName.contains('\\')) {
      setState(() => _error = '本地文件名无效');
      return;
    }
    final remotePath = joinVehiclePath(_currentPath, fileName);
    await _upload?.cancel();
    setState(() {
      _error = null;
      _uploadProgress = VehicleFileUploadProgress(
        localPath: file.path,
        remotePath: remotePath,
        state: VehicleFileUploadState.uploading,
        sentBytes: 0,
        totalBytes: 0,
      );
    });
    _upload = backend
        .uploadFile(file.path, remotePath)
        .listen(
          (progress) {
            if (!mounted) return;
            setState(() => _uploadProgress = progress);
            if (progress.state == VehicleFileUploadState.completed) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text('已上传 ${file.name} 到 $_currentPath')),
              );
              _loadDirectory(_currentPath);
            }
          },
          onError: (Object error) {
            if (!mounted) return;
            setState(() {
              _uploadProgress = VehicleFileUploadProgress(
                localPath: file.path,
                remotePath: remotePath,
                state: VehicleFileUploadState.failed,
                sentBytes: 0,
                totalBytes: 0,
                message: '$error',
              );
            });
          },
        );
  }

  @override
  Widget build(BuildContext context) {
    final backend = widget.backend;
    if (backend == null) {
      return _BackstageFeatureCard(
        child: const Center(child: Text('当前连接后端不支持飞控内存卡文件访问')),
      );
    }
    final progress = _progress;
    final downloading = progress?.state == VehicleFileDownloadState.downloading;
    return Column(
      children: [
        _BackstageFeatureCard(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(18, 14, 14, 14),
            child: Row(
              children: [
                const Icon(
                  Icons.sd_storage_outlined,
                  color: MacGlassTokens.blue,
                ),
                const SizedBox(width: 10),
                const Expanded(
                  child: Text(
                    '飞控内存卡文件',
                    style: MacGlassTypography.sectionTitle,
                  ),
                ),
                _CommandButton(
                  icon: Icons.arrow_upward_rounded,
                  label: '返回上级',
                  onPressed: _currentPath == '/' || _loading
                      ? null
                      : () => _loadDirectory(parentVehiclePath(_currentPath)),
                ),
                const SizedBox(width: 8),
                _CommandButton(
                  icon: Icons.refresh_rounded,
                  label: '刷新',
                  onPressed: _loading
                      ? null
                      : () => _loadDirectory(_currentPath),
                ),
                const SizedBox(width: 8),
                _CommandButton(
                  icon: Icons.file_upload_outlined,
                  label: '上传文件',
                  onPressed:
                      _uploadProgress?.state == VehicleFileUploadState.uploading
                      ? null
                      : _uploadFile,
                ),
                const SizedBox(width: 8),
                _CommandButton(
                  icon: Icons.download_rounded,
                  label: '下载选中',
                  onPressed: _selected == null || downloading
                      ? null
                      : _downloadSelected,
                ),
                if (downloading) ...[
                  const SizedBox(width: 8),
                  _CommandButton(
                    icon: Icons.stop_circle_outlined,
                    label: '取消',
                    onPressed: _cancelDownload,
                  ),
                ],
              ],
            ),
          ),
        ),
        const SizedBox(height: 10),
        _BackstageFeatureCard(
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 10),
            child: Row(
              children: [
                const Icon(
                  Icons.folder_open_rounded,
                  size: 18,
                  color: MacGlassTokens.blue,
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    _currentPath,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: MacGlassTypography.caption.copyWith(
                      fontFamily: 'SF Mono',
                    ),
                  ),
                ),
                Text('${_entries.length} 项', style: MacGlassTypography.caption),
              ],
            ),
          ),
        ),
        if (_error != null)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: Align(
              alignment: Alignment.centerLeft,
              child: Text(_error!, style: const TextStyle(color: Colors.red)),
            ),
          ),
        if (progress != null && progress.state != VehicleFileDownloadState.idle)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: _BackstageFeatureCard(
              child: Padding(
                padding: const EdgeInsets.all(14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '${progress.state == VehicleFileDownloadState.downloading ? '正在下载' : '下载状态'}：${progress.file.name} · ${progress.receivedBytes} / ${progress.totalBytes} B',
                    ),
                    if (progress.state ==
                        VehicleFileDownloadState.downloading) ...[
                      const SizedBox(height: 8),
                      LinearProgressIndicator(value: progress.fraction),
                    ],
                    if (progress.message.isNotEmpty) ...[
                      const SizedBox(height: 4),
                      Text(progress.message, style: MacGlassTypography.caption),
                    ],
                  ],
                ),
              ),
            ),
          ),
        if (_uploadProgress != null &&
            _uploadProgress!.state != VehicleFileUploadState.idle)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: _BackstageFeatureCard(
              child: Padding(
                padding: const EdgeInsets.all(14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '${_uploadProgress!.state == VehicleFileUploadState.uploading ? '正在上传' : '上传状态'}：${_uploadProgress!.remotePath}',
                    ),
                    if (_uploadProgress!.state ==
                        VehicleFileUploadState.uploading) ...[
                      const SizedBox(height: 8),
                      LinearProgressIndicator(value: _uploadProgress!.fraction),
                    ],
                    if (_uploadProgress!.message.isNotEmpty) ...[
                      const SizedBox(height: 4),
                      Text(
                        _uploadProgress!.message,
                        style: MacGlassTypography.caption,
                      ),
                    ],
                  ],
                ),
              ),
            ),
          ),
        const SizedBox(height: 10),
        Expanded(
          child: _BackstageFeatureCard(
            child: _loading && _entries.isEmpty
                ? const Center(child: Text('正在读取飞控内存卡…'))
                : _entries.isEmpty
                ? const Center(child: Text('当前目录为空，或飞控未返回目录项'))
                : ListView.separated(
                    padding: const EdgeInsets.all(10),
                    itemCount: _entries.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 4),
                    itemBuilder: (context, index) {
                      final entry = _entries[index];
                      final selected = _selected?.path == entry.path;
                      return Material(
                        color: selected
                            ? MacGlassTokens.blue.withValues(alpha: 0.12)
                            : Colors.transparent,
                        borderRadius: BorderRadius.circular(12),
                        child: ListTile(
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12),
                          ),
                          selected: selected,
                          onTap: () => _openEntry(entry),
                          leading: Icon(
                            entry.isDirectory
                                ? Icons.folder_rounded
                                : Icons.insert_drive_file_outlined,
                            color: entry.isDirectory
                                ? MacGlassTokens.blue
                                : MacGlassTokens.muted,
                          ),
                          title: Text(entry.name),
                          subtitle: Text(
                            entry.isDirectory ? '文件夹' : entry.path,
                          ),
                          trailing: Text(entry.sizeLabel),
                        ),
                      );
                    },
                  ),
          ),
        ),
      ],
    );
  }
}
