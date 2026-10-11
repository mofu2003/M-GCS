import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_inline_notice.dart';
import 'file_controller.dart';
import 'file_index.dart';
import 'file_models.dart';
import 'onboard_log_page.dart' show formatByteSize;

/// 机载文件浏览页：列出机载目录、下载文件、上传文件，显示进度并可取消。
///
/// 覆盖 S3 的正常、失败与取消分支；所有结果只按 native 的
/// `mg_fs_transfer_status` 呈现，页面不推测完成。断链或飞控不响应时展示本地
/// 索引缓存，并明确标注它可能过期。
class FileBrowserPage extends StatefulWidget {
  const FileBrowserPage({super.key, required this.controller});

  final FileController controller;

  @override
  State<FileBrowserPage> createState() => _FileBrowserPageState();
}

class _FileBrowserPageState extends State<FileBrowserPage> {
  final _uploadController = TextEditingController();

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onChanged);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) widget.controller.refreshList();
    });
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onChanged);
    _uploadController.dispose();
    super.dispose();
  }

  void _onChanged() {
    if (mounted) setState(() {});
  }

  Future<void> _upload() async {
    await widget.controller.uploadFile(_uploadController.text);
    if (mounted && widget.controller.actionError == null) {
      _uploadController.clear();
    }
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    final transfer = controller.transfer;
    final busy = controller.listing || controller.isTransferring;
    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(
        MgMetrics.configPageMargin,
        22,
        MgMetrics.configPageMargin,
        32,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text('机载文件', style: MgTypography.sans(MgTypography.section)),
              const SizedBox(width: 12),
              if (controller.listing)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CupertinoActivityIndicator(radius: 7),
                ),
              const Spacer(),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: controller.parentPath == null || busy
                    ? null
                    : () => controller.goUp(),
                child: const Text('上一级'),
              ),
              const SizedBox(width: 8),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: busy ? null : () => controller.refreshList(),
                child: const Text('刷新目录'),
              ),
            ],
          ),
          const SizedBox(height: 10),
          _PathRow(
            path: controller.currentPath,
            onJump: busy ? null : (path) => controller.refreshList(path: path),
          ),
          if (controller.entriesFromCache) ...[
            const SizedBox(height: 10),
            MgInlineNotice(
              icon: CupertinoIcons.arrow_counterclockwise,
              color: colors.textSecondary,
              text:
                  '当前显示的是本地缓存的目录内容'
                  '（${_formatCacheTime(controller.cacheUpdatedAtMs)}），可能已过期。'
                  '连接飞控后点「刷新目录」获取实时列表。',
            ),
          ],
          if (controller.listError case final message?) ...[
            const SizedBox(height: 10),
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_circle,
              color: colors.danger,
              text: message,
            ),
          ],
          if (controller.indexWriteFailed) ...[
            const SizedBox(height: 8),
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_triangle,
              color: colors.warning,
              text: '本地文件索引写入失败，离线缓存与「已下载」标记可能不完整。',
            ),
          ],
          if (controller.actionError case final message?) ...[
            const SizedBox(height: 8),
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_triangle,
              color: colors.warning,
              text: message,
              trailing: MgDismissButton(
                onPressed: controller.clearActionError,
              ),
            ),
          ],
          if (controller.actionMessage case final message?) ...[
            const SizedBox(height: 8),
            MgInlineNotice(
              icon: CupertinoIcons.checkmark_circle,
              color: colors.accent,
              text: message,
              trailing: MgDismissButton(
                onPressed: controller.clearActionMessage,
              ),
            ),
          ],
          if (transfer != null && transfer.phase != FileTransferPhase.idle) ...[
            const SizedBox(height: 12),
            _TransferCard(
              transfer: transfer,
              onCancel: transfer.isActive ? controller.cancelTransfer : null,
            ),
          ],
          const SizedBox(height: 20),
          Row(
            children: [
              Text('目录内容', style: MgTypography.sans(MgTypography.section)),
              const SizedBox(width: 10),
              Text(
                controller.entries.isEmpty
                    ? ''
                    : '共 ${controller.entries.length} 项',
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.textSecondary),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (controller.entries.isEmpty && !controller.listing)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 24),
              child: Text(
                controller.listError == null
                    ? '目录为空或尚未读取。点「刷新目录」重新请求。'
                    : '尚未取得目录内容。',
                textAlign: TextAlign.center,
                style: MacosTheme.of(
                  context,
                ).typography.body.copyWith(color: colors.textSecondary),
              ),
            )
          else
            for (final entry in controller.entries)
              Padding(
                padding: const EdgeInsets.only(bottom: MgMetrics.configCardGap),
                child: _EntryRow(
                  entry: entry,
                  localCopy: controller.localCopy(
                    controller.childPath(entry.name),
                  ),
                  enabled: !busy && controller.backend.isConnected,
                  onOpen: () => controller.openDirectory(entry.name),
                  onDownload: () => controller.downloadEntry(entry),
                ),
              ),
          const SizedBox(height: 20),
          Text('上传到当前目录', style: MgTypography.sans(MgTypography.section)),
          const SizedBox(height: 10),
          _UploadRow(
            pathController: _uploadController,
            enabled: !busy && controller.backend.isConnected,
            onSubmit: _upload,
          ),
        ],
      ),
    );
  }
}

class _PathRow extends StatelessWidget {
  const _PathRow({required this.path, required this.onJump});

