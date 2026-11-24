import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'update_controller.dart';
import 'update_models.dart';

class UpdateSettingsPage extends StatefulWidget {
  const UpdateSettingsPage({
    super.key,
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final UpdateController controller;

  @override
  State<UpdateSettingsPage> createState() => _UpdateSettingsPageState();
}

class _UpdateSettingsPageState extends State<UpdateSettingsPage> {
  late final TextEditingController _urlController = TextEditingController(
    text: widget.controller.manifestUrl,
  );
  bool _starting = false;

  @override
  void initState() {
    super.initState();
    if (!widget.controller.initialized) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _initialize());
    }
  }

  Future<void> _initialize() async {
    await widget.controller.initialize();
    if (!mounted) return;
    _urlController.text = widget.controller.manifestUrl;
    setState(() {});
  }

  @override
  void dispose() {
    _urlController.dispose();
    super.dispose();
  }

  Future<void> _check() async {
    if (_starting) return;
    setState(() => _starting = true);
    try {
      await widget.controller.checkForUpdates();
    } on Object {
      // 控制器把错误写入状态，页面继续显示可重试入口。
    } finally {
      if (mounted) setState(() => _starting = false);
    }
  }

  Future<void> _applyUrl() async {
    if (_starting ||
        widget.controller.checking ||
        widget.controller.downloading) {
      return;
    }
    setState(() => _starting = true);
    try {
      await widget.controller.setManifestUrl(_urlController.text);
      if (!mounted) return;
      _urlController.text = widget.controller.manifestUrl;
    } on Object {
      // 控制器把保存失败写入状态。
    } finally {
      if (mounted) setState(() => _starting = false);
    }
  }

  Future<void> _restoreDefaultUrl() async {
    if (_starting ||
        widget.controller.checking ||
        widget.controller.downloading) {
      return;
    }
    setState(() => _starting = true);
    try {
      await widget.controller.restoreDefaultManifestUrl();
      if (!mounted) return;
      _urlController.text = widget.controller.manifestUrl;
    } on Object {
      // 控制器把保存失败写入状态。
    } finally {
      if (mounted) setState(() => _starting = false);
    }
  }

  Future<void> _setAutoCheck(bool value) async {
    try {
      await widget.controller.setAutomaticChecks(value);
    } on Object {
      // 控制器把保存失败写入状态。
    }
  }

  Future<void> _download() async {
    try {
      await widget.controller.downloadAvailableUpdate();
    } on Object {
      // 控制器保留具体失败原因。
    }
  }

