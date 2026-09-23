import 'package:flutter/material.dart';

import '../../ui/skins/mac_glass/mac_glass_tokens.dart';

final class MissionWaypointDraft {
  const MissionWaypointDraft({
    required this.latitude,
    required this.longitude,
    required this.altitude,
    this.speedMetersPerSecond,
  });

  final double latitude;
  final double longitude;
  final double altitude;
  final double? speedMetersPerSecond;
}

Future<MissionWaypointDraft?> showMissionWaypointDialog(
  BuildContext context, {
  required double latitude,
  required double longitude,
  double altitude = 50,
  double? speedMetersPerSecond,
  bool editing = false,
}) => showDialog<MissionWaypointDraft>(
  context: context,
  builder: (context) => _MissionWaypointDialog(
    latitude: latitude,
    longitude: longitude,
    altitude: altitude,
    speedMetersPerSecond: speedMetersPerSecond,
    editing: editing,
  ),
);

final class _MissionWaypointDialog extends StatefulWidget {
  const _MissionWaypointDialog({
    required this.latitude,
    required this.longitude,
    required this.altitude,
    required this.speedMetersPerSecond,
    required this.editing,
  });

  final double latitude;
  final double longitude;
  final double altitude;
  final double? speedMetersPerSecond;
  final bool editing;

  @override
  State<_MissionWaypointDialog> createState() => _MissionWaypointDialogState();
}

final class _MissionWaypointDialogState extends State<_MissionWaypointDialog> {
  late final TextEditingController _latitudeController;
  late final TextEditingController _longitudeController;
  late final TextEditingController _altitudeController;
  late final TextEditingController _speedController;
  String? _error;

  @override
  void initState() {
    super.initState();
    _latitudeController = TextEditingController(
      text: widget.latitude.toStringAsFixed(6),
    );
    _longitudeController = TextEditingController(
      text: widget.longitude.toStringAsFixed(6),
    );
    _altitudeController = TextEditingController(
      text: widget.altitude.toStringAsFixed(1),
    );
    _speedController = TextEditingController(
      text: widget.speedMetersPerSecond?.toStringAsFixed(1) ?? '',
    );
  }

  @override
  void dispose() {
    _latitudeController.dispose();
    _longitudeController.dispose();
    _altitudeController.dispose();
    _speedController.dispose();
    super.dispose();
  }

  void _submit() {
    final latitude = double.tryParse(_latitudeController.text.trim());
    final longitude = double.tryParse(_longitudeController.text.trim());
    final altitude = double.tryParse(_altitudeController.text.trim());
    final speedText = _speedController.text.trim();
    final speed = speedText.isEmpty ? null : double.tryParse(speedText);

    if (latitude == null || latitude < -90 || latitude > 90) {
      setState(() => _error = '纬度必须在 -90 到 90 之间');
      return;
    }
    if (longitude == null || longitude < -180 || longitude > 180) {
      setState(() => _error = '经度必须在 -180 到 180 之间');
      return;
    }
    if (altitude == null || altitude < 0) {
      setState(() => _error = '高度必须是非负数');
      return;
    }
    if (speedText.isNotEmpty && (speed == null || speed < 0)) {
      setState(() => _error = '速度必须是非负数');
      return;
    }

    Navigator.of(context).pop(
      MissionWaypointDraft(
        latitude: latitude,
        longitude: longitude,
        altitude: altitude,
        speedMetersPerSecond: speed,
      ),
    );
  }

  @override
  Widget build(BuildContext context) => AlertDialog(
    title: Row(
      children: [
        Icon(Icons.add_location_alt_outlined, size: 22),
        SizedBox(width: 8),
        Text(widget.editing ? '编辑航点' : '添加航点'),
      ],
    ),
    content: SizedBox(
      width: 390,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Align(
            alignment: Alignment.centerLeft,
            child: Text(
              widget.editing
                  ? '可调整航点参数，保存后会同步到地图和任务列表。'
                  : '已按地图位置填入坐标，可在写入前微调。',
              style: TextStyle(color: MacGlassTokens.muted, fontSize: 12),
            ),
          ),
          const SizedBox(height: 14),
          Row(
            children: [
              Expanded(
                child: _NumberField(
                  controller: _latitudeController,
                  label: '纬度',
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _NumberField(
                  controller: _longitudeController,
                  label: '经度',
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Expanded(
                child: _NumberField(
                  controller: _altitudeController,
                  label: '高度（米）',
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _NumberField(
                  controller: _speedController,
                  label: '速度（米/秒，可选）',
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          const Align(
            alignment: Alignment.centerLeft,
            child: Text('类型：NAV_WAYPOINT（第一阶段固定）'),
          ),
          if (_error case final error?) ...[
            const SizedBox(height: 10),
            Align(
              alignment: Alignment.centerLeft,
              child: Text(
                error,
                style: const TextStyle(color: MacGlassTokens.red),
              ),
            ),
          ],
        ],
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.of(context).pop(),
        child: const Text('取消'),
      ),
      FilledButton.icon(
        onPressed: _submit,
        icon: const Icon(Icons.check_rounded, size: 18),
        label: Text(widget.editing ? '保存修改' : '添加到任务'),
      ),
    ],
  );
}

final class _NumberField extends StatelessWidget {
  const _NumberField({required this.controller, required this.label});

  final TextEditingController controller;
  final String label;

  @override
  Widget build(BuildContext context) => TextField(
    controller: controller,
    keyboardType: const TextInputType.numberWithOptions(
      decimal: true,
      signed: true,
    ),
    style: MacGlassFonts.platformTextStyle(
      MacGlassTypography.body.copyWith(color: MacGlassTokens.ink),
    ),
    decoration: InputDecoration(labelText: label, isDense: true),
  );
}
