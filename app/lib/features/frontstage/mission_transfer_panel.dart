import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/material.dart';

import '../../application/connection_session_backend.dart';
import '../../application/mission_models.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';

final class MissionTransferPanel extends StatefulWidget {
  const MissionTransferPanel({required this.backend, super.key});

  final MissionSessionBackend? backend;

  @override
  State<MissionTransferPanel> createState() => _MissionTransferPanelState();
}

final class _MissionTransferPanelState extends State<MissionTransferPanel> {
  List<MissionItemSnapshot> _items = const [];
  bool _busy = false;
  String _status = '尚未读取任务';

  Future<void> _download() async {
    final backend = widget.backend;
    if (backend == null) {
      setState(() => _status = '当前后端不支持任务传输');
      return;
    }
    setState(() {
      _busy = true;
      _status = '正在从飞控读取航点…';
    });
    try {
      final plan = await backend.downloadMission();
      if (!mounted) return;
      setState(() {
        _items = plan.items;
        _status = '已读取 ${plan.items.length} 个任务项';
      });
    } on Object catch (error) {
      if (mounted) setState(() => _status = '读取失败：$error');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _upload() async {
    final backend = widget.backend;
    if (backend == null) {
      setState(() => _status = '当前后端不支持任务传输');
      return;
    }
    if (_items.isEmpty) {
      setState(() => _status = '请先添加至少一个航点');
      return;
    }
    setState(() {
      _busy = true;
      _status = '正在写入飞控…';
    });
    try {
      final result = await backend.uploadMission(_items);
      if (mounted) setState(() => _status = result.message);
    } on Object catch (error) {
      if (mounted) setState(() => _status = '写入失败：$error');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  void _addWaypoint() {
    final previous = _items.isEmpty ? null : _items.last;
    final item = MissionItemSnapshot(
      sequence: _items.length,
      command: 16,
      commandName: 'NAV_WAYPOINT',
      frame: 3,
      frameName: 'GLOBAL_RELATIVE_ALT',
      autoContinue: true,
      latitude: previous?.latitude ?? 0,
      longitude: previous?.longitude ?? 0,
      altitudeMeters: previous?.altitudeMeters ?? 50,
    );
    setState(() {
      _items = [..._items, item];
      _status = '已添加本地航点，写入飞控后才会生效';
    });
  }

  void _removeWaypoint(int index) {
    setState(() {
      _items = [
        for (var i = 0; i < _items.length; i++)
          if (i != index) _items[i].copyWith(sequence: i),
      ];
      _status = '已从本地任务移除航点';
    });
  }

  void _update(int index, {double? lat, double? lon, double? alt}) {
    final item = _items[index];
    final updated = item.copyWith(
      latitude: lat,
      longitude: lon,
      altitudeMeters: alt,
    );
    setState(() => _items = [..._items]..[index] = updated);
  }

  @override
  Widget build(BuildContext context) => Material(
    color: Colors.transparent,
    child: ConstrainedBox(
      constraints: const BoxConstraints(maxWidth: 720, maxHeight: 690),
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xfffbfbfd).withValues(alpha: 0.96),
          border: Border.all(color: const Color(0xffd1d1d6)),
          borderRadius: BorderRadius.circular(18),
          boxShadow: [MacGlassTokens.floatingShadow()],
        ),
        child: Padding(
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
                          '读取、编辑并写入飞控任务',
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
                    onPressed: _busy ? null : _download,
                  ),
                  const SizedBox(width: 8),
                  _MissionActionButton(
                    key: const ValueKey('mg-mission-upload'),
                    icon: Icons.upload_rounded,
                    label: '写入',
                    primary: true,
                    onPressed: _busy ? null : _upload,
                  ),
                ],
              ),
              const SizedBox(height: 14),
              Row(
                children: [
                  Text(
                    '${_items.length} 个任务项',
                    style: const TextStyle(
                      fontWeight: FontWeight.w700,
                      fontSize: 12,
                    ),
                  ),
                  const Spacer(),
                  TextButton.icon(
                    key: const ValueKey('mg-mission-add-waypoint'),
                    onPressed: _busy ? null : _addWaypoint,
                    icon: const Icon(Icons.add_rounded, size: 17),
                    label: const Text('添加航点'),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              Expanded(
                child: _items.isEmpty
                    ? const Center(
                        child: Text(
                          '暂无航点，请读取飞控任务或添加航点',
                          style: TextStyle(color: MacGlassTokens.muted),
                        ),
                      )
                    : ListView.separated(
                        key: const ValueKey('mg-mission-item-list'),
                        padding: const EdgeInsets.only(bottom: 8),
                        itemCount: _items.length,
                        separatorBuilder: (_, _) => const SizedBox(height: 8),
                        itemBuilder: (context, index) => _MissionItemRow(
                          item: _items[index],
                          onDelete: () => _removeWaypoint(index),
                          onUpdate: (lat, lon, alt) =>
                              _update(index, lat: lat, lon: lon, alt: alt),
                        ),
                      ),
              ),
              const SizedBox(height: 8),
              Text(
                _status,
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
  );
}

final class _MissionItemRow extends StatelessWidget {
  const _MissionItemRow({
    required this.item,
    required this.onDelete,
    required this.onUpdate,
  });

  final MissionItemSnapshot item;
  final VoidCallback onDelete;
  final void Function(double? lat, double? lon, double? alt) onUpdate;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: const Color(0xfff2f2f7),
      borderRadius: BorderRadius.circular(12),
      border: Border.all(color: const Color(0xffdedee3)),
    ),
    child: Padding(
      padding: const EdgeInsets.fromLTRB(12, 10, 8, 10),
      child: Row(
        children: [
          SizedBox(
            width: 42,
            child: Text(
              '${item.sequence + 1}',
              textAlign: TextAlign.center,
              style: const TextStyle(fontWeight: FontWeight.w800),
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
