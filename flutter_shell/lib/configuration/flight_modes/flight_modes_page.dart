import 'package:flutter/cupertino.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_colors.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';
import 'package:mgcs_frontend/widgets/aligned_field_row.dart';

import '../widgets/parameter_helpers.dart';
import '../widgets/parameter_value_control.dart';

/// 飞行模式 PWM 档位与辅助 RC 通道功能配置页。
class FlightModesPage extends StatefulWidget {
  const FlightModesPage({super.key, required this.controller});

  final ConnectionController controller;

  @override
  State<FlightModesPage> createState() => _FlightModesPageState();
}

class _FlightModesPageState extends State<FlightModesPage> {
  /// ArduPilot 多旋翼/固定翼的 FLTMODE PWM 分档来自参数描述库的规则。
  /// 固定翼的模式 1 还接受 2050 以上的特殊回绕值，不能用一套通用硬编码覆盖。
  static List<({String label, int min, int max})> _pwmRangesFor(
    List<Map<String, Object?>> ranges,
  ) => [
    for (final range in ranges)
      if (range['label'] is String &&
          range['minimum'] is num &&
          range['maximum'] is num)
        (
          label: range['label']! as String,
          min: (range['minimum']! as num).toInt(),
          max: (range['maximum']! as num).toInt(),
        ),
  ];

  /// 当前模式通道的实时 PWM；通道未知或没有遥控数据时返回 null。
  int? _modeChannelPwm(ConnectionController controller) {
    if (!controller.rcChannelsValid) return null;
    final name = controller.roleParam('flight_mode.channel');
    final channel = name == null
        ? null
        : controller.parameter(name)?.value.round();
    if (channel == null || channel < 1) return null;
    if (channel > controller.rcChannels.length) return null;
    return controller.rcChannels[channel - 1];
  }

  /// PWM 落在第几档；无效 RC 值和超出量程返回 -1。
  static int _modeIndexForPwm(
    int pwm,
    List<({String label, int min, int max})> ranges,
  ) {
    if (pwm < 800 || pwm > 2200) return -1;
    for (var index = 0; index < ranges.length; index++) {
      final range = ranges[index];
      if (pwm >= range.min && pwm <= range.max) return index;
    }
    return -1;
  }

  final Set<String> _registeredMetadata = <String>{};

  @override
  void initState() {
    super.initState();
    widget.controller.ensureParamRoles();
    // 枚举下拉的元数据要按角色解析后的真实参数名登记（safety.md S6）。
    // 角色表异步到位，所以初始化登记一次，角色/机型变化时再补登记。
    widget.controller.profileChanges.addListener(_registerMetadata);
    _registerMetadata();
  }

  void _registerMetadata() {
    if (!mounted) return;
    final names = <String>{};
    void add(String? name) {
      if (name != null) names.add(name);
    }

    add(widget.controller.roleParam('flight_mode.channel'));
    for (var position = 1; position <= 6; position++) {
      add(widget.controller.roleParam('flight_mode.slot', index: position));
    }
    for (var channel = 5; channel <= 16; channel++) {
      add(widget.controller.roleParam('rc.channel_option', index: channel));
    }
    final added = names.difference(_registeredMetadata);
    if (added.isEmpty) return;
    _registeredMetadata.addAll(added);
    widget.controller.loadParameterMetadata(added);
  }

