import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../connection/connection_controller.dart';
import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import 'mg_animated_number.dart';

/// 顶部状态栏的数据快照。
///
/// 控件本身只负责布局和显示，字段来自现有 [ConnectionController]，不新增后端契约。
/// 这一层分开后，测试不需要启动 FFI 或真实飞控链路。
@immutable
class MgTopStatusData {
  const MgTopStatusData({
    required this.connected,
    required this.armed,
    required this.flying,
    required this.flightMode,
    required this.vtolState,
    required this.gpsFixType,
    required this.satellitesVisible,
    required this.batteryRemainingPercent,
    required this.batteryVoltage,
    required this.transportLabel,
  });

  factory MgTopStatusData.fromController(ConnectionController controller) {
    return MgTopStatusData(
      connected: controller.isConnected,
      armed: controller.vehicleArmed,
      flying: controller.vehicleFlying,
      flightMode: controller.vehicleFlightMode,
      vtolState: controller.vehicleVtolState,
      gpsFixType: controller.gpsFixType,
      satellitesVisible: controller.satellitesVisible,
      batteryRemainingPercent: controller.batteryRemainingPercent,
      batteryVoltage: controller.batteryVoltage,
      transportLabel: controller.selectedTransportLabel,
    );
  }

  final bool connected;
  final bool armed;
  final bool flying;
  final String flightMode;
  final int vtolState;
  final int? gpsFixType;
  final int? satellitesVisible;
  final int? batteryRemainingPercent;
  final double? batteryVoltage;
  final String transportLabel;

  String get statusLabel {
    if (!connected) return '未连接';
    if (flying) return '飞行中';
    if (armed) return '已解锁';
    return '待命';
  }

  String get modeLabel {
    if (flightMode.isEmpty) return '—';
    return switch (vtolState) {
      1 => '$flightMode · 转 FW',
      2 => '$flightMode · 转 MR',
      3 => '$flightMode · MR',
      4 => '$flightMode · FW',
      _ => flightMode,
    };
  }

  String get gpsFixLabel => switch (gpsFixType) {
    2 => '2D',
    3 => '3D',
    4 => 'DGPS',
    5 => 'RTK Float',
    6 => 'RTK Fixed',
    null => '—',
    _ => 'Fix $gpsFixType',
  };
}

/// QGC 风格的主页面顶部状态栏。
///
/// 左侧承载连接状态和飞行模式，右侧放运行中必须持续可见的 GPS、电池和链路状态。
class MgTopStatusBar extends StatelessWidget {
  const MgTopStatusBar({super.key, required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.toolbar,
        border: Border(bottom: BorderSide(color: colors.hairline)),
      ),
      child: SizedBox(
        height: MgMetrics.toolbarHeight,
        child: LayoutBuilder(
          builder: (context, constraints) {
            final showSecondary = constraints.maxWidth >= 860;
            return Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12),
              child: Row(
                children: [
                  const _BrandLabel(),
                  const SizedBox(width: 14),
                  _MainStatus(data: data),
                  const SizedBox(width: 18),
                  _FlightMode(data: data),
                  const Spacer(),
                  _GpsIndicator(data: data),
                  const SizedBox(width: 16),
                  _BatteryIndicator(data: data),
                  if (showSecondary) ...[
                    const SizedBox(width: 16),
                    _LinkIndicator(data: data),
                  ],
                ],
              ),
            );
          },
        ),
      ),
    );
  }
}

class _BrandLabel extends StatelessWidget {
  const _BrandLabel();

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(Icons.flight, size: 17, color: colors.accent),
        const SizedBox(width: 7),
        Text(
          'M-GCS',
          style: MgTypography.sans(
            MgTypography.bodyStrong.copyWith(color: colors.textPrimary),
          ),
        ),
      ],
    );
  }
}

