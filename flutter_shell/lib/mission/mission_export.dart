import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:macos_ui/macos_ui.dart';
import 'package:path_provider/path_provider.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/aligned_field_row.dart';
import '../widgets/mg_dialog.dart';
import 'mission_document_controller.dart';

/// 任务文件导出默认落盘目录：应用支持目录下的 `missions/`。
///
/// 与机载日志、机载文件缓存同一根（`getApplicationSupportDirectory`）。这里不引入
/// 文件选择器插件，先把文件放到可预期位置，并在界面显示完整路径供用户修改。
Future<Directory> defaultMissionExportDirectory() async {
  final base = await getApplicationSupportDirectory();
  return Directory('${base.path}${Platform.pathSeparator}missions');
}

/// 一种任务交换格式的展示元数据。
final class MissionExportFormat {
  const MissionExportFormat({
    required this.id,
    required this.label,
    required this.extension,
    required this.description,
  });

  /// native `mg_mission_doc_export_format` 接受的格式名。
  final String id;
  final String label;
  final String extension;
  final String description;
}

/// 支持的交换格式：`waypoints`（MP/QGC 通用文本）与 `plan`（QGroundControl JSON）。
const List<MissionExportFormat> missionExportFormats = [
  MissionExportFormat(
    id: 'waypoints',
    label: 'Waypoints',
    extension: '.waypoints',
    description: 'Mission Planner / QGC 通用的 QGC WPL 110 文本，导出编译后的任务项。',
  ),
  MissionExportFormat(
    id: 'plan',
    label: 'QGC .plan',
    extension: '.plan',
    description: 'QGroundControl JSON，保留测绘等复杂项的设计意图。',
  ),
];

/// 一次导出的结果：写到哪个文件、写了多少条、以及可继续但可能丢字段的提示。
final class MissionExportResult {
  const MissionExportResult({
    required this.path,
    required this.itemCount,
    required this.warnings,
  });

  final String path;
  final int itemCount;
  final List<String> warnings;
}

/// 打开任务导出面板。成功返回 [MissionExportResult]；取消或关闭返回 null。
Future<MissionExportResult?> showMissionExportSheet({
  required BuildContext context,
  required MissionDocumentController controller,
  Future<Directory> Function()? directoryProvider,
}) {
  return showMgSheet<MissionExportResult>(
    context: context,
    builder: (sheetContext) => _MissionExportSheet(
      controller: controller,
      directoryProvider: directoryProvider ?? defaultMissionExportDirectory,
    ),
  );
}

/// 派生默认文件名：`mission-<yyyyMMdd-HHmmss><扩展名>`。
String defaultMissionExportFileName(String extension, {DateTime? now}) {
  final stamp = (now ?? DateTime.now());
  String two(int value) => value.toString().padLeft(2, '0');
  return 'mission-${stamp.year}${two(stamp.month)}${two(stamp.day)}'
      '-${two(stamp.hour)}${two(stamp.minute)}${two(stamp.second)}$extension';
}

class _MissionExportSheet extends StatefulWidget {
  const _MissionExportSheet({
    required this.controller,
    required this.directoryProvider,
  });

  final MissionDocumentController controller;
  final Future<Directory> Function() directoryProvider;

  @override
  State<_MissionExportSheet> createState() => _MissionExportSheetState();
}

class _MissionExportSheetState extends State<_MissionExportSheet> {
  final TextEditingController _path = TextEditingController();
  final MacosTabController _formatTabs = MacosTabController(
    length: missionExportFormats.length,
  );
  bool _busy = false;
  String? _error;
  bool _pathSeeded = false;

  int get _formatIndex => _formatTabs.index;

  MissionExportFormat get _format => missionExportFormats[_formatIndex];

  @override
  void initState() {
    super.initState();
    _formatTabs.addListener(_handleFormatChanged);
    _seedPath();
  }