  @override
  void dispose() {
    widget.controller.profileChanges.removeListener(_registerMetadata);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.ioChanges,
    ]),
    builder: (context, _) {
      final colors = context.mgColors;
      final liveMode =
          widget.controller.vehicleIdentity?['flight_mode'] as String?;
      final channelPwm = _modeChannelPwm(widget.controller);
      final pwmRanges = _pwmRangesFor(widget.controller.flightModePwmRanges);
      final activeIndex = channelPwm == null
          ? -1
          : _modeIndexForPwm(channelPwm, pwmRanges);
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
            LayoutBuilder(
              builder: (context, constraints) {
                final fieldWidth = constraints.maxWidth < 520
                    ? constraints.maxWidth
                    : 320.0;
                return Wrap(
                  spacing: 16,
                  runSpacing: 8,
                  alignment: WrapAlignment.spaceBetween,
                  crossAxisAlignment: WrapCrossAlignment.center,
                  children: [
                    SizedBox(
                      width: fieldWidth,
                      child: AlignedFieldRow(
                        label: '模式通道',
                        labelWidth: 64,
                        child: ParameterValueControl(
                          parameter:
                              widget.controller.parameters[widget.controller
                                      .roleParam('flight_mode.channel') ??
                                  ''],
                          controller: widget.controller,
                          popupWidth: fieldWidth < 300 ? 150 : 180,
                        ),
                      ),
                    ),
                    if (liveMode != null && liveMode.isNotEmpty)
                      _FlightModeBadge(mode: liveMode),
                  ],
                );
              },
            ),
            const SizedBox(height: 14),
            if (pwmRanges.isEmpty)
              Text(
                '飞控未提供飞行模式档位信息',
                style: MgTypography.sans(
                  MgTypography.caption.copyWith(color: colors.textSecondary),
                ),
              )
            else
              GridView.builder(
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                itemCount: pwmRanges.length,
                gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                  crossAxisCount: 3,
                  crossAxisSpacing: 16,
                  mainAxisSpacing: 14,
                  mainAxisExtent: 90,
                ),
                itemBuilder: (context, index) {
                  final position = index + 1;
                  return _FlightModeCard(
                    position: position,
                    pwmRange: pwmRanges[index].label,
                    active: index == activeIndex,
                    parameter:
                        widget.controller.parameters[widget.controller
                                .roleParam(
                                  'flight_mode.slot',
                                  index: position,
                                ) ??
                            ''],
                    controller: widget.controller,
                  );
                },
              ),
            const SizedBox(height: 22),
            MacosListTile(
              leading: const MacosIcon(
                CupertinoIcons.arrow_left_right,
                size: 18,
              ),
              title: const Text('RC 通道功能'),
            ),
            const SizedBox(height: 10),
            GridView.builder(
              shrinkWrap: true,
              physics: const NeverScrollableScrollPhysics(),
              itemCount: 12,
              gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                crossAxisCount: 3,
                crossAxisSpacing: 14,
                mainAxisSpacing: 12,
                mainAxisExtent: 56,
              ),
              itemBuilder: (context, index) {
                final channel = index + 5;
                return _AuxChannelCard(
                  channel: channel,
                  parameter:
                      widget.controller.parameters[widget.controller.roleParam(
                            'rc.channel_option',
                            index: channel,
                          ) ??
                          ''],
                  controller: widget.controller,
                  dividerColor: colors.divider,
                  active: rcSwitchActive(widget.controller, channel),
                );
              },
            ),
          ],
        ),
      );
    },
  );
}

class _FlightModeCard extends StatelessWidget {
  const _FlightModeCard({
    required this.position,
    required this.pwmRange,
    required this.active,
    required this.parameter,
    required this.controller,
  });

  final int position;
  final String pwmRange;

  /// 遥控器当前拨到这个档位时为 true，用于高亮。
  final bool active;
  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: active ? colors.accentSurface : colors.surfaceMuted,
        border: Border.all(
          color: active ? colors.accent : colors.divider,
          width: active ? 1.5 : 1,
        ),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 8, 14, 6),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Text('模式 $position', style: theme.typography.headline),
                const Spacer(),
                Text(pwmRange, style: theme.typography.caption1),
              ],
            ),
            const SizedBox(height: 2),
            ParameterValueControl(parameter: parameter, controller: controller),
          ],
        ),
      ),
    );
  }
}

class _AuxChannelCard extends StatelessWidget {
  const _AuxChannelCard({
    required this.channel,
    required this.parameter,
    required this.controller,
    required this.dividerColor,
    required this.active,
  });

  final int channel;
  final VehicleParameter? parameter;
  final ConnectionController controller;
  final Color dividerColor;

  /// 拨杆当前是否处在这一档的激活位置，用于高亮。
  final bool active;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: active
          ? context.mgColors.accentSurface
          : context.mgColors.surfaceMuted,
      border: Border.all(
        color: active ? context.mgColors.accent : dividerColor,
        width: active ? 1.5 : 1,
      ),
      borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      child: Row(
        children: [
          SizedBox(
            width: 42,
            child: Text(
              'RC$channel',
              style: MacosTheme.of(context).typography.headline,
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: ParameterValueControl(
              parameter: parameter,
              controller: controller,
              popupWidth: 160,
            ),
          ),
        ],
      ),
    ),
  );
}

class _FlightModeBadge extends StatelessWidget {
  const _FlightModeBadge({required this.mode});

  final String mode;

  @override
  Widget build(BuildContext context) => Label(
    icon: const MacosIcon(CupertinoIcons.airplane, size: 14),
    text: Text(
      mode,
      style: MgTypography.sans(
        MgTypography.body.copyWith(color: context.mgColors.accent),
      ),
    ),
  );
}
