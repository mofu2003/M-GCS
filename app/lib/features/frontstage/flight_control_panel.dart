import 'package:flutter/material.dart';

import '../../application/flight_control_controller.dart';
import '../../application/flight_control_models.dart';

final class FlightControlPanel extends StatelessWidget {
  const FlightControlPanel({required this.controller, super.key});

  final FlightControlController controller;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: controller,
      builder: (context, _) {
        final snapshot = controller.snapshot;
        final status = snapshot.modeStatus;
        final connected = controller.available && status != null;
        return Card(
          color: const Color(0xD91C2230),
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    const Icon(Icons.flight_takeoff_rounded, size: 18),
                    const SizedBox(width: 8),
                    const Expanded(child: Text('飞行控制')),
                    Text(status?.currentMode ?? '未连接'),
                  ],
                ),
                const SizedBox(height: 10),
                DropdownButtonFormField<String>(
                  initialValue:
                      snapshot.modes.any(
                        (item) => item.name == status?.currentMode,
                      )
                      ? status?.currentMode
                      : null,
                  decoration: const InputDecoration(labelText: '飞行模式'),
                  items: snapshot.modes
                      .map(
                        (mode) => DropdownMenuItem(
                          value: mode.name,
                          child: Text(mode.displayName),
                        ),
                      )
                      .toList(growable: false),
                  onChanged: !connected || snapshot.busy
                      ? null
                      : (value) {
                          if (value != null) controller.setMode(value);
                        },
                ),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    FilledButton.icon(
                      onPressed: !connected || snapshot.busy || status.armed
                          ? null
                          : controller.arm,
                      icon: const Icon(Icons.lock_open_rounded, size: 16),
                      label: const Text('解锁'),
                    ),
                    OutlinedButton.icon(
                      onPressed: !connected || snapshot.busy || !status.armed
                          ? null
                          : controller.disarm,
                      icon: const Icon(Icons.lock_rounded, size: 16),
                      label: const Text('上锁'),
                    ),
                    OutlinedButton(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : controller.rtl,
                      child: const Text('返航'),
                    ),
                    OutlinedButton(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : controller.land,
                      child: const Text('降落'),
                    ),
                    OutlinedButton(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : () => controller.takeoff(20),
                      child: const Text('起飞 20 m'),
                    ),
                    OutlinedButton(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : controller.preflightCheck,
                      child: const Text('飞前检查'),
                    ),
                    OutlinedButton(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : () => _showGuidedDialog(context, controller),
                      child: const Text('Guided'),
                    ),
                    FilledButton.tonal(
                      onPressed: !connected || snapshot.busy
                          ? null
                          : () async {
                              final confirmed = await showDialog<bool>(
                                context: context,
                                builder: (context) => AlertDialog(
                                  title: const Text('确认紧急停止'),
                                  content: const Text(
                                    '这会立即向飞控发送紧急停止指令，请确认当前操作安全。',
                                  ),
                                  actions: [
                                    TextButton(
                                      onPressed: () =>
                                          Navigator.pop(context, false),
                                      child: const Text('取消'),
                                    ),
                                    FilledButton(
                                      onPressed: () =>
                                          Navigator.pop(context, true),
                                      child: const Text('确认'),
                                    ),
                                  ],
                                ),
                              );
                              if (confirmed == true) {
                                await controller.emergencyStop();
                              }
                            },
                      child: const Text('紧急停止'),
                    ),
                  ],
                ),
                if (snapshot.lastResult case final result?) ...[
                  const SizedBox(height: 8),
                  Text(
                    result.message,
                    style: TextStyle(
                      color: result.success
                          ? Colors.greenAccent
                          : Colors.redAccent,
                    ),
                  ),
                ],
                if (snapshot.preflight case final preflight?) ...[
                  const SizedBox(height: 6),
                  Text(preflight.message),
                ],
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _showGuidedDialog(
    BuildContext context,
    FlightControlController controller,
  ) async {
    final latitude = TextEditingController();
    final longitude = TextEditingController();
    final altitude = TextEditingController(text: '20');
    final formKey = GlobalKey<FormState>();
    try {
      final request = await showDialog<GuidedPointRequest>(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('Guided 目标点'),
          content: Form(
            key: formKey,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                _numberField(latitude, '纬度', -90, 90),
                _numberField(longitude, '经度', -180, 180),
                _numberField(altitude, '相对高度（米）', 1, 500),
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
                if (!formKey.currentState!.validate()) return;
                Navigator.pop(
                  context,
                  GuidedPointRequest(
                    latitude: double.parse(latitude.text),
                    longitude: double.parse(longitude.text),
                    altitudeMeters: double.parse(altitude.text),
                  ),
                );
              },
              child: const Text('发送'),
            ),
          ],
        ),
      );
      if (request != null) await controller.guidedPoint(request);
    } finally {
      latitude.dispose();
      longitude.dispose();
      altitude.dispose();
    }
  }

  Widget _numberField(
    TextEditingController controller,
    String label,
    double min,
    double max,
  ) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: TextFormField(
        controller: controller,
        keyboardType: const TextInputType.numberWithOptions(
          decimal: true,
          signed: true,
        ),
        decoration: InputDecoration(labelText: label),
        validator: (value) {
          final number = double.tryParse(value?.trim() ?? '');
          return number == null || number < min || number > max
              ? '请输入 $min 到 $max 之间的数值'
              : null;
        },
      ),
    );
  }
}
