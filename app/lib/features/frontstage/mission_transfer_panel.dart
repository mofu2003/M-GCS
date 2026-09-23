import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/material.dart';
import 'package:file_selector/file_selector.dart';
import 'dart:convert';
import 'dart:io';

import '../../application/connection_session_backend.dart';
import '../../application/mission_models.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';
import '../mission/mission_editor_controller.dart';

final class MissionTransferPanel extends StatefulWidget {
  const MissionTransferPanel({
    this.backend,
    this.controller,
    this.onItemsChanged,
    super.key,
  });

  final MissionSessionBackend? backend;

  /// Optional shared editor state. When supplied, the panel becomes a pure UI
  /// adapter and does not own or dispose the controller.
  final MissionEditorController? controller;
  final ValueChanged<List<MissionItemSnapshot>>? onItemsChanged;

  @override
  State<MissionTransferPanel> createState() => _MissionTransferPanelState();
}

final class _MissionTransferPanelState extends State<MissionTransferPanel> {
  late MissionEditorController _controller;

  @override
  void initState() {
    super.initState();
    _controller =
        widget.controller ?? MissionEditorController(backend: widget.backend);
    _controller.addListener(_notifyItemsChanged);
  }

  @override
  void didUpdateWidget(covariant MissionTransferPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller ||
        (widget.controller == null && oldWidget.backend != widget.backend)) {
      _controller.removeListener(_notifyItemsChanged);
      if (oldWidget.controller == null) {
        _controller.dispose();
      }
      _controller =
          widget.controller ?? MissionEditorController(backend: widget.backend);
      _controller.addListener(_notifyItemsChanged);
    }
  }

  @override
  void dispose() {
    _controller.removeListener(_notifyItemsChanged);
    if (widget.controller == null) {
      _controller.dispose();
    }
    super.dispose();
  }

  void _notifyItemsChanged() {
    widget.onItemsChanged?.call(_controller.items);
  }

  @override
  Widget build(BuildContext context) => Material(
    color: Colors.transparent,
    child: ConstrainedBox(
      constraints: const BoxConstraints(maxWidth: 820, maxHeight: 720),
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xfffbfbfd).withValues(alpha: 0.96),
          border: Border.all(color: const Color(0xffd1d1d6)),
          borderRadius: BorderRadius.circular(18),
          boxShadow: [MacGlassTokens.floatingShadow()],
        ),
        child: ListenableBuilder(
          listenable: _controller,
          builder: (context, _) => Padding(
            padding: const EdgeInsets.fromLTRB(20, 18, 20, 14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    const Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            '航点任务',
                            style: TextStyle(
                              fontSize: 20,
                              fontWeight: FontWeight.w800,
                            ),
                          ),
                          SizedBox(height: 3),
                          Text(
                            '先完成任务编排与飞控同步，视觉页面后续可独立重构',
                            style: TextStyle(
                              color: MacGlassTokens.muted,
                              fontSize: 12,
                            ),
                          ),
                        ],
                      ),
                    ),
                    _MissionActionButton(
                      key: const ValueKey('mg-mission-download'),
                      icon: Icons.download_rounded,
                      label: '读取',
                      onPressed: _controller.busy ? null : _controller.download,
                    ),
                    const SizedBox(width: 8),
                    _MissionActionButton(
                      key: const ValueKey('mg-mission-upload'),
                      icon: Icons.upload_rounded,
                      label: '写入',
                      primary: true,
                      onPressed: _controller.busy ? null : _controller.upload,
                    ),
                    const SizedBox(width: 8),
                    _MissionActionButton(
                      key: const ValueKey('mg-mission-clear'),
                      icon: Icons.delete_sweep_outlined,
                      label: '清空',
                      onPressed: _controller.busy ? null : _controller.clear,
                    ),
                    const SizedBox(width: 8),
                    PopupMenuButton<String>(
                      tooltip: '任务文件',
                      onSelected: _handleFileAction,
                      itemBuilder: (_) => const [
                        PopupMenuItem(
                          value: 'import_json',
                          child: Text('导入 JSON'),
                        ),
                        PopupMenuItem(
                          value: 'export_json',
                          child: Text('导出 JSON'),
                        ),
                        PopupMenuItem(
                          value: 'import_csv',
                          child: Text('导入 CSV'),
                        ),
                        PopupMenuItem(
                          value: 'export_csv',
                          child: Text('导出 CSV'),
                        ),
                      ],
                      icon: const Icon(Icons.folder_copy_outlined, size: 19),
                    ),
                    const SizedBox(width: 4),
                    PopupMenuButton<String>(
                      tooltip: '规划工具',
                      onSelected: (value) {
                        if (value == 'survey') _showSurveyDialog();
                        if (value == 'corridor') _showCorridorDialog();
                      },
                      itemBuilder: (_) => const [
                        PopupMenuItem(value: 'survey', child: Text('生成测绘航线')),
                        PopupMenuItem(value: 'corridor', child: Text('生成走廊航线')),
                      ],
                      icon: const Icon(Icons.route_outlined, size: 19),
                    ),
                  ],
                ),
                const SizedBox(height: 14),
                Row(
                  children: [
                    Text(
                      '${_controller.items.length} 个任务项',
                      style: const TextStyle(
                        fontWeight: FontWeight.w700,
                        fontSize: 12,
                      ),
                    ),
                    const SizedBox(width: 12),
                    if (_controller.items.isNotEmpty)
                      Text(
                        '当前 ${_controller.currentIndex + 1}',
                        style: const TextStyle(
                          color: MacGlassTokens.muted,
                          fontSize: 12,
                        ),
                      ),
                    const Spacer(),
                    TextButton.icon(
                      key: const ValueKey('mg-mission-add-waypoint'),
                      onPressed: _controller.busy
                          ? null
                          : _controller.addWaypoint,
                      icon: const Icon(Icons.add_rounded, size: 17),
                      label: const Text('添加航点'),
                    ),
                    TextButton.icon(
                      key: const ValueKey('mg-mission-start'),
                      onPressed: _controller.busy || _controller.items.isEmpty
                          ? null
                          : _controller.start,
                      icon: const Icon(Icons.play_arrow_rounded, size: 17),
                      label: const Text('开始任务'),
                    ),
                  ],
                ),
                if (_controller.progress.active ||
                    _controller.progress.progressPercent > 0) ...[
                  const SizedBox(height: 10),
                  _MissionProgressView(progress: _controller.progress),
                ],
                if (!_controller.validation.isValid &&
                    _controller.items.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 6),
                    child: Text(
                      '校验：${_controller.validation.issues.first.message}',
                      style: const TextStyle(
                        color: Colors.deepOrange,
                        fontSize: 12,
                      ),
                    ),
                  ),
                const SizedBox(height: 6),
                Expanded(
                  child: _controller.items.isEmpty
                      ? const Center(
                          child: Text(
                            '暂无航点，请读取飞控任务或添加航点',
                            style: TextStyle(color: MacGlassTokens.muted),
                          ),
                        )
                      : ListView.separated(
                          key: const ValueKey('mg-mission-item-list'),
                          padding: const EdgeInsets.only(bottom: 8),
                          itemCount: _controller.items.length,
                          separatorBuilder: (_, _) => const SizedBox(height: 8),
                          itemBuilder: (context, index) => _MissionItemRow(
                            item: _controller.items[index],
                            isCurrent: index == _controller.currentIndex,
                            onDelete: () => _controller.removeWaypoint(index),
                            onMoveUp: index == 0
                                ? null
                                : () => _controller.moveWaypoint(index, -1),
                            onMoveDown: index == _controller.items.length - 1
                                ? null
                                : () => _controller.moveWaypoint(index, 1),
                            onSetCurrent: () => _controller.setCurrent(index),
                            onUpdate: (lat, lon, alt) =>
                                _controller.updateWaypoint(
                                  index,
                                  latitude: lat,
                                  longitude: lon,
                                  altitude: alt,
                                ),
                          ),
                        ),
                ),
                const SizedBox(height: 8),
                Text(
                  _controller.status,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(
                    color: MacGlassTokens.muted,
                    fontSize: 12,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    ),
  );

  Future<void> _handleFileAction(String action) async {
    try {
      switch (action) {
        case 'import_json':
          final file = await openFile(
            acceptedTypeGroups: [
              const XTypeGroup(label: 'Mission JSON', extensions: ['json']),
            ],
          );
          if (file != null) _controller.importJson(await file.readAsString());
        case 'export_json':
          await _save('mission.json', _controller.exportJson(), 'json');
        case 'import_csv':
          final file = await openFile(
            acceptedTypeGroups: [
              const XTypeGroup(label: 'Mission CSV', extensions: ['csv']),
            ],
          );
          if (file != null) _controller.importCsv(await file.readAsString());
        case 'export_csv':
          await _save('mission.csv', _controller.exportCsv(), 'csv');
      }
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('任务文件操作失败：$error')));
    }
  }

  Future<void> _save(
    String suggestedName,
    String content,
    String extension,
  ) async {
    final location = await getSaveLocation(
      suggestedName: suggestedName,
      acceptedTypeGroups: [
        XTypeGroup(label: extension.toUpperCase(), extensions: [extension]),
      ],
    );
    if (location == null) return;
    await File(
      location.path,
    ).writeAsString(content, encoding: utf8, flush: true);
    if (!mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text('已保存任务文件：${location.path}')));
  }

  Future<void> _showSurveyDialog() async {
    final values = await _showPlanningFields(
      title: '生成测绘航线',
      fields: const [
        ('中心纬度', '31.2304'),
        ('中心经度', '121.4737'),
        ('区域宽度（米）', '100'),
        ('区域长度（米）', '100'),
        ('飞行高度（米）', '50'),
      ],
    );
    if (values == null) return;
    await _controller.generateSurvey(
      SurveyConfig(
        centerLatitude: values[0],
        centerLongitude: values[1],
        widthMeters: values[2],
        lengthMeters: values[3],
        altitudeMeters: values[4],
      ),
    );
  }

  Future<void> _showCorridorDialog() async {
    final values = await _showPlanningFields(
      title: '生成走廊航线',
      fields: const [
        ('起点纬度', '31.2304'),
        ('起点经度', '121.4737'),
        ('终点纬度', '31.2350'),
        ('终点经度', '121.4800'),
        ('飞行高度（米）', '50'),
      ],
    );
    if (values == null) return;
    await _controller.generateCorridor(
      CorridorConfig(
        startLatitude: values[0],
        startLongitude: values[1],
        endLatitude: values[2],
        endLongitude: values[3],
        altitudeMeters: values[4],
      ),
    );
  }

  Future<List<double>?> _showPlanningFields({
    required String title,
    required List<(String, String)> fields,
  }) async {
    final controllers = [
      for (final field in fields) TextEditingController(text: field.$2),
    ];
    final result = await showDialog<List<double>>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(title),
        content: SizedBox(
          width: 360,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              for (var i = 0; i < fields.length; i++)
                Padding(
                  padding: const EdgeInsets.only(bottom: 8),
                  child: TextField(
                    controller: controllers[i],
                    keyboardType: const TextInputType.numberWithOptions(
                      decimal: true,
                    ),
                    decoration: InputDecoration(labelText: fields[i].$1),
                  ),
                ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () {
              final values = controllers
                  .map((item) => double.tryParse(item.text.trim()))
                  .toList();
              if (values.any((value) => value == null || !value.isFinite)) {
                return;
              }
              Navigator.pop(context, values.cast<double>());
            },
            child: const Text('生成'),
          ),
        ],
      ),
    );
    for (final controller in controllers) {
      controller.dispose();
    }
    return result;
  }
}

