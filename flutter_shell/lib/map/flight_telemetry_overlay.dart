import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:latlong2/latlong.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../mission/mission_document_models.dart';
import '../mission/mission_geo.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';

/// 地图上的飞行仪表盘。
///
/// 平时是低高度状态条，展开后显示飞行、导航、任务和引导目标。它只读，
/// 不承载任务编辑或飞行控制，数据来自现有 10Hz 遥测轮询。
class FlightTelemetryOverlay extends StatefulWidget {
  const FlightTelemetryOverlay({
    super.key,
    required this.controller,
    this.maxWidth = 680,
  });

  final ConnectionController controller;
  final double maxWidth;

  @override
  State<FlightTelemetryOverlay> createState() =>
      _FlightTelemetryOverlayState();
}

class _FlightTelemetryOverlayState extends State<FlightTelemetryOverlay> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    return AnimatedSize(
      duration: const Duration(milliseconds: 180),
      curve: Curves.easeOutCubic,
      alignment: Alignment.topLeft,
      child: AnimatedBuilder(
        animation: widget.controller.telemetryChanges,
        builder: (context, _) {
          final colors = MgColors.of(context);
          final controller = widget.controller;
          return ConstrainedBox(
            constraints: BoxConstraints(maxWidth: widget.maxWidth),
            child: DecoratedBox(
              decoration: BoxDecoration(
                color: colors.mapControlSurface,
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
                border: Border.all(color: colors.divider),
                boxShadow: [
                  BoxShadow(
                    color: colors.shadow,
                    blurRadius: 18,
                    offset: const Offset(0, 5),
                  ),
                ],
              ),
              child: Padding(
                padding: const EdgeInsets.fromLTRB(10, 7, 6, 7),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    _buildCompactRow(context, controller),
                    if (_expanded) ...[
                      const SizedBox(height: 7),
                      Divider(height: 1, color: colors.divider),
                      const SizedBox(height: 8),
                      _buildExpandedGrid(context, controller),
                    ],
                  ],
                ),
              ),
            ),
          );
        },
      ),
    );
  }

  Widget _buildCompactRow(
    BuildContext context,
    ConnectionController controller,
  ) {
    final colors = MgColors.of(context);
    return Wrap(
      spacing: 10,
      runSpacing: 4,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        _StatusDot(connected: controller.isConnected),
        const SizedBox(width: 6),
        _CompactValue(
          value: _modeLabel(controller),
          color: colors.textPrimary,
        ),
        const SizedBox(width: 12),
        _CompactValue(
          value: controller.vehicleArmed ? '已解锁' : '已上锁',
          color: controller.vehicleArmed
              ? colors.success
              : colors.textSecondary,
        ),
        _CompactMetric(
          label: 'GPS',
          value: controller.satellitesVisible?.toString() ?? '—',
          color: _gpsColor(colors, controller),
        ),
        _CompactMetric(
          label: '地速',
          value: _format(controller.groundSpeedMetersPerSecond, 1),
          unit: 'm/s',
        ),
        _CompactMetric(
          label: '高度',
          value: _format(controller.relativeAltitudeMeters, 1),
          unit: 'm',
        ),
        _CompactMetric(
          label: '电量',
          value: controller.batteryRemainingPercent == null
              ? '—'
              : '${controller.batteryRemainingPercent}',
          unit: '%',
          color: _batteryColor(colors, controller.batteryRemainingPercent),
        ),
        MacosTooltip(
          message: _expanded ? '收起仪表盘' : '展开仪表盘',
          child: MacosIconButton(
            icon: MacosIcon(
              _expanded
                  ? CupertinoIcons.chevron_up
                  : CupertinoIcons.chevron_down,
              size: 13,
            ),
            semanticLabel: _expanded ? '收起仪表盘' : '展开仪表盘',
            boxConstraints: const BoxConstraints.tightFor(
              width: 28,
              height: 28,
            ),
            padding: EdgeInsets.zero,
            backgroundColor: colors.transparent,
            hoverColor: colors.mapControlHover,
            onPressed: () => setState(() => _expanded = !_expanded),
          ),
        ),
      ],
    );
  }

  Widget _buildExpandedGrid(
    BuildContext context,
    ConnectionController controller,
  ) {
    final colors = MgColors.of(context);
    final mission = controller.missionPlanning.executionProgress;
    final tiles = <Widget>[
      _DashboardTile(
        icon: CupertinoIcons.link,
        label: '链路',
        value: controller.isConnected ? '已连接' : '未连接',
        color: controller.isConnected ? colors.success : colors.danger,
      ),
      _DashboardTile(
        icon: CupertinoIcons.location,
        label: '定位',
        value: _gpsLabel(controller),
        color: _gpsColor(colors, controller),
      ),
      _DashboardTile(
        icon: CupertinoIcons.airplane,
        label: '空速',
        value: _valueWithUnit(controller.airspeedMetersPerSecond, 1, 'm/s'),
      ),
      _DashboardTile(
        icon: CupertinoIcons.speedometer,
        label: '地速',
        value: _valueWithUnit(controller.groundSpeedMetersPerSecond, 1, 'm/s'),
      ),
      _DashboardTile(
        icon: CupertinoIcons.arrow_up,
        label: '相对高度',
        value: _valueWithUnit(controller.relativeAltitudeMeters, 1, 'm'),
      ),
      _DashboardTile(
        icon: CupertinoIcons.arrow_up_circle,
        label: '绝对高度',
        value: _valueWithUnit(controller.absoluteAltitudeMeters, 1, 'm'),
      ),
      _DashboardTile(
        icon: CupertinoIcons.location_north,
        label: '航向',
        value: _valueWithUnit(controller.headingDegrees, 0, '°'),
      ),
      _DashboardTile(
        icon: CupertinoIcons.battery_100,
        label: '电池',
        value: _batteryLabel(controller),
        color: _batteryColor(colors, controller.batteryRemainingPercent),
      ),
      _DashboardTile(
        icon: CupertinoIcons.location_circle,
        label: '任务',
        value: _missionLabel(mission),
        color: mission?.aborted == true
            ? colors.danger
            : mission?.finished == true
            ? colors.success
            : colors.textPrimary,
      ),
    ];

    return LayoutBuilder(
      builder: (context, constraints) {
        final width = constraints.maxWidth.isFinite
            ? constraints.maxWidth
            : widget.maxWidth;
        final columns = width >= 560 ? 3 : 2;
        final tileWidth = (width - (columns - 1) * 7) / columns;
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Wrap(
              spacing: 7,
              runSpacing: 7,
              children: [
                for (final tile in tiles)
                  SizedBox(width: tileWidth, child: tile),
              ],
            ),
            if (controller.guidedTargetValid &&
                controller.guidedTargetPosition != null) ...[
              const SizedBox(height: 8),
              _GuidedTargetTile(
                target: controller.guidedTargetPosition!,
                vehicle: controller.vehiclePosition,
                vehicleValid: controller.vehiclePositionValid,
              ),
            ],
          ],
        );
      },
    );
  }

  static String _modeLabel(ConnectionController controller) {
    final mode = controller.vehicleFlightMode.isEmpty
        ? '—'
        : controller.vehicleFlightMode;
    return switch (controller.vehicleVtolState) {
      1 => '$mode · 正在转换为固定翼',
      2 => '$mode · 正在转换为多旋翼',
      3 => '$mode · MC',
      4 => '$mode · FW',
      _ => mode,
    };
  }

  static String _gpsLabel(ConnectionController controller) {
    final fix = controller.gpsFixType;
    final satellites = controller.satellitesVisible;
    if (fix == null || fix <= 0) return '无定位';
    final fixLabel = switch (fix) {
      2 => '2D',
      3 => '3D',
      4 => 'DGPS',
      5 => 'RTK Float',
      6 => 'RTK Fixed',
      _ => 'Fix $fix',
    };
    return satellites == null ? fixLabel : '$fixLabel · $satellites 星';
  }

  static Color _gpsColor(MgColors colors, ConnectionController controller) {
    if (!controller.isConnected || (controller.gpsFixType ?? 0) < 3) {
      return colors.warning;
    }
    return colors.success;
  }

  static Color _batteryColor(MgColors colors, int? percent) {
    if (percent == null) return colors.textSecondary;
    if (percent <= 20) return colors.danger;
    if (percent <= 35) return colors.warning;
    return colors.success;
  }

  static String _batteryLabel(ConnectionController controller) {
    final percent = controller.batteryRemainingPercent;
    final voltage = controller.batteryVoltage;
    if (percent != null && voltage != null) {
      return '$percent% · ${voltage.toStringAsFixed(1)}V';
    }
    if (percent != null) return '$percent%';
    if (voltage != null) return '${voltage.toStringAsFixed(1)}V';
    return '—';
  }

  static String _missionLabel(MissionExecutionProgress? progress) {
    if (progress == null || !progress.active) return '—';
    if (progress.finished) return '完成 · ${progress.total} 项';
    if (progress.aborted) return '已中止 · ${progress.total} 项';
    final current = (progress.currentIndex + 1).clamp(1, progress.total);
    if (progress.reachedEnd) return '航段完成 · $current/${progress.total}';
    return '$current/${progress.total} · 已到达 ${progress.reachedCount}';
  }

  static String _format(double? value, int digits) {
    if (value == null || !value.isFinite) return '—';
    return value.toStringAsFixed(digits);
  }

  static String _valueWithUnit(double? value, int digits, String unit) {
    return '${_format(value, digits)} $unit';
  }
}