  @override
  void dispose() {
    _formatTabs.removeListener(_handleFormatChanged);
    _formatTabs.dispose();
    _path.dispose();
    super.dispose();
  }

  Future<void> _seedPath() async {
    Directory directory;
    try {
      directory = await widget.directoryProvider();
    } catch (_) {
      directory = Directory.current;
    }
    if (!mounted) return;
    final separator = Platform.pathSeparator;
    final name = defaultMissionExportFileName(_format.extension);
    setState(() {
      _path.text = '${directory.path}$separator$name';
      _pathSeeded = true;
    });
  }

  void _handleFormatChanged() {
    if (!mounted) return;
    // 用户还没改过路径时，跟随格式切换更新扩展名；改过就保留用户输入。
    final previous =
        missionExportFormats[_formatTabs.previousIndex].extension;
    if (_pathSeeded && _path.text.endsWith(previous)) {
      _path.text =
          '${_path.text.substring(0, _path.text.length - previous.length)}'
          '${_format.extension}';
    }
    setState(() => _error = null);
  }

  String _resolvePath() {
    final trimmed = _path.text.trim();
    if (trimmed.isEmpty) return '';
    final name = trimmed.split(RegExp(r'[/\\]')).last;
    if (!name.contains('.')) return '$trimmed${_format.extension}';
    return trimmed;
  }

  Future<void> _export() async {
    final path = _resolvePath();
    if (path.isEmpty) {
      setState(() => _error = '请填写导出路径');
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final export = await widget.controller.exportInterchange(_format.id);
      final file = File(path);
      await file.parent.create(recursive: true);
      await file.writeAsString(export.text, flush: true);
      if (!mounted) return;
      Navigator.of(context).pop(
        MissionExportResult(
          path: path,
          itemCount: export.itemCount,
          warnings: export.warnings,
        ),
      );
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _error = '导出失败：$error';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final body = MgTypography.sans(
      MgTypography.body.copyWith(color: colors.textPrimary),
    );
    return Center(
      child: MacosSheet(
        insetPadding: EdgeInsets.zero,
        child: SizedBox(
          width: 460,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 10, 14, 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisSize: MainAxisSize.min,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        '导出任务文件',
                        style: MacosTheme.of(context).typography.title3,
                      ),
                    ),
                    MacosIconButton(
                      icon: const MacosIcon(CupertinoIcons.xmark, size: 13),
                      semanticLabel: '关闭',
                      onPressed: _busy
                          ? null
                          : () => Navigator.of(context).pop(),
                    ),
                  ],
                ),
                const SizedBox(height: 4),
                Text(
                  '导出当前任务文档。由用户在航线规划里确认 Home 并上传飞控，'
                  '导出本身不改变飞控状态。',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
                const SizedBox(height: 10),
                Center(
                  child: MacosSegmentedControl(
                    controller: _formatTabs,
                    tabs: [
                      for (final format in missionExportFormats)
                        MacosTab(label: format.label),
                    ],
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  _format.description,
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
                const SizedBox(height: 10),
                AlignedFieldRow(
                  label: '导出路径',
                  labelWidth: 60,
                  child: MacosTextField(
                    controller: _path,
                    enabled: !_busy,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 4,
                    ),
                    style: body,
                    onSubmitted: (_) => _busy ? null : _export(),
                  ),
                ),
                if (_error != null) ...[
                  const SizedBox(height: 8),
                  Text(
                    _error!,
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(color: colors.danger),
                    ),
                  ),
                ],
                const SizedBox(height: 12),
                Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    PushButton(
                      controlSize: ControlSize.regular,
                      secondary: true,
                      onPressed: _busy
                          ? null
                          : () => Navigator.of(context).pop(),
                      child: const Text('取消'),
                    ),
                    const SizedBox(width: 8),
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: _busy ? null : _export,
                      child: _busy
                          ? const ProgressCircle(radius: 7)
                          : const Text('导出'),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