  Future<void> _openDownloaded() async {
    try {
      await widget.controller.openDownloadedUpdate();
    } on Object {
      // 控制器保留具体失败原因。
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return AnimatedBuilder(
      animation: widget.controller,
      builder: (context, _) {
        final controller = widget.controller;
        final current = controller.currentVersion;
        final update = controller.availableUpdate;
        final asset = controller.selectedAsset;
        final downloaded = controller.downloadedFile;
        final hasSource = controller.manifestUrl.trim().isNotEmpty;
        return ListView(
          controller: widget.scrollController,
          padding: const EdgeInsets.all(20),
          children: [
            Text(
              '软件更新',
              style: MgTypography.sans(
                MgTypography.title.copyWith(color: colors.textPrimary),
              ),
            ),
            const SizedBox(height: 4),
            Text(
              '启动时自动检查新版本；更新包下载后先校验 SHA-256，再由系统打开安装。',
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: colors.textSecondary),
              ),
            ),
            const SizedBox(height: 18),
            _UpdateCard(
              title: '当前版本',
              child: Padding(
                padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                child: Row(
                  children: [
                    Container(
                      width: 36,
                      height: 36,
                      alignment: Alignment.center,
                      decoration: BoxDecoration(
                        color: colors.accentSurface,
                        borderRadius: BorderRadius.circular(
                          MgMetrics.controlRadius,
                        ),
                      ),
                      child: Icon(
                        CupertinoIcons.arrow_down_circle,
                        size: 19,
                        color: colors.accent,
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'M-GCS ${current.version}',
                            style: MgTypography.sans(
                              MgTypography.bodyStrong.copyWith(
                                color: colors.textPrimary,
                              ),
                            ),
                          ),
                          Text(
                            '${controller.target.label} · '
                            '${controller.target.architecture}'
                            '${current.build > 0 ? ' · 构建 ${current.build}' : ''}',
                            style: MgTypography.sans(
                              MgTypography.caption.copyWith(
                                color: colors.textSecondary,
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                    Text(
                      _lastCheckLabel(
                        controller.lastCheckAt,
                        succeeded: controller.lastCheckSucceeded,
                      ),
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textTertiary,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 16),
            _UpdateCard(
              title: '检查设置',
              child: Padding(
                padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                '启动时自动检查',
                                style: MgTypography.sans(
                                  MgTypography.body.copyWith(
                                    color: colors.textPrimary,
                                  ),
                                ),
                              ),
                              Text(
                                hasSource
                                    ? '每 24 小时最多检查一次，不打断飞行操作。'
                                    : '配置更新清单地址后启用。',
                                style: MgTypography.sans(
                                  MgTypography.caption.copyWith(
                                    color: colors.textSecondary,
                                  ),
                                ),
                              ),
                            ],
                          ),
                        ),
                        MacosSwitch(
                          value: hasSource && controller.automaticChecks,
                          onChanged: hasSource
                              ? (value) => unawaited(_setAutoCheck(value))
                              : null,
                          semanticLabel: '启动时自动检查更新',
                        ),
                      ],
                    ),
                    const SizedBox(height: 14),
                    Divider(height: 1, color: colors.hairline),
                    const SizedBox(height: 14),
                    Text(
                      '更新清单地址',
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                    const SizedBox(height: 6),
                    Row(
                      children: [
                        Expanded(
                          child: MacosTextField(
                            key: const ValueKey('update-manifest-url'),
                            controller: _urlController,
                            placeholder: 'https://example.com/mgcs/update.json',
                            onSubmitted: (_) => unawaited(_applyUrl()),
                          ),
                        ),
                        const SizedBox(width: 8),
                        PushButton(
                          key: const ValueKey('update-save-source'),
                          controlSize: ControlSize.regular,
                          secondary: true,
                          onPressed:
                              _starting ||
                                  controller.checking ||
                                  controller.downloading
                              ? null
                              : () => unawaited(_applyUrl()),
                          child: const Text('应用'),
                        ),
                      ],
                    ),
                    const SizedBox(height: 6),
                    Text(
                      '默认从 M-GCS 的 GitHub Releases 获取；也可填写自建 HTTPS 更新源，留空可关闭自动检查。',
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                    const SizedBox(height: 14),
                    Row(
                      children: [
                        PushButton(
                          key: const ValueKey('update-check-now'),
                          controlSize: ControlSize.regular,
                          onPressed:
                              controller.checking ||
                                  _starting ||
                                  controller.downloading
                              ? null
                              : () => unawaited(_check()),
                          child: Text(controller.checking ? '正在检查…' : '检查更新'),
                        ),
                        const SizedBox(width: 10),
                        PushButton(
                          key: const ValueKey('update-restore-default'),
                          controlSize: ControlSize.regular,
                          secondary: true,
                          onPressed:
                              controller.manifestUrl ==
                                      kDefaultUpdateManifestUrl ||
                                  controller.checking ||
                                  _starting ||
                                  controller.downloading
                              ? null
                              : () => unawaited(_restoreDefaultUrl()),
                          child: const Text('恢复默认源'),
                        ),
                        const SizedBox(width: 10),
                        if (controller.checking)
                          const ProgressCircle(radius: 7),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (controller.error != null) ...[
              const SizedBox(height: 14),
              _MessageBanner(
                icon: CupertinoIcons.exclamationmark_triangle,
                color: colors.danger,
                message: controller.error!,
              ),
            ],
            if (update != null) ...[
              const SizedBox(height: 16),
              _UpdateCard(
                title: '版本 ${update.version} 可用',
                child: Padding(
                  padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      if (update.releaseNotes.isNotEmpty)
                        Text(
                          update.releaseNotes,
                          style: MgTypography.sans(
                            MgTypography.body.copyWith(
                              color: colors.textPrimary,
                            ),
                          ),
                        )
                      else
                        Text(
                          '本次更新没有附带发布说明。',
                          style: MgTypography.sans(
                            MgTypography.body.copyWith(
                              color: colors.textSecondary,
                            ),
                          ),
                        ),
                      const SizedBox(height: 12),
                      if (asset == null)
                        const _MessageBanner(
                          icon: CupertinoIcons.desktopcomputer,
                          color: null,
                          message: '当前平台和架构没有对应的更新包。',
                        )
                      else ...[
                        _AssetSummary(asset: asset),
                        const SizedBox(height: 12),
                        if (controller.downloading) ...[
                          LinearProgressIndicator(
                            value: controller.downloadProgress,
                            minHeight: 3,
                          ),
                          const SizedBox(height: 6),
                          Text(
                            controller.downloadProgress == null
                                ? '正在下载更新包…'
                                : '正在下载 · '
                                      '${(controller.downloadProgress! * 100).round()}%',
                            style: MgTypography.sans(
                              MgTypography.caption.copyWith(
                                color: colors.textSecondary,
                              ),
                            ),
                          ),
                        ] else if (downloaded != null)
                          Row(
                            children: [
                              PushButton(
                                key: const ValueKey('update-open-installer'),
                                controlSize: ControlSize.regular,
                                onPressed: () => unawaited(_openDownloaded()),
                                child: const Text('打开安装包'),
                              ),
                              const SizedBox(width: 10),
                              Expanded(
                                child: Text(
                                  downloaded.path,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: MgTypography.sans(
                                    MgTypography.caption.copyWith(
                                      color: colors.textTertiary,
                                    ),
                                  ),
                                ),
                              ),
                            ],
                          )
                        else
                          Align(
                            alignment: Alignment.centerLeft,
                            child: PushButton(
                              key: const ValueKey('update-download'),
                              controlSize: ControlSize.regular,
                              onPressed: controller.checking
                                  ? null
                                  : () => unawaited(_download()),
                              child: const Text('下载更新包'),
                            ),
                          ),
                      ],
                    ],
                  ),
                ),
              ),
            ] else if (!controller.checking &&
                controller.lastCheckSucceeded &&
                controller.error == null) ...[
              const SizedBox(height: 14),
              _MessageBanner(
                icon: CupertinoIcons.check_mark_circled,
                color: colors.success,
                message: '当前已是最新版本。',
              ),
            ] else if (!controller.checking &&
                controller.lastCheckAt != null &&
                !controller.lastCheckSucceeded &&
                controller.error == null) ...[
              const SizedBox(height: 14),
              const _MessageBanner(
                icon: CupertinoIcons.arrow_clockwise,
                color: null,
                message: '上次自动检查未取得更新清单；地面站可以正常使用，也可以稍后重试。',
              ),
            ],
          ],
        );
      },
    );
  }

  static String _lastCheckLabel(DateTime? value, {required bool succeeded}) {
    if (value == null) return '尚未检查';
    if (!succeeded) return '上次检查失败';
    final local = value.toLocal();
    String two(int part) => part.toString().padLeft(2, '0');
    return '上次检查 ${local.month}/${local.day} '
        '${two(local.hour)}:${two(local.minute)}';
  }
}

class _AssetSummary extends StatelessWidget {
  const _AssetSummary({required this.asset});

  final UpdateAsset asset;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Container(
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: colors.workspace,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Row(
        children: [
          Icon(
            CupertinoIcons.archivebox,
            size: 17,
            color: colors.textSecondary,
          ),
          const SizedBox(width: 9),
          Expanded(
            child: Text(
              asset.fileName,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: MgTypography.sans(
                MgTypography.body.copyWith(color: colors.textPrimary),
              ),
            ),
          ),
          if (asset.size case final size?)
            Text(
              _formatBytes(size),
              style: MgTypography.sans(
                MgTypography.caption.copyWith(color: colors.textSecondary),
              ),
            ),
        ],
      ),
    );
  }

  static String _formatBytes(int bytes) {
    if (bytes >= 1024 * 1024 * 1024) {
      return '${(bytes / (1024 * 1024 * 1024)).toStringAsFixed(1)} GB';
    }
    if (bytes >= 1024 * 1024) {
      return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
    }
    if (bytes >= 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '$bytes B';
  }
}

class _MessageBanner extends StatelessWidget {
  const _MessageBanner({
    required this.icon,
    required this.color,
    required this.message,
  });

  final IconData icon;
  final Color? color;
  final String message;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final foreground = color ?? colors.textSecondary;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: foreground.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      ),
      child: Row(
        children: [
          Icon(icon, size: 16, color: foreground),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              message,
              style: MgTypography.sans(
                MgTypography.body.copyWith(color: foreground),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _UpdateCard extends StatelessWidget {
  const _UpdateCard({required this.title, required this.child});

  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 14, 8),
            child: Text(
              title,
              style: MgTypography.sans(
                MgTypography.section.copyWith(color: colors.textPrimary),
              ),
            ),
          ),
          Divider(height: 1, color: colors.hairline),
          child,
        ],
      ),
    );
  }
}
