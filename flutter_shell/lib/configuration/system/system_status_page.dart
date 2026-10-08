import 'package:flutter/cupertino.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import 'widgets/system_overview_card.dart';
import 'widgets/system_status_cards.dart';

/// 飞控系统概览、传感器健康与实时遥测状态页。
class SystemStatusPage extends StatelessWidget {
  const SystemStatusPage({super.key, required this.controller});

  final ConnectionController controller;

  static const _sensorLabels = <String, (String, IconData)>{
    'gyro': ('陀螺仪', CupertinoIcons.rotate_right),
    'accel': ('加速度计', CupertinoIcons.arrow_up_down),
    'mag': ('磁力计', CupertinoIcons.compass),
    'baro': ('气压计', CupertinoIcons.speedometer),
    'diff_pressure': ('空速计', CupertinoIcons.wind),
    'gps': ('GPS', CupertinoIcons.location),
    'rc': ('遥控接收机', CupertinoIcons.antenna_radiowaves_left_right),
    'battery': ('电池监测', CupertinoIcons.battery_100),
    'motors': ('电机输出', CupertinoIcons.circle_grid_3x3),
    'optical_flow': ('光流传感器', CupertinoIcons.viewfinder),
    'proximity': ('近距传感器', CupertinoIcons.dot_radiowaves_left_right),
  };

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: controller.telemetryChanges,
    builder: (context, _) {
      final connected = controller.isConnected;
      final firmware = controller.firmwareInfo;
      final identity = controller.vehicleIdentity;
      final version = controller.firmwareVersionInfo;
      final health = controller.sensorHealth;
      final items = (health?['items'] as List<Object?>? ?? const [])
          .whereType<Map<String, Object?>>();
      final visibleSensors = items
          .where((item) => _sensorLabels.containsKey(item['key']))
          .toList();
      final summary = health?['summary'] as Map<String, Object?>?;
      final versionReceived = version?['received'] == true;
      final rawSystemId = identity?['system_id'] as int?;
      final versionLabel = versionReceived
          ? '${version!['major']}.${version['minor']}.${version['patch']}'
          : '尚未上报';
      final firmwareLabel = _displayFirmware(firmware?['firmware'] as String?);
      final vehicleLabel = _displayVehicle(
        firmware?['vehicle_type'] as String?,
      );

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
            SystemOverviewCard(
              connected: connected,
              connectionState: _connectionState(controller),
              firmware: firmwareLabel,
              vehicle: vehicleLabel,
              systemId: rawSystemId != null && rawSystemId > 0
                  ? rawSystemId
                  : null,
              firmwareVersion: versionLabel,
              background: context.mgColors.surfaceMuted,
              borderColor: context.mgColors.divider,
            ),
            const SizedBox(height: 22),
            SystemSectionTitle(
              title: '传感器与子系统',
              trailing: HealthSummary(summary: summary),
            ),
            const SizedBox(height: 10),
            if (!connected || health == null)
              SensorDataPendingCard(
                connected: connected,
                borderColor: context.mgColors.divider,
                background: context.mgColors.surface,
              )
            else
              LayoutBuilder(
                builder: (context, constraints) {
                  final columns = constraints.maxWidth >= 960
                      ? 3
                      : constraints.maxWidth >= 620
                      ? 2
                      : 1;
                  final cardWidth =
                      (constraints.maxWidth - (columns - 1) * 12) / columns;
                  return Wrap(
                    spacing: 12,
                    runSpacing: 12,
                    children: [
                      for (final item in visibleSensors)
                        SizedBox(
                          width: cardWidth,
                          child: SensorStatusCard(
                            sensor: _sensorLabels[item['key']]!.$1,
                            icon: _sensorLabels[item['key']]!.$2,
                            status: item['status'] as String? ?? 'unknown',
                            borderColor: context.mgColors.divider,
                            background: context.mgColors.surface,
                          ),
                        ),
                    ],
                  );
                },
              ),
            const SizedBox(height: 22),
            SystemSectionTitle(title: '实时状态'),
            const SizedBox(height: 10),
            LayoutBuilder(
              builder: (context, constraints) {
                final columns = constraints.maxWidth >= 900 ? 3 : 2;
                final cardWidth =
                    (constraints.maxWidth - (columns - 1) * 12) / columns;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    SizedBox(
                      width: cardWidth,
                      child: TelemetryStatusCard(
                        label: 'GPS 定位',
                        icon: CupertinoIcons.location,
                        value: _gpsFixLabel(controller.gpsFixType),
                        detail: controller.satellitesVisible == null
                            ? '卫星数：等待数据'
                            : '卫星数：${controller.satellitesVisible}',
                        fresh: controller.gpsFixType != null,
                        borderColor: context.mgColors.divider,
                        background: context.mgColors.surface,
                      ),
                    ),
                    SizedBox(
                      width: cardWidth,
                      child: TelemetryStatusCard(
                        label: '电池',
                        icon: CupertinoIcons.battery_100,
                        value: controller.batteryRemainingPercent == null
                            ? '等待数据'
                            : '${controller.batteryRemainingPercent}%',
                        detail: controller.batteryVoltage == null
                            ? '电压：等待数据'
                            : '电压：${controller.batteryVoltage!.toStringAsFixed(2)} V',
                        fresh:
                            controller.batteryRemainingPercent != null ||
                            controller.batteryVoltage != null,
                        borderColor: context.mgColors.divider,
                        background: context.mgColors.surface,
                      ),
                    ),
                    SizedBox(
                      width: cardWidth,
                      child: TelemetryStatusCard(
                        label: '航向',
                        icon: CupertinoIcons.compass,
                        value: controller.headingDegrees == null
                            ? '等待数据'
                            : '${controller.headingDegrees!.toStringAsFixed(0)}°',
                        detail: '来自飞控航向遥测',
                        fresh: controller.headingDegrees != null,
                        borderColor: context.mgColors.divider,
                        background: context.mgColors.surface,
                      ),
                    ),
                  ],
                );
              },
            ),
          ],
        ),
      );
    },
  );

  static String _displayFirmware(String? value) {
    if (value == null || value.isEmpty || value.toLowerCase() == 'unknown') {
      return '等待识别';
    }
    return value;
  }

  static String _displayVehicle(String? value) {
    if (value == null || value.isEmpty || value.toLowerCase() == 'unknown') {
      return '等待识别';
    }
    return value;
  }

  static String _connectionState(ConnectionController controller) {
    if (controller.isConnected) return '已连接';
    if (controller.hasSession) return '等待飞控心跳';
    return '未连接';
  }

  static String _gpsFixLabel(int? fixType) {
    if (fixType == null) return '等待数据';
    return switch (fixType) {
      0 => '无定位',
      1 => '无定位',
      2 => '2D 定位',
      3 => '3D 定位',
      4 => 'DGPS 定位',
      5 => 'RTK 浮点解',
      >= 6 => 'RTK 固定解',
      _ => '未知状态',
    };
  }
}
