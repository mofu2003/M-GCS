import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:latlong2/latlong.dart';

import '../connection/connection_controller.dart';
import '../mission/mission_geo.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_animated_number.dart';
import 'mg_floating_panel.dart';

/// 底部遥测仪表盘的数据快照。
///
/// 字段与 QGC `QGCCorePlugin::createInstrumentPanel()` 为 VTOL / 固定翼机型
/// 生成的默认面板一一对应（见 `src/API/QGCCorePlugin.cc`）。控件只负责布局，
/// 不直接读 [ConnectionController]，测试因此不用起 FFI。
@immutable
class MgTelemetryDashboardData {
  const MgTelemetryDashboardData({
    required this.relativeAltitudeMeters,
    required this.distanceToHomeMeters,
    required this.climbRateMetersPerSecond,
    required this.groundSpeedMetersPerSecond,
    required this.airSpeedMetersPerSecond,
    required this.throttlePercent,
    required this.flightTimeSeconds,
    required this.flightDistanceMeters,
  });

  factory MgTelemetryDashboardData.fromController(
    ConnectionController controller,
  ) {
    return MgTelemetryDashboardData(
      relativeAltitudeMeters: controller.relativeAltitudeMeters,
      distanceToHomeMeters: _distanceToHomeMeters(controller),
      climbRateMetersPerSecond: controller.climbRateMetersPerSecond,
      groundSpeedMetersPerSecond: controller.groundSpeedMetersPerSecond,
      airSpeedMetersPerSecond: controller.airspeedMetersPerSecond,
      throttlePercent: controller.throttlePercent,
      flightTimeSeconds: controller.flightTimeSeconds,
      flightDistanceMeters: controller.flightDistanceMeters,
    );
  }

  /// 相对 Home 的高度（`Vehicle.AltitudeRelative`）。
  final double? relativeAltitudeMeters;

  /// 与 Home 点的水平距离（`Vehicle.DistanceToHome`）。
  final double? distanceToHomeMeters;

  /// 垂直速度（`Vehicle.ClimbRate`）。
  final double? climbRateMetersPerSecond;

  /// 地速（`Vehicle.GroundSpeed`）。
  final double? groundSpeedMetersPerSecond;

  /// 空速（`Vehicle.AirSpeed`）。
  final double? airSpeedMetersPerSecond;

  /// 油门百分比（`Vehicle.ThrottlePct`）。
  final double? throttlePercent;

  /// 本次解锁后的累计飞行时间（`Vehicle.FlightTime`）。
  final double flightTimeSeconds;

  /// 本次解锁后的累计飞行里程（`Vehicle.FlightDistance`）。
  final double flightDistanceMeters;

  static double? _distanceToHomeMeters(ConnectionController controller) {
    final home = controller.homePosition;
    final fix = controller.vehiclePosition;
    if (!controller.homePositionValid ||
        !controller.vehiclePositionValid ||
        home == null ||
        fix == null) {
      return null;
    }
    return wgs84DistanceMeters(
      LatLng(home.latitude, home.longitude),
      LatLng(fix.latitude, fix.longitude),
    );
  }
}

/// 飞行时间格式与 QGC 的 `Vehicle.FlightTime` 一致：`hh:mm:ss`。
String formatFlightTime(double seconds) {
  if (!seconds.isFinite || seconds < 0) return '--:--:--';
  final total = seconds.floor();
  final hours = (total ~/ 3600).toString().padLeft(2, '0');
  final minutes = ((total % 3600) ~/ 60).toString().padLeft(2, '0');
  final secs = (total % 60).toString().padLeft(2, '0');
  return '$hours:$minutes:$secs';
}

/// 悬浮在地图下方的遥测仪表盘。
///
/// 数值取自 QGC 默认 instrument panel（相对高度 / 距 Home / 垂直速度 / 地速 /
/// 空速 / 油门 / 飞行时间 / 飞行里程），排布沿用 QGC 的“4 列 × 2 行、按列排列”，
/// 指标名改为中文语。外观做成 macOS 浮动面板：圆角、毛玻璃半透明底、细边框加投影。
class MgTelemetryDashboard extends StatelessWidget {
  const MgTelemetryDashboard({super.key, required this.data});

