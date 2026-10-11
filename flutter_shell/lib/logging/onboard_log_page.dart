import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_confirm.dart';
import '../widgets/mg_inline_notice.dart';
import 'log_controller.dart';
import 'log_models.dart';

/// 机载 DataFlash 日志页：列出日志、逐条下载、显示进度、可取消。
///
/// 覆盖 S1 的正常、取消与失败分支；下载结果不推测，只按 native 的
/// `mg_onboard_log_download_status` 呈现。
class OnboardLogPage extends StatefulWidget {
  const OnboardLogPage({super.key, required this.controller});

  final LogController controller;

  @override
  State<OnboardLogPage> createState() => _OnboardLogPageState();
}

class _OnboardLogPageState extends State<OnboardLogPage> {
  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onChanged);
    // 进入页面即请求一次；断链时控制器会给出确定的错误，而不是停在加载态。
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) widget.controller.refreshList();
    });
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onChanged);
    super.dispose();
  }

  void _onChanged() {
    if (mounted) setState(() {});
  }

  /// 擦除全部日志是不可恢复的飞控操作，先走全应用统一的确认弹窗。
  Future<void> _confirmErase() async {
    final confirmed = await showMgConfirmDialog(
      context,
      title: '擦除全部机载日志？',
      message: '将请求飞控删除机上全部日志文件，此操作不可恢复。已下载到本地的副本不受影响。',
      confirmLabel: '擦除',
    );
    if (!confirmed || !mounted) return;
    await widget.controller.eraseAll();
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final colors = context.mgColors;
    final download = controller.download;
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
              Text('机载日志', style: MgTypography.sans(MgTypography.section)),
              const SizedBox(width: 12),
              if (controller.loadingList)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CupertinoActivityIndicator(radius: 7),
                ),
              const Spacer(),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed:
                    controller.backend.isConnected &&
                        !controller.loadingList &&
                        !controller.isDownloading
                    ? _confirmErase
                    : null,
                child: const Text('擦除全部'),
              ),
              const SizedBox(width: 8),
              PushButton(
                controlSize: ControlSize.regular,
                onPressed: controller.loadingList
                    ? null
                    : () => controller.refreshList(),
                child: const Text('刷新列表'),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (controller.listError case final message?)
            MgInlineNotice(
              icon: CupertinoIcons.exclamationmark_circle,
              color: colors.danger,
              text: message,
            ),
          if (controller.actionError case final message?)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MgInlineNotice(
                icon: CupertinoIcons.exclamationmark_triangle,
                color: colors.warning,
                text: message,
                trailing: MgDismissButton(onPressed: controller.clearActionError),
              ),
            ),
          if (controller.actionMessage case final message?)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: MgInlineNotice(
                icon: CupertinoIcons.checkmark_circle,
                color: colors.accent,
                text: message,
                trailing: MgDismissButton(onPressed: controller.clearActionMessage),
              ),
            ),
          if (download != null) ...[
            const SizedBox(height: 12),
            _DownloadCard(
              download: download,
              onCancel: download.isActive ? controller.cancelDownload : null,
            ),
          ],
          const SizedBox(height: 20),
          Row(
            children: [
              Text('日志列表', style: MgTypography.sans(MgTypography.section)),
              const SizedBox(width: 10),
              Text(
                controller.entries.isEmpty
                    ? ''
                    : '共 ${controller.entries.length} 条'
                          '${controller.listComplete ? '' : '（接收中）'}',
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.textSecondary),
              ),
            ],
          ),
          const SizedBox(height: 10),
          if (controller.entries.isEmpty && !controller.loadingList)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 24),
              child: Text(
                controller.listError == null
                    ? '暂无机载日志。点「刷新列表」重新向飞控请求。'
                    : '尚未取得日志列表。',
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
                child: _LogRow(
                  entry: entry,
                  enabled:
                      !controller.isDownloading &&
                      controller.backend.isConnected,
                  onDownload: () => controller.downloadEntry(entry),
                ),
              ),
        ],
      ),
    );
  }
}

class _LogRow extends StatelessWidget {
  const _LogRow({
    required this.entry,
    required this.enabled,
    required this.onDownload,
  });

  final OnboardLogEntry entry;
  final bool enabled;
  final VoidCallback onDownload;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final stamp = entry.timestamp;
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
            CupertinoIcons.doc_text,
            size: 16,
            color: colors.textSecondary,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  '日志 #${entry.id}',
                  style: MacosTheme.of(context).typography.body,
                ),
                const SizedBox(height: 2),
                Text(
                  stamp == null ? '记录时间未知' : _formatStamp(stamp),
                  style: MacosTheme.of(
                    context,
                  ).typography.caption1.copyWith(color: colors.textSecondary),
                ),
              ],
            ),
          ),
          Text(
            formatByteSize(entry.sizeBytes),
            style: MacosTheme.of(context).typography.caption1.copyWith(
              color: colors.textSecondary,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
          const SizedBox(width: 16),
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

class _DownloadCard extends StatelessWidget {
  const _DownloadCard({required this.download, required this.onCancel});

  final OnboardLogDownload download;
  final VoidCallback? onCancel;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final (label, color) = switch (download.phase) {
      LogDownloadPhase.downloading => ('下载中', colors.accent),
      LogDownloadPhase.saving => ('正在写入本地文件', colors.accent),
      LogDownloadPhase.completed => ('已完成', colors.success),
      LogDownloadPhase.cancelled => ('已取消', colors.textSecondary),
      LogDownloadPhase.failed => ('失败', colors.danger),
      LogDownloadPhase.idle => ('空闲', colors.textSecondary),
    };
    final counter = download.totalBytes > 0
        ? '${formatByteSize(download.receivedBytes)} / ${formatByteSize(download.totalBytes)}'
        : formatByteSize(download.receivedBytes);
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
                '日志 #${download.logId}',
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
            value: download.progress * 100,
            semanticLabel: '日志下载进度 $label',
          ),
          if (download.phase == LogDownloadPhase.completed)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                '已写入 ${formatByteSize(download.savedBytes)} → ${download.path}',
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.textSecondary),
              ),
            ),
          if (download.message.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                download.message,
                style: MacosTheme.of(
                  context,
                ).typography.caption1.copyWith(color: colors.danger),
              ),
            ),
        ],
      ),
    );
  }
}

String _formatStamp(DateTime stamp) {
  String two(int value) => value.toString().padLeft(2, '0');
  return '${stamp.year}-${two(stamp.month)}-${two(stamp.day)} '
      '${two(stamp.hour)}:${two(stamp.minute)}:${two(stamp.second)}';
}

/// 把字节数格式化成 KB/MB，供列表与下载卡片共用。
String formatByteSize(int bytes) {
  if (bytes < 1024) return '$bytes B';
  const units = ['KB', 'MB', 'GB'];
  var value = bytes / 1024;
  var unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  return '${value.toStringAsFixed(value >= 100 ? 0 : 1)} ${units[unit]}';
}