final class _MissionProgressView extends StatelessWidget {
  const _MissionProgressView({required this.progress});

  final MissionProgressSnapshot progress;

  @override
  Widget build(BuildContext context) {
    final transferred = progress.state == MissionTransferState.downloading
        ? progress.received
        : progress.sent;
    final percent = progress.progressPercent > 0
        ? progress.progressPercent
        : progress.total > 0
        ? ((transferred / progress.total) * 100).round().clamp(0, 100)
        : 0;
    final state = switch (progress.state) {
      MissionTransferState.downloading => '正在读取任务',
      MissionTransferState.uploading => '正在写入任务',
      MissionTransferState.waitingAck => '等待飞控确认',
      MissionTransferState.idle => '任务传输完成',
    };
    return DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xffeef4ff),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(12, 8, 12, 9),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Text(
                  state,
                  style: const TextStyle(
                    color: Color(0xff2456a6),
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                const Spacer(),
                Text(
                  progress.total > 0
                      ? '$transferred / ${progress.total}  ·  $percent%'
                      : '$percent%',
                  style: const TextStyle(
                    color: Color(0xff2456a6),
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 6),
            ClipRRect(
              borderRadius: BorderRadius.circular(5),
              child: LinearProgressIndicator(
                minHeight: 5,
                value: percent / 100,
                backgroundColor: const Color(0xffdbe7fa),
                valueColor: const AlwaysStoppedAnimation(Color(0xff3478d4)),
              ),
            ),
            if (progress.error.isNotEmpty) ...[
              const SizedBox(height: 4),
              Text(
                progress.error,
                style: const TextStyle(color: Colors.deepOrange, fontSize: 11),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

final class _MissionItemRow extends StatelessWidget {
  const _MissionItemRow({
    required this.item,
    required this.isCurrent,
    required this.onDelete,
    required this.onMoveUp,
    required this.onMoveDown,
    required this.onSetCurrent,
    required this.onUpdate,
  });

  final MissionItemSnapshot item;
  final bool isCurrent;
  final VoidCallback onDelete;
  final VoidCallback? onMoveUp;
  final VoidCallback? onMoveDown;
  final VoidCallback onSetCurrent;
  final void Function(double? lat, double? lon, double? alt) onUpdate;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: isCurrent ? const Color(0xffe8f1ff) : const Color(0xfff2f2f7),
      borderRadius: BorderRadius.circular(12),
      border: Border.all(color: const Color(0xffdedee3)),
    ),
    child: Padding(
      padding: const EdgeInsets.fromLTRB(12, 10, 8, 10),
      child: Row(
        children: [
          SizedBox(
            width: 58,
            child: Column(
              children: [
                Text(
                  '${item.sequence + 1}',
                  textAlign: TextAlign.center,
                  style: const TextStyle(fontWeight: FontWeight.w800),
                ),
                if (isCurrent)
                  const Text(
                    '当前',
                    style: TextStyle(fontSize: 10, color: Colors.blue),
                  ),
              ],
            ),
          ),
          SizedBox(
            width: 138,
            child: Text(
              item.commandName,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.keyboard_arrow_up_rounded, size: 18),
            tooltip: '上移',
            onPressed: onMoveUp,
          ),
          IconButton(
            icon: const Icon(Icons.keyboard_arrow_down_rounded, size: 18),
            tooltip: '下移',
            onPressed: onMoveDown,
          ),
          IconButton(
            icon: const Icon(Icons.radio_button_checked_rounded, size: 18),
            tooltip: '设为当前航点',
            onPressed: onSetCurrent,
          ),
          Expanded(
            child: _MissionNumberField(
              value: item.latitude,
              hint: '纬度',
              onSubmitted: (v) => onUpdate(v, null, null),
            ),
          ),
          const SizedBox(width: 6),
          Expanded(
            child: _MissionNumberField(
              value: item.longitude,
              hint: '经度',
              onSubmitted: (v) => onUpdate(null, v, null),
            ),
          ),
          const SizedBox(width: 6),
          SizedBox(
            width: 92,
            child: _MissionNumberField(
              value: item.altitudeMeters,
              hint: '高度 m',
              onSubmitted: (v) => onUpdate(null, null, v),
            ),
          ),
          IconButton(
            key: ValueKey('mg-mission-delete-${item.sequence}'),
            tooltip: '删除航点',
            onPressed: onDelete,
            icon: const Icon(Icons.delete_outline_rounded, size: 18),
          ),
        ],
      ),
    ),
  );
}

final class _MissionNumberField extends StatefulWidget {
  const _MissionNumberField({
    required this.value,
    required this.hint,
    required this.onSubmitted,
  });
  final double? value;
  final String hint;
  final ValueChanged<double?> onSubmitted;

  @override
  State<_MissionNumberField> createState() => _MissionNumberFieldState();
}

final class _MissionNumberFieldState extends State<_MissionNumberField> {
  late final TextEditingController _controller;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(
      text: widget.value?.toStringAsFixed(6) ?? '',
    );
  }

  @override
  void didUpdateWidget(covariant _MissionNumberField oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.value != widget.value && !_controller.selection.isValid) {
      _controller.text = widget.value?.toStringAsFixed(6) ?? '';
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => TextField(
    controller: _controller,
    keyboardType: const TextInputType.numberWithOptions(
      decimal: true,
      signed: true,
    ),
    style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
    decoration: InputDecoration(
      hintText: widget.hint,
      isDense: true,
      border: InputBorder.none,
      contentPadding: const EdgeInsets.symmetric(horizontal: 6, vertical: 8),
    ),
    onSubmitted: (text) => widget.onSubmitted(double.tryParse(text.trim())),
  );
}

final class _MissionActionButton extends StatelessWidget {
  const _MissionActionButton({
    required this.icon,
    required this.label,
    required this.onPressed,
    this.primary = false,
    super.key,
  });
  final IconData icon;
  final String label;
  final VoidCallback? onPressed;
  final bool primary;

  @override
  Widget build(BuildContext context) => cupertino.CupertinoButton(
    onPressed: onPressed,
    color: primary ? MacGlassTokens.black : const Color(0xfff2f2f7),
    disabledColor: const Color(0xffe5e5ea),
    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
    minimumSize: const Size(0, 36),
    borderRadius: BorderRadius.circular(9),
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 16),
        const SizedBox(width: 5),
        Text(
          label,
          style: TextStyle(
            color: primary ? Colors.white : MacGlassTokens.ink,
            fontSize: 12,
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    ),
  );
}