  final MgTelemetryDashboardData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MgFloatingPanel(
      child: SizedBox(
        height: MgMetrics.dashboardHeight,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              _column([
                _metricCell(
                  context,
                  icon: Icons.arrow_upward,
                  label: '相对高度',
                  value: data.relativeAltitudeMeters,
                  unit: 'm',
                  fractionDigits: 1,
                  iconColor: colors.accent,
                ),
                _metricCell(
                  context,
                  icon: CupertinoIcons.bookmark_fill,
                  label: '距 Home',
                  value: data.distanceToHomeMeters,
                  unit: 'm',
                  fractionDigits: 1,
                  iconColor: colors.textSecondary,
                ),
              ]),
              _column([
                _metricCell(
                  context,
                  icon: Icons.north,
                  label: '垂直速度',
                  value: data.climbRateMetersPerSecond,
                  unit: 'm/s',
                  fractionDigits: 1,
                  iconColor: colors.textSecondary,
                ),
                _metricCell(
                  context,
                  icon: Icons.arrow_forward,
                  label: '地速',
                  value: data.groundSpeedMetersPerSecond,
                  unit: 'm/s',
                  fractionDigits: 1,
                  iconColor: colors.textSecondary,
                ),
              ]),
              _column([
                _metricCell(
                  context,
                  icon: Icons.air,
                  label: '空速',
                  value: data.airSpeedMetersPerSecond,
                  unit: 'm/s',
                  fractionDigits: 1,
                  iconColor: colors.textSecondary,
                ),
                _metricCell(
                  context,
                  icon: Icons.speed,
                  label: '油门',
                  value: data.throttlePercent,
                  unit: '%',
                  fractionDigits: 0,
                  iconColor: colors.textSecondary,
                ),
              ]),
              _column([
                _textCell(
                  context,
                  icon: Icons.timer_outlined,
                  label: '飞行时间',
                  text: formatFlightTime(data.flightTimeSeconds),
                  iconColor: colors.textSecondary,
                ),
                _metricCell(
                  context,
                  icon: Icons.directions_walk,
                  label: '飞行里程',
                  value: data.flightDistanceMeters,
                  unit: 'm',
                  fractionDigits: 1,
                  iconColor: colors.textSecondary,
                ),
              ]),
            ],
          ),
        ),
      ),
    );
  }

  Widget _column(List<Widget> cells) => SizedBox(
    width: MgMetrics.dashboardColumnWidth,
    child: Column(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: cells,
    ),
  );

  Widget _metricCell(
    BuildContext context, {
    required IconData icon,
    required String label,
    required double? value,
    required String unit,
    required int fractionDigits,
    required Color iconColor,
  }) {
    return _DashboardCell(
      icon: icon,
      label: label,
      iconColor: iconColor,
      unit: unit,
      value: MgAnimatedNumber(
        value: value ?? double.nan,
        fractionDigits: fractionDigits,
        textAlign: TextAlign.left,
        style: MgTypography.dataLarge,
        color: context.mgColors.textPrimary,
      ),
    );
  }

  Widget _textCell(
    BuildContext context, {
    required IconData icon,
    required String label,
    required String text,
    required Color iconColor,
  }) {
    return _DashboardCell(
      icon: icon,
      label: label,
      iconColor: iconColor,
      value: Text(
        text,
        maxLines: 1,
        style: MgTypography.sans(
          MgTypography.dataLarge.copyWith(color: context.mgColors.textPrimary),
        ),
      ),
    );
  }
}

/// 单个读数：图标 + 中文指标名 + 数值（+ 单位），整行垂直居中。
class _DashboardCell extends StatelessWidget {
  const _DashboardCell({
    required this.icon,
    required this.label,
    required this.iconColor,
    required this.value,
    this.unit,
  });

  final IconData icon;
  final String label;
  final Color iconColor;
  final Widget value;
  final String? unit;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final unitStyle = MgTypography.sans(
      MgTypography.caption.copyWith(color: colors.textSecondary),
    );
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        Icon(icon, size: 15, color: iconColor),
        const SizedBox(width: 6),
        // 标签列可收缩：换字体或放大字号时截断，而不是把整行撑破。
        Flexible(
          child: Text(
            label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ),
        const SizedBox(width: 6),
        value,
        if (unit != null) ...[
          const SizedBox(width: 3),
          Text(unit!, maxLines: 1, style: unitStyle),
        ],
      ],
    );
  }
}