class _StatusDot extends StatelessWidget {
  const _StatusDot({required this.connected});

  final bool connected;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return Container(
      width: 7,
      height: 7,
      decoration: BoxDecoration(
        color: connected ? colors.success : colors.danger,
        shape: BoxShape.circle,
      ),
    );
  }
}

class _CompactValue extends StatelessWidget {
  const _CompactValue({required this.value, required this.color});

  final String value;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Text(
      value,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: MgTypography.sans(
        MgTypography.bodyStrong.copyWith(color: color),
      ),
    );
  }
}

class _CompactMetric extends StatelessWidget {
  const _CompactMetric({
    required this.label,
    required this.value,
    this.unit,
    this.color,
  });

  final String label;
  final String value;
  final String? unit;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 6),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.baseline,
        textBaseline: TextBaseline.alphabetic,
        children: [
          Text(
            label,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
          const SizedBox(width: 5),
          Text(
            unit == null ? value : '$value $unit',
            style: MgTypography.sans(
              MgTypography.data.copyWith(color: color ?? colors.textPrimary),
            ),
          ),
        ],
      ),
    );
  }
}

class _DashboardTile extends StatelessWidget {
  const _DashboardTile({
    required this.icon,
    required this.label,
    required this.value,
    this.color,
  });

  final IconData icon;
  final String label;
  final String value;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    final valueColor = color ?? colors.textPrimary;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.valueTile,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: colors.divider),
      ),
      child: SizedBox(
        height: 55,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Row(
                children: [
                  Icon(icon, size: 12, color: colors.textSecondary),
                  const SizedBox(width: 4),
                  Expanded(
                    child: Text(
                      label,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.caption.copyWith(
                          color: colors.textSecondary,
                        ),
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 2),
              Text(
                value,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MgTypography.sans(
                  MgTypography.dataLarge.copyWith(color: valueColor),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _GuidedTargetTile extends StatelessWidget {
  const _GuidedTargetTile({
    required this.target,
    required this.vehicle,
    required this.vehicleValid,
  });

  final VehicleFix target;
  final VehicleFix? vehicle;
  final bool vehicleValid;

  @override
  Widget build(BuildContext context) {
    final colors = MgColors.of(context);
    final distance = vehicleValid && vehicle != null
        ? wgs84DistanceMeters(
            LatLng(vehicle!.latitude, vehicle!.longitude),
            LatLng(target.latitude, target.longitude),
          )
        : null;
    final distanceText = distance == null
        ? '距离未知'
        : distance >= 1000
        ? '${(distance / 1000).toStringAsFixed(2)} km'
        : '${distance.toStringAsFixed(0)} m';
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.accentSurface,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: colors.warning.withValues(alpha: 0.45)),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 7),
        child: Row(
          children: [
            Icon(CupertinoIcons.placemark, size: 14, color: colors.warning),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                '引导目标  ${target.latitude.toStringAsFixed(5)}, '
                '${target.longitude.toStringAsFixed(5)}',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MgTypography.sans(
                  MgTypography.body.copyWith(color: colors.textPrimary),
                ),
              ),
            ),
            const SizedBox(width: 8),
            Text(
              distanceText,
              style: MgTypography.sans(
                MgTypography.data.copyWith(color: colors.warning),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