class _MainStatus extends StatelessWidget {
  const _MainStatus({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final statusColor = !data.connected
        ? colors.danger
        : data.flying
        ? colors.accent
        : data.armed
        ? colors.success
        : colors.warning;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: statusColor.withValues(alpha: colors.isDark ? 0.18 : 0.10),
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 7,
              height: 7,
              decoration: BoxDecoration(
                color: statusColor,
                shape: BoxShape.circle,
              ),
            ),
            const SizedBox(width: 7),
            Text(
              data.statusLabel,
              style: MgTypography.sans(
                MgTypography.body.copyWith(color: colors.textPrimary),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FlightMode extends StatelessWidget {
  const _FlightMode({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(Icons.airplanemode_active, size: 16, color: colors.textSecondary),
        const SizedBox(width: 7),
        Text(
          data.modeLabel,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MgTypography.sans(
            MgTypography.data.copyWith(color: colors.textPrimary),
          ),
        ),
      ],
    );
  }
}

class _GpsIndicator extends StatelessWidget {
  const _GpsIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final color = !data.connected || (data.gpsFixType ?? 0) < 3
        ? colors.warning
        : colors.success;
    return MacosTooltip(
      message: 'GPS：${data.gpsFixLabel}',
      child: _TopBarMetric(
        icon: Icons.gps_fixed,
        label: 'GPS',
        color: color,
        value: MgAnimatedNumber(
          value: data.satellitesVisible?.toDouble() ?? double.nan,
          fractionDigits: 0,
          textAlign: TextAlign.left,
          style: MgTypography.data,
          color: colors.textPrimary,
          placeholder: '—',
        ),
        suffix: data.satellitesVisible == null ? null : '星',
      ),
    );
  }
}

class _BatteryIndicator extends StatelessWidget {
  const _BatteryIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final percent = data.batteryRemainingPercent;
    final color = switch (percent) {
      null => colors.textSecondary,
      <= 20 => colors.danger,
      <= 35 => colors.warning,
      _ => colors.success,
    };
    final voltage = data.batteryVoltage;
    return MacosTooltip(
      message:
          '电池：${percent == null ? '—' : '$percent%'}'
          '${voltage == null ? '' : ' · ${voltage.toStringAsFixed(1)} V'}',
      child: _TopBarMetric(
        icon: Icons.battery_std,
        label: 'BAT',
        color: color,
        value: percent == null
            ? Text(
                voltage == null ? '—' : '${voltage.toStringAsFixed(1)} V',
                style: MgTypography.sans(
                  MgTypography.data.copyWith(color: colors.textPrimary),
                ),
              )
            : MgAnimatedNumber(
                value: percent.toDouble(),
                fractionDigits: 0,
                textAlign: TextAlign.left,
                style: MgTypography.data,
                color: colors.textPrimary,
              ),
        suffix: percent == null ? null : '%',
      ),
    );
  }
}

class _LinkIndicator extends StatelessWidget {
  const _LinkIndicator({required this.data});

  final MgTopStatusData data;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return MacosTooltip(
      message: data.connected ? '链路已连接' : '链路未连接',
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            Icons.link,
            size: 15,
            color: data.connected ? colors.success : colors.danger,
          ),
          const SizedBox(width: 6),
          Text(
            data.connected ? data.transportLabel : '未连接',
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ),
    );
  }
}

class _TopBarMetric extends StatelessWidget {
  const _TopBarMetric({
    required this.icon,
    required this.label,
    required this.color,
    required this.value,
    this.suffix,
  });

  final IconData icon;
  final String label;
  final Color color;
  final Widget value;
  final String? suffix;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 15, color: color),
        const SizedBox(width: 6),
        Text(
          label,
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textSecondary),
          ),
        ),
        const SizedBox(width: 5),
        value,
        if (suffix != null) ...[
          const SizedBox(width: 2),
          Text(
            suffix!,
            style: MgTypography.sans(
              MgTypography.caption.copyWith(color: colors.textSecondary),
            ),
          ),
        ],
      ],
    );
  }
}