  final String path;
  final Future<void> Function(String path)? onJump;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      children: [
        Text(
          '当前目录',
          style: MacosTheme.of(
            context,
          ).typography.caption1.copyWith(color: colors.textSecondary),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            path,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(context).typography.body,
          ),
        ),
        for (final common in FileController.commonPaths) ...[
          const SizedBox(width: 6),
          PushButton(
            controlSize: ControlSize.small,
            onPressed: onJump == null || common == path
                ? null
                : () => onJump!(common),
            child: Text(common),
          ),
        ],
      ],
    );
  }
}

class _EntryRow extends StatelessWidget {
  const _EntryRow({
    required this.entry,
    required this.localCopy,
    required this.enabled,
    required this.onOpen,
    required this.onDownload,
  });

  final RemoteFileEntry entry;
  final CachedDownload? localCopy;
  final bool enabled;
  final VoidCallback onOpen;
  final VoidCallback onDownload;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final copy = localCopy;
    // 本地副本大小与机载声明一致才算「一致」；不一致时明确提示，不能谎报完整。
    final sizeMatches = copy != null && copy.sizeBytes == entry.sizeBytes;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: colors.surfaceMuted,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Row(
        children: [
          MacosIcon(
            entry.isDirectory
                ? CupertinoIcons.folder
                : CupertinoIcons.doc_text,
            size: 16,
            color: colors.textSecondary,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(entry.name, style: MacosTheme.of(context).typography.body),
                if (copy != null) ...[
                  const SizedBox(height: 2),
                  Text(
                    sizeMatches
                        ? '本地已有副本：${copy.localPath}'
                        : '本地副本大小与机载不一致（本地 ${formatByteSize(copy.sizeBytes)}，'
                              '机载 ${formatByteSize(entry.sizeBytes)}）',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: MacosTheme.of(context).typography.caption1.copyWith(
                      color: sizeMatches ? colors.success : colors.warning,
                    ),
                  ),
                ],
              ],
            ),
          ),
          if (!entry.isDirectory)
            Text(
              formatByteSize(entry.sizeBytes),
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: colors.textSecondary,
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
            ),
          const SizedBox(width: 16),
          if (entry.isDirectory)
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: enabled ? onOpen : null,
              child: const Text('打开'),
            )
          else
            PushButton(
              controlSize: ControlSize.regular,
              onPressed: enabled ? onDownload : null,
              child: const Text('下载'),
            ),
        ],
      ),
    );
  }
}

class _TransferCard extends StatelessWidget {
  const _TransferCard({required this.transfer, required this.onCancel});

  final FileTransfer transfer;
  final VoidCallback? onCancel;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final (label, color) = switch (transfer.phase) {
      FileTransferPhase.running => (switch (transfer.task) {
        FileTaskKind.download => '下载中',
        FileTaskKind.upload => '上传中',
        FileTaskKind.list => '读取目录中',
        FileTaskKind.idle => '进行中',
      }, colors.accent),
      FileTransferPhase.cancelling => ('正在取消', colors.warning),
      FileTransferPhase.completed => ('已完成', colors.success),
      FileTransferPhase.cancelled => ('已取消', colors.textSecondary),
      FileTransferPhase.failed => ('失败', colors.danger),
      FileTransferPhase.idle => ('空闲', colors.textSecondary),
    };
    final counter = transfer.totalBytes > 0
        ? '${formatByteSize(transfer.receivedBytes)} / ${formatByteSize(transfer.totalBytes)}'
        : formatByteSize(transfer.receivedBytes);
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Text(
                transfer.remotePath.isEmpty
                    ? transfer.localPath
                    : transfer.remotePath,
                style: MacosTheme.of(context).typography.body,
              ),
              const SizedBox(width: 10),
              Text(
                label,
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: color),
              ),
              const Spacer(),
              Text(
                counter,
                style: MacosTheme.of(context).typography.caption1.copyWith(
                  color: colors.textSecondary,
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
              if (onCancel != null) ...[
                const SizedBox(width: 12),
                PushButton(
                  controlSize: ControlSize.regular,
                  onPressed: onCancel,
                  child: const Text('取消'),
                ),
              ],
            ],
          ),
          const SizedBox(height: 10),
          ProgressBar(
            value: transfer.progress * 100,
            semanticLabel: '文件传输进度 $label',
          ),
          if (transfer.phase == FileTransferPhase.completed &&
              transfer.localPath.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                '已写入 ${transfer.localPath}',
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.textSecondary),
              ),
            ),
          if (transfer.message.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                transfer.message,
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(
                  color: transfer.phase == FileTransferPhase.failed
                      ? colors.danger
                      : colors.textSecondary,
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _UploadRow extends StatelessWidget {
  const _UploadRow({
    required this.pathController,
    required this.enabled,
    required this.onSubmit,
  });

  final TextEditingController pathController;
  final bool enabled;
  final VoidCallback onSubmit;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Expanded(
        child: MacosTextField(
          controller: pathController,
          enabled: enabled,
          placeholder: '要上传的本地文件完整路径',
          onSubmitted: (_) => onSubmit(),
        ),
      ),
      const SizedBox(width: 8),
      PushButton(
        controlSize: ControlSize.regular,
        onPressed: enabled ? onSubmit : null,
        child: const Text('上传'),
      ),
    ],
  );
}

String _formatCacheTime(int milliseconds) {
  if (milliseconds <= 0) return '时间未知';
  final stamp = DateTime.fromMillisecondsSinceEpoch(milliseconds);
  String two(int value) => value.toString().padLeft(2, '0');
  return '${stamp.year}-${two(stamp.month)}-${two(stamp.day)} '
      '${two(stamp.hour)}:${two(stamp.minute)}';
}
