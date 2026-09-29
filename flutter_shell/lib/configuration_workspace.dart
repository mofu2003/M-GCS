import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'realtime_waveform.dart';
import 'theme/mg_colors.dart';
import 'theme/mg_typography.dart';
import 'widgets/aligned_field_row.dart';
import 'widgets/adaptive_popup_button.dart';
import 'widgets/app_workspace_shell.dart';
import 'widgets/mac_panel.dart';
import 'widgets/mg_motor_output_bar.dart';
import 'widgets/parameter_load_progress.dart';

/// 后台页面统一间距标准（标签页内层叠加 MacosTabView 自身的 12pt）。
///
/// 左右边距 24、区块间距 16、区块标题到内容 8、卡片间距 10。
/// 新页面沿用这套数值，避免各页各自为政导致疏密不一致。
const double _pageMargin = 24;
const double _tabBodyInset = 12;
const double _sectionGap = 16;
const double _headerGap = 8;
const double _cardGap = 10;

/// 标签页外壳：标签卡用 macos_ui 的 [MacosSegmentedControl]，
/// 面板沿用 macos_ui 标签视图的配色与圆角。
///
/// 不用 [MacosTabView]：它在 didChangeDependencies 中重复 addListener，
/// 而 dispose 只移除一次，点击标签会给已销毁的 State 发通知。
class _MacTabSurface extends StatefulWidget {
  const _MacTabSurface({
    super.key,
    required this.labels,
    required this.children,
    this.controller,
  });

  final List<String> labels;
  final List<Widget> children;

  /// 外部控制器。需要知道当前选中哪一页时传入，
  /// 不传则由本组件自己创建（默认行为）。
  final MacosTabController? controller;

  @override
  State<_MacTabSurface> createState() => _MacTabSurfaceState();
}

class _MacTabSurfaceState extends State<_MacTabSurface> {
  late final MacosTabController _tabs =
      widget.controller ??
      MacosTabController(initialIndex: 0, length: widget.labels.length);
  late final bool _ownsTabs = widget.controller == null;

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
  }

  void _handleTabChanged() {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    if (_ownsTabs) _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(
      _tabBodyInset,
      kMacTabStripGap,
      _tabBodyInset,
      _tabBodyInset,
    ),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Center(
          child: MacosSegmentedControl(
            controller: _tabs,
            tabs: [for (final label in widget.labels) MacosTab(label: label)],
          ),
        ),
        const SizedBox(height: kMacTabStripGap),
        Expanded(
          child: MacPanel(
            child: IndexedStack(index: _tabs.index, children: widget.children),
          ),
        ),
      ],
    ),
  );
}

/// 参数配置模式：左侧固定导航，右侧切换独立配置页面。
class ConfigurationWorkspace extends StatefulWidget {
  const ConfigurationWorkspace({
    super.key,
    required this.onReturnToMap,
    required this.connectionController,
  });

  final VoidCallback onReturnToMap;
  final ConnectionController connectionController;

  @override
  State<ConfigurationWorkspace> createState() => _ConfigurationWorkspaceState();
}

class _ConfigurationWorkspaceState extends State<ConfigurationWorkspace> {
  static const _basePages = <_ConfigPageInfo>[
    _ConfigPageInfo('system', '系统状态', CupertinoIcons.heart),
    _ConfigPageInfo('calibration', '飞控校准', CupertinoIcons.scope),
    _ConfigPageInfo(
      'ports',
      '端口配置',
      CupertinoIcons.antenna_radiowaves_left_right,
    ),
    _ConfigPageInfo('motors', '电机输出', CupertinoIcons.slider_horizontal_3),
    _ConfigPageInfo('flightModes', '飞行模式', CupertinoIcons.airplane),
    _ConfigPageInfo('pid', 'PID 调参', CupertinoIcons.waveform_path_ecg),
    _ConfigPageInfo('battery', '电池配置', CupertinoIcons.battery_100),
    _ConfigPageInfo('arming', '解锁检查', CupertinoIcons.checkmark_shield),
    _ConfigPageInfo('allParameters', '全部参数', CupertinoIcons.list_number),
  ];

  String _selectedPageId = 'motors';

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.connectionController.profileChanges,
    builder: (context, _) {
      final pages = _pagesForProfile(
        widget.connectionController.controlProfile,
      );
      final selectedPageId = pages.any((page) => page.id == _selectedPageId)
          ? _selectedPageId
          : 'motors';
      final selectedIndex = pages.indexWhere(
        (page) => page.id == selectedPageId,
      );
      return AppWorkspaceShell(
        items: [
          for (final page in pages)
            AppWorkspaceItem(id: page.id, label: page.label, icon: page.icon),
        ],
        selectedId: pages[selectedIndex].id,
        onSelected: (id) => setState(() => _selectedPageId = id),
        onReturnToMap: widget.onReturnToMap,
        contentBuilder: (context, scrollController) => _buildSelectedPage(
          context,
          scrollController,
          pageId: selectedPageId,
        ),
      );
    },
  );

  List<_ConfigPageInfo> _pagesForProfile(VehicleControlProfile profile) => [
    for (final page in _basePages) page,
  ];

  Widget _buildSelectedPage(
    BuildContext context,
    ScrollController scrollController, {
    required String pageId,
  }) => switch (pageId) {
    'ports' => _PortConfigurationPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'motors' => _MotorOutputPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'flightModes' => _FlightModesPage(controller: widget.connectionController),
    'pid' => _PidTuningPage(controller: widget.connectionController),
    'battery' => const _BatteryConfigurationPage(),
    'arming' => _ArmingChecksPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'allParameters' => _AllParametersPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'system' => _SystemStatusPage(controller: widget.connectionController),
    'calibration' => _CalibrationPage(controller: widget.connectionController),
    _ => _AllParametersPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
  };
}

class _ConfigPageInfo {
  const _ConfigPageInfo(this.id, this.label, this.icon);

  final String id;
  final String label;
  final IconData icon;
}

class _CalibrationPage extends StatefulWidget {
  const _CalibrationPage({required this.controller});

  final ConnectionController controller;

  @override
  State<_CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<_CalibrationPage> {
  /// 加速度校准 / 磁力计校准 / 空速计校准 / 遥控器校准
  static const int _pageCount = 4;

  late final MacosTabController _tabs = MacosTabController(
    initialIndex: 0,
    length: _pageCount,
  );

  @override
  void initState() {
    super.initState();
    _tabs.addListener(_handleTabChanged);
    _syncWaveformPolling();
  }

  void _handleTabChanged() {
    if (!mounted) return;
    _syncWaveformPolling();
    setState(() {});
  }

  /// 前两页显示波形，只有停在这两页时才向后端取数。
  /// 两个面板同时挂在树上，所以取数开关必须由页面统一决定，
  /// 不能让面板各自调用，否则会互相覆盖。
  void _syncWaveformPolling() {
    widget.controller.setWaveformActive(_tabs.index <= 1);
  }

  @override
  void dispose() {
    _tabs.removeListener(_handleTabChanged);
    widget.controller.setWaveformActive(false);
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => _MacTabSurface(
    controller: _tabs,
    labels: const ['加速度校准', '磁力计校准', '空速计校准', '遥控器校准'],
    children: [
      _AccelerometerCalibrationView(controller: widget.controller),
      _MagnetometerCalibrationView(controller: widget.controller),
      const SizedBox.expand(),
      _RcCalibrationView(controller: widget.controller),
    ],
  );
}

/// 按 RCMAP_* 找到通道当前承担的功能，用于给主通道加中文标注。
String? _rcChannelFunction(ConnectionController controller, int channel) {
  const functions = [
    ('RCMAP_ROLL', '横滚'),
    ('RCMAP_PITCH', '俯仰'),
    ('RCMAP_THROTTLE', '油门'),
    ('RCMAP_YAW', '偏航'),
  ];
  for (final (name, label) in functions) {
    final value = controller.parameters[name]?.value;
    if (value != null && value.round() == channel) return label;
  }
  return null;
}

/// 遥控器校准：实时行程 + 反向开关，校准指令由飞控采样行程。
class _RcCalibrationView extends StatelessWidget {
  const _RcCalibrationView({required this.controller});

  final ConnectionController controller;

  static const _primaryChannels = [1, 2, 3, 4];
  static const _auxiliaryChannels = [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      controller.parameterChanges,
      controller.telemetryChanges,
      controller.calibrationChanges,
    ]),
    builder: (context, _) {
      final theme = MacosTheme.of(context);
      final colors = context.mgColors;
      final status = controller.calibrationStatus;
      final radioActive = status.active && status.type == CalibrationType.radio;
      final error = controller.calibrationError;
      final message = error ?? (radioActive ? status.message : '');
      final hint = message.isNotEmpty
          ? message
          : radioActive
          ? '正在校准，请按提示打满各通道行程'
          : '点击开始校准后按提示打满各通道行程';

      return SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(
          kWaveformContentInset,
          8,
          kWaveformContentInset,
          20,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            MacPanel(
              padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      hint,
                      style: theme.typography.body.copyWith(
                        color: error != null
                            ? colors.danger
                            : colors.textSecondary,
                      ),
                    ),
                  ),
                  const SizedBox(width: 12),
                  if (radioActive)
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: () =>
                          unawaited(controller.cancelCalibration()),
                      child: const Text('取消'),
                    )
                  else
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: controller.isConnected && !status.active
                          ? () => unawaited(
                              controller.startCalibration(
                                CalibrationType.radio,
                              ),
                            )
                          : null,
                      child: const Text('开始校准'),
                    ),
                ],
              ),
            ),
            const SizedBox(height: _sectionGap),
            Text('主通道', style: theme.typography.title3),
            const SizedBox(height: _headerGap),
            _RcChannelGrid(channels: _primaryChannels, controller: controller),
            const SizedBox(height: _sectionGap),
            Text('辅助通道', style: theme.typography.title3),
            const SizedBox(height: _headerGap),
            _RcChannelGrid(
              channels: _auxiliaryChannels,
              controller: controller,
            ),
          ],
        ),
      );
    },
  );
}

class _RcChannelGrid extends StatelessWidget {
  const _RcChannelGrid({required this.channels, required this.controller});

  final List<int> channels;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 900 ? 2 : 1;
      final width = (constraints.maxWidth - (columns - 1) * _cardGap) / columns;
      return Wrap(
        spacing: _cardGap,
        runSpacing: 4,
        children: [
          for (final channel in channels)
            SizedBox(
              width: width,
              child: _RcChannelTile(channel: channel, controller: controller),
            ),
        ],
      );
    },
  );
}

class _RcChannelTile extends StatelessWidget {
  const _RcChannelTile({required this.channel, required this.controller});

  final int channel;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final function = _rcChannelFunction(controller, channel);
    final value =
        controller.rcChannelsValid && channel <= controller.rcChannels.length
        ? controller.rcChannels[channel - 1]
        : null;
    final reversed =
        (controller.parameters['RC${channel}_REVERSED']?.value ?? 0) != 0;
    final canReverse =
        controller.isConnected &&
        controller.parameters.containsKey('RC${channel}_REVERSED');

    return SizedBox(
      height: MgMetrics.rowHeight,
      child: Row(
        children: [
          SizedBox(
            width: 108,
            child: Text(
              function == null ? '通道$channel' : '通道$channel（$function）',
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: theme.typography.caption1,
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: MgMotorOutputBar(
              value: value,
              minimum: 800,
              maximum: 2200,
              semanticLabel: '通道$channel PWM',
            ),
          ),
          const SizedBox(width: 10),
          MacosSwitch(
            value: reversed,
            size: ControlSize.small,
            onChanged: canReverse
                ? (next) => unawaited(
                    controller.setParameter(
                      'RC${channel}_REVERSED',
                      next ? 1 : 0,
                    ),
                  )
                : null,
            semanticLabel: '通道$channel 反向',
          ),
        ],
      ),
    );
  }
}

class _MagnetometerCalibrationView extends StatelessWidget {
  const _MagnetometerCalibrationView({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      controller.waveformChanges,
      controller.parameterChanges,
      controller.calibrationChanges,
    ]),
    builder: (context, _) => Column(
      children: [
        Flexible(
          child: SensorWaveformPanel(
            controller: controller,
            groups: const [SensorWaveformGroup.mag],
            instanceKind: WaveformInstanceKind.mag,
          ),
        ),
        const SizedBox(height: 8),
        Padding(
          padding: const EdgeInsets.symmetric(
            horizontal: kWaveformContentInset,
          ),
          child: Column(
            children: [
              _MagnetometerOptionsCard(controller: controller),
              const SizedBox(height: 8),
              _MagnetometerCalibrationCard(controller: controller),
            ],
          ),
        ),
      ],
    ),
  );
}

class _MagnetometerOptionsCard extends StatelessWidget {
  const _MagnetometerOptionsCard({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final orientation = controller.parameter('COMPASS_ORIENT');
    final autoRotation =
        _parameterNumeric(controller, 'COMPASS_AUTO_ROT', fallback: 0) != 0;
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          Expanded(
            child: _MagnetometerToggle(
              label: '自动磁偏角',
              value:
                  _parameterNumeric(
                    controller,
                    'COMPASS_AUTODEC',
                    fallback: 0,
                  ) !=
                  0,
              onChanged: controller.isConnected
                  ? (value) => controller.setParameter(
                      'COMPASS_AUTODEC',
                      value ? 1 : 0,
                    )
                  : null,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: _MagnetometerToggle(
              label: '自动检测安装方向',
              value: autoRotation,
              onChanged: controller.isConnected
                  ? (value) => controller.setParameter(
                      'COMPASS_AUTO_ROT',
                      value ? 1 : 0,
                    )
                  : null,
            ),
          ),
          const SizedBox(width: 12),
          SizedBox(
            width: 260,
            child: AdaptivePopupButton<int>(
              value: orientation?.value.round() ?? 0,
              options: _orientationOptionsFor(orientation),
              labelOf: (value) => _orientationLabelFor(orientation, value),
              onChanged: autoRotation || !controller.isConnected
                  ? null
                  : (value) => controller.setParameter(
                      'COMPASS_ORIENT',
                      value.toDouble(),
                    ),
            ),
          ),
        ],
      ),
    );
  }
}

class _MagnetometerToggle extends StatelessWidget {
  const _MagnetometerToggle({
    required this.label,
    required this.value,
    required this.onChanged,
  });

  final String label;
  final bool value;
  final ValueChanged<bool>? onChanged;

  @override
  Widget build(BuildContext context) => Container(
    height: 42,
    padding: const EdgeInsets.symmetric(horizontal: 12),
    decoration: BoxDecoration(
      color: context.mgColors.valueTile,
      borderRadius: BorderRadius.circular(9),
    ),
    child: Row(
      children: [
        Expanded(child: Text(label)),
        MacosSwitch(
          value: value,
          size: ControlSize.small,
          onChanged: onChanged,
        ),
      ],
    ),
  );
}

String _orientationLabelFor(VehicleParameter? parameter, int value) {
  for (final option in parameter?.options ?? const <ParameterOption>[]) {
    if (option.value.round() == value) return option.label;
  }
  return value == 0 ? 'None' : '$value';
}

List<int> _orientationOptionsFor(VehicleParameter? parameter) {
  final options =
      parameter?.options.map((option) => option.value.round()).toList() ??
      const <int>[];
  if (options.contains(0)) return options;
  return [0, ...options];
}

class _MagnetometerCalibrationCard extends StatelessWidget {
  const _MagnetometerCalibrationCard({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final status = controller.calibrationStatus;
    final calibrating =
        status.active && status.type == CalibrationType.magnetometer;
    final instances = controller.waveformMagInstances;
    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          Text('容差等级', style: MacosTheme.of(context).typography.callout),
          const SizedBox(width: 10),
          SizedBox(
            width: 140,
            child: AdaptivePopupButton<String>(
              value: '默认',
              options: const ['默认', '严格', '宽松'],
              labelOf: (value) => value,
              onChanged: null,
            ),
          ),
          const SizedBox(width: 18),
          Expanded(
            child: Wrap(
              spacing: 18,
              runSpacing: 4,
              children: [
                for (final instance in instances)
                  _MagneticFieldStrength(
                    controller: controller,
                    instance: instance,
                  ),
              ],
            ),
          ),
          const Spacer(),
          PushButton(
            controlSize: ControlSize.large,
            onPressed: controller.isConnected && !status.active
                ? () =>
                      controller.startCalibration(CalibrationType.magnetometer)
                : null,
            child: const Text('开始校准'),
          ),
          const SizedBox(width: 8),
          PushButton(
            controlSize: ControlSize.large,
            secondary: true,
            onPressed: calibrating
                ? () => controller.cancelCalibration()
                : null,
            child: const Text('取消'),
          ),
        ],
      ),
    );
  }
}

class _MagneticFieldStrength extends StatelessWidget {
  const _MagneticFieldStrength({
    required this.controller,
    required this.instance,
  });

  final ConnectionController controller;
  final WaveformInstance instance;

  @override
  Widget build(BuildContext context) {
    final values = controller.waveformMagLatest(instance.index);
    final finite = values.every((value) => value.isFinite);
    final magnitude = finite
        ? math.sqrt(
                values[0] * values[0] +
                    values[1] * values[1] +
                    values[2] * values[2],
              ) *
              1000
        : double.nan;
    return Text.rich(
      TextSpan(
        text: '磁力计${instance.index + 1} 磁场强度：',
        style: MacosTheme.of(context).typography.callout,
        children: [
          TextSpan(
            text: magnitude.isFinite ? magnitude.round().toString() : '—',
            style: MgTypography.sans(
              MgTypography.bodyStrong.copyWith(color: context.mgColors.success),
            ),
          ),
        ],
      ),
    );
  }
}

class _AccelerometerCalibrationView extends StatelessWidget {
  const _AccelerometerCalibrationView({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      // 用 loose 约束让波形区贴合自身内容高度：之前用 Expanded 会把它撑满，
      // 内容不足时图表下方会空出一大片，把校准卡片推得很远。
      Flexible(
        child: SensorWaveformPanel(
          controller: controller,
          groups: const [SensorWaveformGroup.accel, SensorWaveformGroup.gyro],
          instanceKind: WaveformInstanceKind.imu,
        ),
      ),
      const SizedBox(height: 8),
      // 左右内边距和卡片间距与上面的波形卡片保持一致，两行卡片左右边界对齐。
      Padding(
        padding: const EdgeInsets.symmetric(horizontal: kWaveformContentInset),
        child: SizedBox(
          height: 224,
          child: AnimatedBuilder(
            animation: controller.calibrationChanges,
            builder: (context, _) => Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(
                  child: _AccelerometerCalibrationCard(
                    controller: controller,
                    mode: AccelerometerCalibrationMode.sixFace,
                  ),
                ),
                const SizedBox(width: kWaveformCardGap),
                Expanded(
                  child: _AccelerometerCalibrationCard(
                    controller: controller,
                    mode: AccelerometerCalibrationMode.level,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    ],
  );
}

class _AccelerometerCalibrationCard extends StatelessWidget {
  const _AccelerometerCalibrationCard({
    required this.controller,
    required this.mode,
  });

  final ConnectionController controller;
  final AccelerometerCalibrationMode mode;

  bool get isSixFace => mode == AccelerometerCalibrationMode.sixFace;

  bool get isThisCalibrationActive {
    final status = controller.calibrationStatus;
    return status.active &&
        status.isAccelerometer &&
        (isSixFace
            ? status.accelerometerMode == 'six_face'
            : status.accelerometerMode == 'level');
  }

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    final status = controller.calibrationStatus;
    final activeOtherCalibration = status.active && !isThisCalibrationActive;
    final canStart =
        controller.isConnected &&
        !status.active &&
        controller.calibrationError == null;
    final step = isSixFace ? status.currentStep.clamp(1, 6) : 1;
    final orientation = isSixFace ? _AccelOrientation.fromStep(step) : null;
    final busy = isThisCalibrationActive;
    final waiting = busy && status.isWaitingOrientation;
    final title = isSixFace ? '六面校准' : '水平校准';
    final subtitle = isSixFace ? '按飞控提示依次摆放，摆好后确认当前姿态' : '将飞机放在平整水平面上，保持静止后开始';
    final message = controller.calibrationError ?? (busy ? status.message : '');

    return MacPanel(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Row(
        children: [
          SizedBox(
            width: 150,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _CalibrationAircraft(orientation: orientation),
                const SizedBox(height: 6),
                Text(
                  orientation?.label ?? '水平放置',
                  style: theme.typography.caption1.copyWith(
                    color: colors.accent,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Row(
                  children: [
                    Text(title, style: theme.typography.title3),
                    const Spacer(),
                    if (busy && isSixFace)
                      Text(
                        '第 $step/6 面',
                        style: theme.typography.caption1.copyWith(
                          color: colors.accent,
                        ),
                      ),
                  ],
                ),
                const SizedBox(height: 4),
                Text(subtitle, style: theme.typography.caption1),
                const SizedBox(height: 12),
                if (busy) ...[
                  LinearProgressIndicator(
                    value: status.progress.clamp(0, 100) / 100,
                    minHeight: 5,
                    borderRadius: BorderRadius.circular(3),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    message.isEmpty
                        ? (isSixFace ? '请按图示摆放并保持静止' : '飞控正在执行水平校准')
                        : message,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: theme.typography.caption1,
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      if (isSixFace)
                        Expanded(
                          child: PushButton(
                            controlSize: ControlSize.large,
                            secondary: !waiting,
                            onPressed: waiting
                                ? () => controller.confirmAccelerometerPosition(
                                    step,
                                  )
                                : null,
                            child: Text(waiting ? '确认当前姿态' : '等待飞控提示'),
                          ),
                        )
                      else
                        Expanded(
                          child: Text(
                            '保持静止，等待飞控完成…',
                            style: theme.typography.caption1,
                          ),
                        ),
                      const SizedBox(width: 8),
                      PushButton(
                        controlSize: ControlSize.large,
                        secondary: true,
                        onPressed: () => controller.cancelCalibration(),
                        child: const Text('取消'),
                      ),
                    ],
                  ),
                ] else ...[
                  Text(
                    activeOtherCalibration
                        ? '另一项加速度计校准正在进行'
                        : controller.isConnected
                        ? '开始后请严格按照图示摆放飞机'
                        : '连接飞控后才能开始校准',
                    style: theme.typography.caption1,
                  ),
                  const SizedBox(height: 10),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: PushButton(
                      controlSize: ControlSize.large,
                      onPressed: canStart
                          ? () => controller.startAccelerometerCalibration(mode)
                          : null,
                      child: Text(isSixFace ? '开始六面校准' : '开始水平校准'),
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

enum _AccelOrientation {
  level('水平放置'),
  left('左侧朝下'),
  right('右侧朝下'),
  noseDown('机头朝下'),
  noseUp('机头朝上'),
  inverted('倒放');

  const _AccelOrientation(this.label);

  final String label;

  static _AccelOrientation fromStep(int step) => switch (step) {
    2 => left,
    3 => right,
    4 => noseDown,
    5 => noseUp,
    6 => inverted,
    _ => level,
  };
}

class _CalibrationAircraft extends StatelessWidget {
  const _CalibrationAircraft({required this.orientation});

  final _AccelOrientation? orientation;

  @override
  Widget build(BuildContext context) {
    // 统一走色板：边框和文字取强调色，底色取其淡化的同色。
    //
    // 不用 MacosTheme.primaryColor：它跟随窗口焦点变化，窗口不是主窗口时
    // 会退化成灰色，图框一失焦就变色。
    final colors = context.mgColors;
    final label = orientation?.label ?? '等待开始';
    return Container(
      width: 142,
      height: 128,
      decoration: BoxDecoration(
        color: colors.accentSurface,
        border: Border.all(color: colors.accent, width: 2),
        borderRadius: BorderRadius.circular(14),
      ),
      child: Center(
        child: Text(
          label,
          textAlign: TextAlign.center,
          style: MgTypography.sans(
            MgTypography.title.copyWith(
              color: colors.accent,
              fontWeight: FontWeight.w700,
            ),
          ),
        ),
      ),
    );
  }
}

/// PID 调参页面外壳：按固件决定显示哪些调参标签页。
class _PidTuningPage extends StatelessWidget {
  const _PidTuningPage({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: controller.profileChanges,
    builder: (context, _) {
      final labels = switch (controller.controlProfile) {
        VehicleControlProfile.copter => const ['多旋翼 PID'],
        VehicleControlProfile.fixedWing => const ['固定翼 PID'],
        VehicleControlProfile.planeVtol => const ['固定翼 PID', 'VTOL PID'],
        VehicleControlProfile.unknown => const ['PID 调参'],
      };
      return _MacTabSurface(
        key: ValueKey(labels.join('|')),
        labels: labels,
        children: [
          for (var index = 0; index < labels.length; index++)
            _PidParametersPage(
              controller: controller,
              profile: controller.controlProfile,
              vtol: index == 1,
            ),
        ],
      );
    },
  );
}

/// 调参层级。基础档只放自稳 P 和速率 P/I/D；扩展档补齐前馈、滤波、限幅
/// 以及位置环，避免一屏同时铺开四十多个输入框。
enum _PidLevel { basic, extended }

class _PidParametersPage extends StatefulWidget {
  const _PidParametersPage({
    required this.controller,
    required this.profile,
    this.vtol = false,
  });

  final ConnectionController controller;
  final VehicleControlProfile profile;
  final bool vtol;

  @override
  State<_PidParametersPage> createState() => _PidParametersPageState();
}

class _PidParametersPageState extends State<_PidParametersPage> {
  final ScrollController _scrollController = ScrollController();
  late final MacosTabController _levels = MacosTabController(
    initialIndex: 0,
    length: 2,
  );
  int _levelIndex = 0;

  _PidLevel get _level =>
      _levelIndex == 0 ? _PidLevel.basic : _PidLevel.extended;

  @override
  void initState() {
    super.initState();
    _levels.addListener(_handleLevelChanged);
    _loadVisibleParameters();
  }

  void _handleLevelChanged() {
    final index = _levels.index;
    if (!mounted || index == _levelIndex) return;
    setState(() => _levelIndex = index);
  }

  @override
  void dispose() {
    _levels.removeListener(_handleLevelChanged);
    _levels.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(covariant _PidParametersPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.profile != widget.profile || oldWidget.vtol != widget.vtol) {
      _loadVisibleParameters();
    }
  }

  /// 两个层级一次全部登记，切换档位时不再往返后端补元数据。
  void _loadVisibleParameters() {
    final names = <String>{
      for (final level in _PidLevel.values)
        for (final section in _pidSections(widget.profile, widget.vtol, level))
          for (final card in section.cards)
            for (final field in card.fields) field.parameterName,
    }.toList(growable: false);
    if (names.isEmpty) return;
    widget.controller.loadParameterMetadata(names);
  }

  @override
  Widget build(BuildContext context) => SizedBox.expand(
    // 面板必须撑满标签视图，否则 Stack 会垂直居中面板，在标签卡下方留出空档。
    child: AnimatedBuilder(
      animation: Listenable.merge([
        widget.controller.parameterChanges,
        widget.controller.profileChanges,
      ]),
      builder: (context, _) {
        final controller = widget.controller;
        if (!controller.isConnected) {
          return _ParameterEmptyState(
            scrollController: _scrollController,
            message: '连接飞控后读取 PID 参数',
          );
        }
        if (!controller.parametersLoaded) {
          return _ParameterLoadingState(
            scrollController: _scrollController,
            controller: controller,
            title: 'PID 参数',
          );
        }
        final profile = controller.controlProfile;
        if (profile == VehicleControlProfile.unknown) {
          return _ParameterEmptyState(
            scrollController: _scrollController,
            message: '暂不支持当前固件或机型的 PID 页面',
          );
        }
        final sections = _pidSections(profile, widget.vtol, _level);
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(
                _tabBodyInset,
                8,
                _tabBodyInset,
                0,
              ),
              child: Align(
                alignment: Alignment.centerLeft,
                child: MacosSegmentedControl(
                  controller: _levels,
                  tabs: const [
                    MacosTab(label: '基础'),
                    MacosTab(label: '扩展'),
                  ],
                ),
              ),
            ),
            const SizedBox(height: _headerGap),
            Expanded(
              child: SingleChildScrollView(
                controller: _scrollController,
                // 外层标签视图已提供 12pt，这里补足到统一标准的 24pt。
                padding: const EdgeInsets.fromLTRB(
                  _tabBodyInset,
                  0,
                  _tabBodyInset,
                  16,
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    for (final section in sections) ...[
                      _PidSectionHeader(title: section.title),
                      const SizedBox(height: _headerGap),
                      _PidCardGrid(
                        controller: controller,
                        cards: section.cards,
                      ),
                      const SizedBox(height: _sectionGap),
                    ],
                  ],
                ),
              ),
            ),
          ],
        );
      },
    ),
  );
}

class _PidGainField {
  const _PidGainField(this.label, this.parameterName);

  final String label;
  final String parameterName;
}

class _PidCard {
  const _PidCard(this.title, this.fields);

  final String title;
  final List<_PidGainField> fields;
}

class _PidSection {
  const _PidSection(this.title, this.cards);

  final String title;
  final List<_PidCard> cards;
}

const _pidAxes = [('横滚', 'RLL'), ('俯仰', 'PIT'), ('偏航', 'YAW')];

/// 三轴速率卡：基础档给 P/I/D，扩展档给前馈、积分上限与三档滤波。
_PidCard _pidRateCard(String title, String base, _PidLevel level) =>
    switch (level) {
      _PidLevel.basic => _PidCard(title, [
        _PidGainField('比例 (P)', '${base}_P'),
        _PidGainField('积分 (I)', '${base}_I'),
        _PidGainField('微分 (D)', '${base}_D'),
      ]),
      _PidLevel.extended => _PidCard(title, [
        _PidGainField('前馈 (FF)', '${base}_FF'),
        _PidGainField('积分上限 (I Max)', '${base}_IMAX'),
        _PidGainField('误差滤波 (FLTE)', '${base}_FLTE'),
        _PidGainField('微分滤波 (FLTD)', '${base}_FLTD'),
        _PidGainField('目标滤波 (FLTT)', '${base}_FLTT'),
      ]),
    };

List<_PidSection> _pidSections(
  VehicleControlProfile profile,
  bool vtol,
  _PidLevel level,
) {
  // 垂起的多旋翼部分与多旋翼一一对应，只是把 ATC_/PSC_ 换成 Q_A_/Q_P_；
  // 参数名已在 ArduPilot 4.8 参数库里逐条核对过。
  if (vtol || profile == VehicleControlProfile.copter) {
    return _multirotorPidSections(vtol, level);
  }
  if (profile == VehicleControlProfile.fixedWing ||
      profile == VehicleControlProfile.planeVtol) {
    return _fixedWingPidSections(level);
  }
  return const [];
}

List<_PidSection> _multirotorPidSections(bool vtol, _PidLevel level) {
  final angle = vtol ? 'Q_A_ANG' : 'ATC_ANG';
  final rate = vtol ? 'Q_A_RAT' : 'ATC_RAT';
  final accel = vtol ? 'Q_A_ACC' : 'ATC_ACC';
  final rateMax = vtol ? 'Q_A_RATE' : 'ATC_RATE';
  final attitude = vtol ? 'Q_A' : 'ATC';
  final psc = vtol ? 'Q_P' : 'PSC';

  if (level == _PidLevel.basic) {
    return [
      _PidSection('自稳 P', [
        for (final (label, axis) in _pidAxes)
          _PidCard('自稳 $label', [
            _PidGainField('比例 (P)', '${angle}_${axis}_P'),
          ]),
      ]),
      _PidSection('速率 PID', [
        for (final (label, axis) in _pidAxes)
          _pidRateCard('$label速率', '${rate}_$axis', level),
      ]),
    ];
  }
  return [
    _PidSection('速率滤波与前馈', [
      for (final (label, axis) in _pidAxes)
        _pidRateCard('$label速率', '${rate}_$axis', level),
    ]),
    _PidSection('响应限制', [
      _PidCard('最大角加速度 (ACCEL MAX)', [
        _PidGainField('横滚', '${accel}_R_MAX'),
        _PidGainField('俯仰', '${accel}_P_MAX'),
        _PidGainField('偏航', '${accel}_Y_MAX'),
      ]),
      _PidCard('最大角速度 (RATE MAX)', [
        _PidGainField('横滚', '${rateMax}_R_MAX'),
        _PidGainField('俯仰', '${rateMax}_P_MAX'),
        _PidGainField('偏航', '${rateMax}_Y_MAX'),
      ]),
      _PidCard('姿态平滑', [
        _PidGainField('输入时间常数 (INPUT TC)', '${attitude}_INPUT_TC'),
        _PidGainField('角度上限 (ANGLE MAX)', '${attitude}_ANGLE_MAX'),
      ]),
    ]),
    _PidSection('位置与速度', [
      _PidCard('水平位置', [_PidGainField('位置 P', '${psc}_NE_POS_P')]),
      _PidCard('水平速度', [
        _PidGainField('比例 (P)', '${psc}_NE_VEL_P'),
        _PidGainField('积分 (I)', '${psc}_NE_VEL_I'),
        _PidGainField('微分 (D)', '${psc}_NE_VEL_D'),
        _PidGainField('积分上限 (I Max)', '${psc}_NE_VEL_IMAX'),
      ]),
      _PidCard('垂直位置', [_PidGainField('位置 P', '${psc}_D_POS_P')]),
      _PidCard('垂直速度', [
        _PidGainField('比例 (P)', '${psc}_D_VEL_P'),
        _PidGainField('积分 (I)', '${psc}_D_VEL_I'),
        _PidGainField('积分上限 (I Max)', '${psc}_D_VEL_IMAX'),
      ]),
    ]),
  ];
}

List<_PidSection> _fixedWingPidSections(_PidLevel level) {
  const axes = [('横滚', 'RLL'), ('俯仰', 'PTCH'), ('偏航', 'YAW')];
  if (level == _PidLevel.basic) {
    return [
      _PidSection('速率 PID', [
        for (final (label, axis) in axes)
          _pidRateCard('$label速率', '${axis}_RATE', level),
      ]),
    ];
  }
  return [
    _PidSection('速率滤波与前馈', [
      for (final (label, axis) in axes)
        _pidRateCard('$label速率', '${axis}_RATE', level),
    ]),
    _PidSection('舵机与姿态', [
      _PidCard('滚转', [_PidGainField('时间常数 (TCONST)', 'RLL2SRV_TCONST')]),
      _PidCard('俯仰', [
        _PidGainField('时间常数 (TCONST)', 'PTCH2SRV_TCONST'),
        _PidGainField('俯仰→滚转混控', 'PTCH2SRV_RLL'),
      ]),
      _PidCard('偏航', [
        _PidGainField('阻尼 (DAMP)', 'YAW2SRV_DAMP'),
        _PidGainField('积分 (INT)', 'YAW2SRV_INT'),
        _PidGainField('积分上限 (I Max)', 'YAW2SRV_IMAX'),
        _PidGainField('偏航→滚转补偿', 'YAW2SRV_RLL'),
        _PidGainField('侧滑补偿 (SLIP)', 'YAW2SRV_SLIP'),
      ]),
    ]),
    _PidSection('TECS 能量控制', [
      _PidCard('高度与速度', [
        _PidGainField('时间常数', 'TECS_TIME_CONST'),
        _PidGainField('速度权重', 'TECS_SPDWEIGHT'),
        _PidGainField('积分增益', 'TECS_INTEG_GAIN'),
        _PidGainField('油门阻尼', 'TECS_THR_DAMP'),
        _PidGainField('俯仰阻尼', 'TECS_PTCH_DAMP'),
      ]),
      _PidCard('爬升与下降', [
        _PidGainField('最大爬升率', 'TECS_CLMB_MAX'),
        _PidGainField('最大下降率', 'TECS_SINK_MAX'),
      ]),
    ]),
  ];
}

class _PidSectionHeader extends StatelessWidget {
  const _PidSectionHeader({required this.title});

  final String title;

  @override
  Widget build(BuildContext context) =>
      Text(title, style: MacosTheme.of(context).typography.title3);
}

/// 卡片按可用宽度自动分栏；卡片内部的字段再按两列排。
class _PidCardGrid extends StatelessWidget {
  const _PidCardGrid({required this.controller, required this.cards});

  final ConnectionController controller;
  final List<_PidCard> cards;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      if (cards.isEmpty) return const SizedBox.shrink();
      final columns = constraints.maxWidth >= 1180
          ? math.min(4, cards.length)
          : constraints.maxWidth >= 800
          ? math.min(3, cards.length)
          : constraints.maxWidth >= 540
          ? math.min(2, cards.length)
          : 1;
      final width = (constraints.maxWidth - (columns - 1) * _cardGap) / columns;
      return Wrap(
        spacing: _cardGap,
        runSpacing: _cardGap,
        children: [
          for (final card in cards)
            SizedBox(
              width: width,
              child: _PidCardView(card: card, controller: controller),
            ),
        ],
      );
    },
  );
}

class _PidCardView extends StatelessWidget {
  const _PidCardView({required this.card, required this.controller});

  final _PidCard card;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: context.mgColors.surface,
        border: Border.all(color: context.mgColors.divider),
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            card.title,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.typography.headline,
          ),
          const SizedBox(height: 10),
          LayoutBuilder(
            builder: (context, constraints) {
              final columns = card.fields.length <= 1 ? 1 : 2;
              final width =
                  (constraints.maxWidth - (columns - 1) * 8) / columns;
              return Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  for (final field in card.fields)
                    SizedBox(
                      width: width,
                      child: _PidGainTile(field: field, controller: controller),
                    ),
                ],
              );
            },
          ),
        ],
      ),
    );
  }
}

class _PidGainTile extends StatelessWidget {
  const _PidGainTile({required this.field, required this.controller});

  final _PidGainField field;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final parameter = controller.parameters[field.parameterName];
    // 外层已经是卡片，这里只排标签和输入框，避免卡片套卡片。
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          field.label,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: theme.typography.caption1.copyWith(
            color: context.mgColors.textSecondary,
          ),
        ),
        const SizedBox(height: 5),
        _ParameterValueControl(
          parameter: parameter,
          controller: controller,
          center: true,
        ),
      ],
    );
  }
}

class _BatteryConfigurationPage extends StatelessWidget {
  const _BatteryConfigurationPage();

  @override
  Widget build(BuildContext context) => const _BlankConfigurationPage();
}

/// 一条解锁检查项。位值与 ArduPilot 的 bitmask 定义一致。
class _ArmingCheck {
  const _ArmingCheck(
    this.bit,
    this.label, {
    this.fixedWingOnly = false,
    this.multirotorOnly = false,
  });

  final int bit;
  final String label;
  final bool fixedWingOnly;
  final bool multirotorOnly;
}

/// 解锁检查清单。旧固件写 ARMING_CHECK（置位=执行检查），
/// 新固件写 ARMING_SKIPCHK（置位=跳过检查），两者位值相同、语义相反。
const _armingCheckCatalog = [
  _ArmingCheck(2, '气压计'),
  _ArmingCheck(4, '罗盘'),
  _ArmingCheck(8, 'GPS 锁定'),
  _ArmingCheck(16, 'INS'),
  _ArmingCheck(32, '参数'),
  _ArmingCheck(64, '遥控通道'),
  _ArmingCheck(128, '板载电压'),
  _ArmingCheck(256, '电池电量'),
  _ArmingCheck(512, '空速计', fixedWingOnly: true),
  _ArmingCheck(1024, '日志可用性'),
  _ArmingCheck(2048, '硬件安全开关'),
  _ArmingCheck(4096, 'GPS 配置'),
  _ArmingCheck(8192, '系统'),
  _ArmingCheck(16384, '任务'),
  _ArmingCheck(32768, '测距仪'),
  _ArmingCheck(65536, '相机'),
  _ArmingCheck(131072, 'AuxAuth'),
  _ArmingCheck(262144, '视觉里程计', multirotorOnly: true),
  _ArmingCheck(524288, 'FFT'),
];

List<_ArmingCheck> _armingChecksFor(VehicleControlProfile profile) {
  final fixedWing =
      profile == VehicleControlProfile.fixedWing ||
      profile == VehicleControlProfile.planeVtol;
  final multirotor = profile == VehicleControlProfile.copter;
  return [
    for (final check in _armingCheckCatalog)
      if (!check.fixedWingOnly || fixedWing)
        if (!check.multirotorOnly || multirotor) check,
  ];
}

/// 新固件优先用 ARMING_SKIPCHK，旧固件回退到 ARMING_CHECK。
String? _armingParameterName(ConnectionController controller) {
  if (controller.parameters.containsKey('ARMING_SKIPCHK')) {
    return 'ARMING_SKIPCHK';
  }
  if (controller.parameters.containsKey('ARMING_CHECK')) return 'ARMING_CHECK';
  return null;
}

class _ArmingChecksPage extends StatefulWidget {
  const _ArmingChecksPage({
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<_ArmingChecksPage> createState() => _ArmingChecksPageState();
}

class _ArmingChecksPageState extends State<_ArmingChecksPage> {
  Future<void> _write(
    ConnectionController controller,
    String parameterName,
    bool inverted,
    List<_ArmingCheck> checks,
    bool Function(_ArmingCheck) enabled,
  ) {
    final current = controller.parameters[parameterName]?.value.round() ?? 0;
    var mask = current;
    for (final check in checks) {
      // 置位含义随参数而变：SKIPCHK 置位表示跳过，CHECK 置位表示执行。
      final bitSet = inverted ? !enabled(check) : enabled(check);
      mask = bitSet ? (mask | check.bit) : (mask & ~check.bit);
    }
    return controller.setParameter(parameterName, mask.toDouble());
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.profileChanges,
    ]),
    builder: (context, _) {
      final controller = widget.controller;
      if (!controller.isConnected) {
        return _ParameterEmptyState(
          scrollController: widget.scrollController,
          message: '连接飞控后读取解锁检查项',
        );
      }
      if (!controller.parametersLoaded) {
        return _ParameterLoadingState(
          scrollController: widget.scrollController,
          controller: controller,
          title: '解锁检查项',
        );
      }
      final parameterName = _armingParameterName(controller);
      if (parameterName == null) {
        return _ParameterEmptyState(
          scrollController: widget.scrollController,
          message: '当前固件未提供解锁检查参数',
        );
      }
      final inverted = parameterName == 'ARMING_SKIPCHK';
      final mask = controller.parameters[parameterName]?.value.round() ?? 0;
      final checks = _armingChecksFor(controller.controlProfile);
      final busy = controller.isParameterWriting(parameterName);
      bool enabled(_ArmingCheck check) =>
          inverted ? (mask & check.bit) == 0 : (mask & check.bit) != 0;
      final allEnabled = checks.every(enabled);

      return SingleChildScrollView(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(_pageMargin, 22, _pageMargin, 32),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (controller.parameterWriteError != null) ...[
              _ParameterWriteStatus(controller: controller),
              const SizedBox(height: _headerGap),
            ],
            // 和其它配置页同一观感：整块用带边框的面板框住，内部按行/列打分隔线，
            // 行高取标准开关行高度，比表格行更紧凑。
            DecoratedBox(
              decoration: BoxDecoration(
                color: context.mgColors.surface,
                border: Border.all(color: context.mgColors.divider),
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
              ),
              child: Padding(
                // 垂直方向不留内边距：否则上下多出来的空白会算进首/末行，
                // 让它们比中间行更高。行高本身已经留出了呼吸空间。
                padding: const EdgeInsets.symmetric(horizontal: 4),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    SizedBox(
                      height: MgMetrics.controlHeight,
                      child: _ArmingToggle(
                        label: '全部',
                        value: allEnabled,
                        enabled: !busy,
                        onChanged: (value) => unawaited(
                          _write(
                            controller,
                            parameterName,
                            inverted,
                            checks,
                            (_) => value,
                          ),
                        ),
                      ),
                    ),
                    Divider(
                      height: 1,
                      thickness: 1,
                      color: context.mgColors.divider,
                    ),
                    LayoutBuilder(
                      builder: (context, constraints) {
                        final columns = constraints.maxWidth >= 900
                            ? 3
                            : constraints.maxWidth >= 560
                            ? 2
                            : 1;
                        // 按列切行，行间与列间都打分隔线，形成参数表观感。
                        final rows = [
                          for (
                            var start = 0;
                            start < checks.length;
                            start += columns
                          )
                            checks.sublist(
                              start,
                              math.min(start + columns, checks.length),
                            ),
                        ];
                        return Column(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            for (var row = 0; row < rows.length; row++) ...[
                              if (row > 0)
                                Divider(
                                  height: 1,
                                  thickness: 1,
                                  color: context.mgColors.divider,
                                ),
                              SizedBox(
                                height: MgMetrics.controlHeight,
                                child: Row(
                                  crossAxisAlignment:
                                      CrossAxisAlignment.stretch,
                                  children: [
                                    for (
                                      var column = 0;
                                      column < columns;
                                      column++
                                    ) ...[
                                      if (column > 0)
                                        SizedBox(
                                          width: 1,
                                          child: ColoredBox(
                                            color: context.mgColors.divider,
                                          ),
                                        ),
                                      Expanded(
                                        child: column < rows[row].length
                                            ? _ArmingToggle(
                                                label: rows[row][column].label,
                                                value: enabled(
                                                  rows[row][column],
                                                ),
                                                enabled: !busy,
                                                onChanged: (value) => unawaited(
                                                  _write(
                                                    controller,
                                                    parameterName,
                                                    inverted,
                                                    checks,
                                                    (item) =>
                                                        item.bit ==
                                                            rows[row][column]
                                                                .bit
                                                        ? value
                                                        : enabled(item),
                                                  ),
                                                ),
                                              )
                                            : const SizedBox.shrink(),
                                      ),
                                    ],
                                  ],
                                ),
                              ),
                            ],
                          ],
                        );
                      },
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      );
    },
  );
}

class _ArmingToggle extends StatelessWidget {
  const _ArmingToggle({
    required this.label,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final String label;
  final bool value;
  final bool enabled;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 8),
    child: Row(
      children: [
        Expanded(
          child: Text(
            label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacosTheme.of(context).typography.body,
          ),
        ),
        const SizedBox(width: 8),
        MacosSwitch(
          value: value,
          size: ControlSize.small,
          onChanged: enabled ? onChanged : null,
          semanticLabel: label,
        ),
      ],
    ),
  );
}

class _BlankConfigurationPage extends StatelessWidget {
  const _BlankConfigurationPage();

  @override
  Widget build(BuildContext context) => const SizedBox.expand();
}

class _MotorOutputPage extends StatefulWidget {
  const _MotorOutputPage({
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<_MotorOutputPage> createState() => _MotorOutputPageState();
}

class _MotorOutputPageState extends State<_MotorOutputPage> {
  static const _headers = [
    '序号',
    'PWM 值',
    '反向',
    '功能',
    'PWM 最小值',
    'PWM 中位值',
    'PWM 最大值',
  ];

  @override
  void initState() {
    super.initState();
    widget.controller.loadParameterMetadata([
      for (var channel = 1; channel <= 16; channel++) ...[
        'SERVO${channel}_FUNCTION',
        'SERVO${channel}_REVERSED',
        'SERVO${channel}_MIN',
        'SERVO${channel}_TRIM',
        'SERVO${channel}_MAX',
      ],
    ]);
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.telemetryChanges,
    ]),
    builder: (context, _) {
      // 参数未读完时所有输出通道都是空值，先给出加载进度而不是空表格。
      if (!widget.controller.parametersLoaded) {
        return _ParameterLoadingState(
          scrollController: widget.scrollController,
          controller: widget.controller,
          title: '电机输出参数',
        );
      }
      return SingleChildScrollView(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(_pageMargin, 22, _pageMargin, 32),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (widget.controller.parameterWriteError != null) ...[
              _ParameterWriteStatus(controller: widget.controller),
              const SizedBox(height: 10),
            ],
            DecoratedBox(
              decoration: BoxDecoration(
                color: context.mgColors.surface,
                border: Border.all(color: context.mgColors.divider),
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
                child: SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: ConstrainedBox(
                    constraints: const BoxConstraints(minWidth: 1040),
                    child: Table(
                      columnWidths: const {
                        0: FixedColumnWidth(76),
                        1: FixedColumnWidth(220),
                        2: FixedColumnWidth(90),
                        3: FlexColumnWidth(1.5),
                        4: FixedColumnWidth(132),
                        5: FixedColumnWidth(132),
                        6: FixedColumnWidth(132),
                      },
                      children: [
                        TableRow(
                          decoration: BoxDecoration(
                            color: context.mgColors.accentSurface,
                          ),
                          children: [
                            for (final header in _headers)
                              _HeaderCell(label: header),
                          ],
                        ),
                        for (var channel = 1; channel <= 16; channel++)
                          TableRow(
                            decoration: BoxDecoration(
                              border: Border(
                                bottom: BorderSide(
                                  color: context.mgColors.divider,
                                  width: 0.6,
                                ),
                              ),
                            ),
                            children: [
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _ChannelNumberCell(number: channel),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _LiveOutputCell(
                                  value:
                                      widget.controller.servoOutputsValid &&
                                          channel <=
                                              widget
                                                  .controller
                                                  .servoOutputs
                                                  .length
                                      ? widget.controller.servoOutputs[channel -
                                            1]
                                      : null,
                                  minimum: _parameterNumeric(
                                    widget.controller,
                                    'SERVO${channel}_MIN',
                                    fallback: 1100,
                                  ),
                                  maximum: _parameterNumeric(
                                    widget.controller,
                                    'SERVO${channel}_MAX',
                                    fallback: 1900,
                                  ),
                                ),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _ReverseCell(
                                  parameter: widget
                                      .controller
                                      .parameters['SERVO${channel}_REVERSED'],
                                  controller: widget.controller,
                                ),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _FunctionCell(
                                  parameter: widget
                                      .controller
                                      .parameters['SERVO${channel}_FUNCTION'],
                                  controller: widget.controller,
                                ),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _LimitCell(
                                  parameter: widget
                                      .controller
                                      .parameters['SERVO${channel}_MIN'],
                                  tone: _LimitTone.min,
                                  controller: widget.controller,
                                ),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _LimitCell(
                                  parameter: widget
                                      .controller
                                      .parameters['SERVO${channel}_TRIM'],
                                  tone: _LimitTone.mid,
                                  controller: widget.controller,
                                ),
                              ),
                              TableCell(
                                verticalAlignment:
                                    TableCellVerticalAlignment.middle,
                                child: _LimitCell(
                                  parameter: widget
                                      .controller
                                      .parameters['SERVO${channel}_MAX'],
                                  tone: _LimitTone.max,
                                  controller: widget.controller,
                                ),
                              ),
                            ],
                          ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      );
    },
  );
}

class _HeaderCell extends StatelessWidget {
  const _HeaderCell({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: 42,
    child: Center(
      child: Text(
        label,
        style: MacosTheme.of(
          context,
        ).typography.caption1.copyWith(fontWeight: FontWeight.w600),
      ),
    ),
  );
}

class _ChannelNumberCell extends StatelessWidget {
  const _ChannelNumberCell({required this.number});

  final int number;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: Container(
        width: 24,
        height: 24,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: context.mgColors.accentSurface,
          shape: BoxShape.circle,
          border: Border.all(
            color: context.mgColors.accent.withValues(alpha: 0.35),
          ),
        ),
        child: Text(
          '$number',
          style: MgTypography.sans(
            MgTypography.captionStrong.copyWith(color: context.mgColors.accent),
          ),
        ),
      ),
    ),
  );
}

class _LiveOutputCell extends StatelessWidget {
  const _LiveOutputCell({
    required this.value,
    required this.minimum,
    required this.maximum,
  });

  final int? value;
  final double minimum;
  final double maximum;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 8),
      child: Center(
        child: MgMotorOutputBar(
          value: value,
          minimum: minimum,
          maximum: maximum,
          semanticLabel: 'PWM 当前输出',
        ),
      ),
    ),
  );
}

double _parameterNumeric(
  ConnectionController controller,
  String name, {
  required double fallback,
}) => controller.parameter(name)?.value ?? fallback;

class _ReverseCell extends StatelessWidget {
  const _ReverseCell({required this.parameter, required this.controller});

  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: parameter == null
          ? Text('—', style: MacosTheme.of(context).typography.body)
          : MacosSwitch(
              value: parameter!.value != 0,
              onChanged: parameter!.readOnly || !controller.isConnected
                  ? null
                  : (value) => unawaited(
                      controller.setParameter(parameter!.name, value ? 1 : 0),
                    ),
              size: ControlSize.small,
              semanticLabel: '反向输出',
            ),
    ),
  );
}

class _FunctionCell extends StatelessWidget {
  const _FunctionCell({required this.parameter, required this.controller});

  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: MgMetrics.rowHeight,
    child: Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 5),
        child: SizedBox(
          width: double.infinity,
          child: _ParameterValueControl(
            parameter: parameter,
            controller: controller,
            center: false,
            popupWidth: 108,
          ),
        ),
      ),
    ),
  );
}

enum _LimitTone { min, mid, max }

String _formatParameterValue(double value) =>
    value.isFinite && value == value.roundToDouble()
    ? value.toInt().toString()
    : value.toString();

class _ParameterValueControl extends StatelessWidget {
  const _ParameterValueControl({
    required this.parameter,
    required this.controller,
    this.center = false,
    this.popupWidth = 132,
  });

  final VehicleParameter? parameter;
  final ConnectionController controller;
  final bool center;
  final double popupWidth;

  @override
  Widget build(BuildContext context) {
    final current = parameter;
    if (current == null || !current.value.isFinite) {
      return _ValueWell(text: '—', enabled: false, center: center);
    }
    final matchingOptions = current.options.any(
      (option) => option.value == current.value,
    );
    final options = current.options.length > 1
        ? [
            ...current.options,
            if (!matchingOptions)
              ParameterOption(
                value: current.value,
                label: '未收录（${_formatParameterValue(current.value)}）',
              ),
          ]
        : const <ParameterOption>[];
    if (options.length > 1) {
      final labels = {for (final option in options) option.value: option.label};
      return AdaptivePopupButton<double>(
        value: current.value,
        options: options.map((option) => option.value).toList(growable: false),
        labelOf: (value) => labels[value] ?? _formatParameterValue(value),
        maxContentWidth: popupWidth,
        onChanged: !controller.isConnected || current.readOnly
            ? null
            : (value) =>
                  unawaited(controller.setParameter(current.name, value)),
      );
    }
    return _ValueWell(
      text: _formatParameterValue(current.value),
      enabled: controller.isConnected && !current.readOnly,
      center: center,
      onSubmitted: (value) => controller.setParameter(
        current.name,
        _clampToDescriptor(current, value),
      ),
    );
  }

  double _clampToDescriptor(VehicleParameter parameter, double value) {
    var clamped = value;
    final minimum = parameter.minimum;
    final maximum = parameter.maximum;
    if (minimum != null && clamped < minimum) clamped = minimum;
    if (maximum != null && clamped > maximum) clamped = maximum;
    return clamped;
  }
}

class _ValueWell extends StatefulWidget {
  const _ValueWell({
    required this.text,
    required this.enabled,
    required this.center,
    this.fillColor,
    this.onSubmitted,
  });

  final String text;
  final bool enabled;
  final bool center;
  final Color? fillColor;
  final Future<bool> Function(double value)? onSubmitted;

  @override
  State<_ValueWell> createState() => _ValueWellState();
}

class _ValueWellState extends State<_ValueWell> {
  late final TextEditingController _textController = TextEditingController(
    text: widget.text,
  );
  late final FocusNode _focusNode = FocusNode();
  late String _lastCommittedText = widget.text;

  @override
  void initState() {
    super.initState();
    _focusNode.addListener(_handleFocusChange);
  }

  void _handleFocusChange() {
    if (!_focusNode.hasFocus) unawaited(_commitCurrentValue());
  }

  Future<bool> _commitCurrentValue() async {
    final text = _textController.text.trim();
    if (text == _lastCommittedText) return true;
    final value = double.tryParse(text);
    final submit = widget.onSubmitted;
    if (value == null || !value.isFinite || submit == null) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
      return false;
    }

    _lastCommittedText = text;
    final success = await submit(value);
    if (!success && mounted) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
    }
    return success;
  }

  @override
  void didUpdateWidget(_ValueWell oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!_focusNode.hasFocus && oldWidget.text != widget.text) {
      _textController.text = widget.text;
      _lastCommittedText = widget.text;
    }
  }

  @override
  void dispose() {
    _focusNode.removeListener(_handleFocusChange);
    _textController.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final textColor = context.mgColors.textPrimary;
    return MacosTextField(
      controller: _textController,
      focusNode: _focusNode,
      enabled: widget.enabled,
      readOnly: !widget.enabled,
      placeholder: widget.text,
      placeholderStyle: MgTypography.sans(
        MgTypography.body.copyWith(color: textColor),
      ),
      style: MgTypography.sans(
        MgTypography.bodyStrong.copyWith(color: textColor),
      ),
      textAlign: widget.center ? TextAlign.center : TextAlign.start,
      keyboardType: const TextInputType.numberWithOptions(
        decimal: true,
        signed: true,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: widget.fillColor ?? context.mgColors.well,
        borderRadius: BorderRadius.circular(MgMetrics.controlRadius),
        border: Border.all(color: context.mgColors.divider),
      ),
      onSubmitted: (value) {
        unawaited(
          _commitCurrentValue().whenComplete(() => _focusNode.unfocus()),
        );
      },
    );
  }
}

class _LimitCell extends StatelessWidget {
  const _LimitCell({
    required this.parameter,
    required this.tone,
    required this.controller,
  });

  final VehicleParameter? parameter;
  final _LimitTone tone;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final toneColor = switch (tone) {
      _LimitTone.min => colors.danger,
      _LimitTone.mid => colors.accent,
      _LimitTone.max => colors.success,
    };
    return SizedBox(
      height: MgMetrics.rowHeight,
      child: Center(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 5, vertical: 5),
          child: _ValueWell(
            text: parameter == null
                ? '—'
                : _formatParameterValue(parameter!.value),
            enabled:
                parameter != null &&
                controller.isConnected &&
                !parameter!.readOnly,
            center: true,
            fillColor: toneColor.withValues(alpha: colors.isDark ? 0.28 : 0.12),
            onSubmitted: parameter == null
                ? null
                : (value) => controller.setParameter(parameter!.name, value),
          ),
        ),
      ),
    );
  }
}

class _ParameterWriteStatus extends StatelessWidget {
  const _ParameterWriteStatus({required this.controller});

  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final error = controller.parameterWriteError;
    final message = error ?? '参数写入失败';
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2, vertical: 3),
      child: Row(
        children: [
          MacosIcon(
            CupertinoIcons.exclamationmark_circle,
            size: 14,
            color: colors.danger,
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Text(
              message,
              style: MacosTheme.of(
                context,
              ).typography.caption1.copyWith(color: colors.danger),
            ),
          ),
        ],
      ),
    );
  }
}

class _FlightModesPage extends StatefulWidget {
  const _FlightModesPage({required this.controller});

  final ConnectionController controller;

  @override
  State<_FlightModesPage> createState() => _FlightModesPageState();
}

class _FlightModesPageState extends State<_FlightModesPage> {
  static const _pwmRanges = [
    '≤ 1230',
    '1231–1360',
    '1361–1490',
    '1491–1620',
    '1621–1749',
    '≥ 1750',
  ];

  @override
  void initState() {
    super.initState();
    widget.controller.loadParameterMetadata([
      'FLTMODE_CH',
      for (var position = 1; position <= 6; position++) 'FLTMODE$position',
      for (var channel = 5; channel <= 16; channel++) 'RC${channel}_OPTION',
    ]);
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: Listenable.merge([
      widget.controller.parameterChanges,
      widget.controller.telemetryChanges,
    ]),
    builder: (context, _) {
      final colors = context.mgColors;
      final liveMode =
          widget.controller.vehicleIdentity?['flight_mode'] as String?;
      return SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(_pageMargin, 22, _pageMargin, 32),
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
                        child: _ParameterValueControl(
                          parameter: widget.controller.parameters['FLTMODE_CH'],
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
            GridView.builder(
              shrinkWrap: true,
              physics: const NeverScrollableScrollPhysics(),
              itemCount: 6,
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
                  pwmRange: _pwmRanges[index],
                  parameter: widget.controller.parameters['FLTMODE$position'],
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
                      widget.controller.parameters['RC${channel}_OPTION'],
                  controller: widget.controller,
                  dividerColor: colors.divider,
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
    required this.parameter,
    required this.controller,
  });

  final int position;
  final String pwmRange;
  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: context.mgColors.surfaceMuted,
        border: Border.all(color: context.mgColors.divider),
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
            _ParameterValueControl(
              parameter: parameter,
              controller: controller,
            ),
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
  });

  final int channel;
  final VehicleParameter? parameter;
  final ConnectionController controller;
  final Color dividerColor;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: context.mgColors.surfaceMuted,
      border: Border.all(color: dividerColor),
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
            child: _ParameterValueControl(
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
        MgTypography.bodyStrong.copyWith(color: context.mgColors.accent),
      ),
    ),
  );
}

class _ParameterLoadingState extends StatelessWidget {
  const _ParameterLoadingState({
    required this.scrollController,
    required this.controller,
    required this.title,
  });

  final ScrollController scrollController;
  final ConnectionController controller;
  final String title;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    final error = controller.parameterError;
    return SingleChildScrollView(
      controller: scrollController,
      child: Center(
        child: Padding(
          padding: const EdgeInsets.all(64),
          child: SizedBox(
            width: 420,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(title, style: theme.typography.title2),
                const SizedBox(height: 16),
                if (error != null) ...[
                  Text(
                    error,
                    textAlign: TextAlign.center,
                    style: theme.typography.callout.copyWith(
                      color: colors.danger,
                    ),
                  ),
                  const SizedBox(height: 14),
                  PushButton(
                    controlSize: ControlSize.regular,
                    onPressed: controller.isConnected
                        ? () => controller.requestParameters(force: true)
                        : null,
                    child: const Text('重试读取'),
                  ),
                ] else if (controller.isConnected)
                  ParameterLoadProgress(
                    received: controller.parameterReceived,
                    total: controller.parameterTotal,
                  )
                else
                  Text(
                    '连接飞控后自动读取参数',
                    textAlign: TextAlign.center,
                    style: theme.typography.callout.copyWith(
                      color: colors.textSecondary,
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _ParameterEmptyState extends StatelessWidget {
  const _ParameterEmptyState({
    required this.scrollController,
    required this.message,
  });

  final ScrollController scrollController;
  final String message;

  @override
  Widget build(BuildContext context) => SingleChildScrollView(
    controller: scrollController,
    child: Center(
      child: Padding(
        padding: const EdgeInsets.all(64),
        child: Text(
          message,
          style: MacosTheme.of(
            context,
          ).typography.callout.copyWith(color: context.mgColors.textSecondary),
        ),
      ),
    ),
  );
}

class _ParameterSummaryRow extends StatelessWidget {
  const _ParameterSummaryRow({
    required this.parameter,
    required this.controller,
  });

  final VehicleParameter parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final colors = context.mgColors;
    return Container(
      margin: const EdgeInsets.only(bottom: 6),
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: context.mgColors.surface,
        border: Border.all(color: context.mgColors.divider),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  parameter.displayName ?? parameter.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.typography.body.copyWith(
                    color: _adaptiveLabelColor(context),
                    fontWeight: FontWeight.w600,
                  ),
                ),
                Text(
                  parameter.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 16),
          SizedBox(
            width: 220,
            child: _ParameterValueControl(
              parameter: parameter,
              controller: controller,
            ),
          ),
        ],
      ),
    );
  }
}

class _PortConfigurationPage extends StatefulWidget {
  const _PortConfigurationPage({
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<_PortConfigurationPage> createState() => _PortConfigurationPageState();
}

class _PortConfigurationPageState extends State<_PortConfigurationPage> {
  @override
  void initState() {
    super.initState();
    if (widget.controller.isConnected) {
      unawaited(widget.controller.refreshSerialHardwareMappings());
    }
    widget.controller.loadParameterMetadata([
      for (var port = 0; port <= 15; port++) ...[
        'SERIAL${port}_PROTOCOL',
        'SERIAL${port}_BAUD',
      ],
      for (var port = 1; port <= 4; port++) ...[
        'SER_TEL${port}_PROT',
        'SER_TEL${port}_BAUD',
      ],
      for (var port = 1; port <= 2; port++) ...[
        'SER_GPS${port}_PROT',
        'SER_GPS${port}_BAUD',
      ],
    ]);
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.parameterChanges,
    builder: (context, _) {
      final controller = widget.controller;
      final serialPorts = controller.serialParameterPorts;
      final theme = MacosTheme.of(context);
      if (!controller.parametersLoaded) {
        return _ParameterLoadingState(
          scrollController: widget.scrollController,
          controller: controller,
          title: '串口参数',
        );
      }
      if (serialPorts.isEmpty) {
        return _ParameterEmptyState(
          scrollController: widget.scrollController,
          message: '飞控未返回串口配置参数',
        );
      }
      return SingleChildScrollView(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(_pageMargin, 22, _pageMargin, 32),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (controller.parameterWriteError != null) ...[
              _ParameterWriteStatus(controller: controller),
              const SizedBox(height: 10),
            ],
            if (controller.serialHardwareMappingLoading)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '正在读取飞控硬件串口映射…',
                  style: theme.typography.caption1.copyWith(
                    color: context.mgColors.textSecondary,
                  ),
                ),
              )
            else if (controller.serialHardwareMappingError case final error?)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '飞控未提供硬件串口映射：$error',
                  style: theme.typography.caption1.copyWith(
                    color: context.mgColors.textSecondary,
                  ),
                ),
              ),
            LayoutBuilder(
              builder: (context, constraints) {
                final columns = constraints.maxWidth >= 900 ? 2 : 1;
                final cardWidth =
                    (constraints.maxWidth - (columns - 1) * 14) / columns;
                return Wrap(
                  spacing: 14,
                  runSpacing: 14,
                  children: [
                    for (final port in serialPorts)
                      SizedBox(
                        width: cardWidth,
                        child: _SerialParameterCard(
                          serialPrefix: port,
                          controller: controller,
                          dividerColor: context.mgColors.divider,
                          hardwareName: controller.serialHardwareMappings[port],
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
}

class _SerialParameterCard extends StatefulWidget {
  const _SerialParameterCard({
    required this.serialPrefix,
    required this.controller,
    required this.dividerColor,
    required this.hardwareName,
  });

  final String serialPrefix;
  final ConnectionController controller;
  final Color dividerColor;
  final String? hardwareName;

  @override
  State<_SerialParameterCard> createState() => _SerialParameterCardState();
}

class _SerialParameterCardState extends State<_SerialParameterCard> {
  bool _showOptions = false;

  @override
  Widget build(BuildContext context) {
    final controller = widget.controller;
    final theme = MacosTheme.of(context);
    final serial = widget.serialPrefix;
    final extraParameters = controller.parameters.values
        .where(
          (parameter) =>
              parameter.name.startsWith('${serial}_') &&
              parameter.name != '${serial}_PROTOCOL' &&
              parameter.name != '${serial}_PROT' &&
              parameter.name != '${serial}_BAUD',
        )
        .toList(growable: false);
    final protocol =
        controller.parameters['${serial}_PROTOCOL'] ??
        controller.parameters['${serial}_PROT'];
    final baud = controller.parameters['${serial}_BAUD'];

    return DecoratedBox(
      decoration: BoxDecoration(
        color: context.mgColors.surfaceMuted,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: widget.dividerColor),
        boxShadow: [
          BoxShadow(
            color: context.mgColors.shadow,
            blurRadius: 10,
            offset: const Offset(0, 3),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Text(
                  serial,
                  style: MgTypography.sans(
                    MgTypography.section.copyWith(fontWeight: FontWeight.w700),
                  ),
                ),
                if (widget.hardwareName case final hardwareName?) ...[
                  const SizedBox(width: 10),
                  Flexible(
                    child: Text(
                      hardwareName,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: MgTypography.sans(
                        MgTypography.bodyStrong.copyWith(
                          color: context.mgColors.accent,
                        ),
                      ),
                    ),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 10),
            LayoutBuilder(
              builder: (context, constraints) {
                // 并列控件使用同一套网格：标签列等宽、控件等宽等高，
                // 两列的上边缘与下边缘严格对齐。
                const labelWidth = 52.0;
                const columnGap = 14.0;
                final compact = constraints.maxWidth < 560;
                final fields = [
                  if (protocol != null)
                    AlignedFieldRow(
                      label: '协议',
                      labelWidth: labelWidth,
                      child: _ParameterValueControl(
                        parameter: protocol,
                        controller: controller,
                        // 填满列宽，保证并列下拉框左右边缘严格对齐。
                        popupWidth: double.infinity,
                      ),
                    ),
                  if (baud != null)
                    AlignedFieldRow(
                      label: '波特率',
                      labelWidth: labelWidth,
                      child: _ParameterValueControl(
                        parameter: baud,
                        controller: controller,
                        popupWidth: double.infinity,
                      ),
                    ),
                ];
                if (compact) {
                  return Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      for (var index = 0; index < fields.length; index++) ...[
                        if (index > 0) const SizedBox(height: 12),
                        fields[index],
                      ],
                    ],
                  );
                }
                return Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    for (var index = 0; index < fields.length; index++) ...[
                      if (index > 0) const SizedBox(width: columnGap),
                      Expanded(flex: 1, child: fields[index]),
                    ],
                  ],
                );
              },
            ),
            if (protocol == null && baud == null)
              Text(
                '该逻辑串口没有可读取的协议或波特率参数。',
                style: theme.typography.caption1.copyWith(
                  color: context.mgColors.textSecondary,
                ),
              ),
            if (extraParameters.isNotEmpty) ...[
              const SizedBox(height: 8),
              Divider(height: 1, color: context.mgColors.divider),
              Row(
                children: [
                  MacosDisclosureButton(
                    isPressed: _showOptions,
                    onPressed: () =>
                        setState(() => _showOptions = !_showOptions),
                  ),
                  const SizedBox(width: 4),
                  Flexible(
                    child: Text(
                      '其他串口参数',
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.typography.callout,
                    ),
                  ),
                  const SizedBox(width: 7),
                  Text(
                    '${extraParameters.length}',
                    style: theme.typography.caption1,
                  ),
                ],
              ),
              if (_showOptions)
                for (final parameter in extraParameters)
                  Padding(
                    padding: const EdgeInsets.only(left: 18, top: 4),
                    child: _ParameterSummaryRow(
                      parameter: parameter,
                      controller: controller,
                    ),
                  ),
            ],
          ],
        ),
      ),
    );
  }
}

class _AllParametersPage extends StatefulWidget {
  const _AllParametersPage({
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<_AllParametersPage> createState() => _AllParametersPageState();
}

class _AllParametersPageState extends State<_AllParametersPage> {
  String _query = '';
  final Map<String, bool> _expandedGroups = {};

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.controller.parameterChanges,
    builder: (context, _) {
      final controller = widget.controller;
      if (!controller.parametersLoaded) {
        return _ParameterLoadingState(
          scrollController: widget.scrollController,
          controller: controller,
          title: '飞控参数',
        );
      }

      final query = _query.trim().toLowerCase();
      final grouped = <String, List<VehicleParameter>>{};
      for (final parameter in controller.parameters.values) {
        final terms = [
          parameter.name,
          parameter.displayName ?? '',
          parameter.description ?? '',
          parameter.group ?? '',
        ].join(' ').toLowerCase();
        if (query.isNotEmpty && !terms.contains(query)) continue;
        final group = _parameterGroup(parameter);
        grouped.putIfAbsent(group, () => []).add(parameter);
      }
      for (final values in grouped.values) {
        values.sort((a, b) => a.name.compareTo(b.name));
      }
      final groups = grouped.keys.toList()..sort();
      final entries = <_ParameterTreeEntry>[];
      for (final group in groups) {
        entries.add(_ParameterTreeEntry.group(group, grouped[group]!.length));
        final isExpanded = _expandedGroups[group] ?? query.isNotEmpty;
        if (isExpanded) {
          entries.addAll(grouped[group]!.map(_ParameterTreeEntry.parameter));
        }
      }

      if (controller.parameters.isEmpty) {
        return _ParameterEmptyState(
          scrollController: widget.scrollController,
          message: '飞控没有返回参数',
        );
      }
      if (entries.isEmpty) {
        return _ParameterEmptyState(
          scrollController: widget.scrollController,
          message: '没有找到匹配的参数',
        );
      }

      return ListView.builder(
        controller: widget.scrollController,
        padding: const EdgeInsets.fromLTRB(_pageMargin, 18, _pageMargin, 32),
        itemCount: entries.length + 1,
        itemBuilder: (context, index) {
          if (index == 0) {
            return Padding(
              padding: const EdgeInsets.only(bottom: 16),
              child: MacosSearchField(
                placeholder: '搜索参数名称或说明',
                onChanged: (value) => setState(() {
                  _query = value;
                  _expandedGroups.clear();
                }),
                style: MgTypography.sans(
                  MgTypography.body.copyWith(
                    color: _adaptiveLabelColor(context),
                  ),
                ),
              ),
            );
          }
          final entry = entries[index - 1];
          if (entry.parameter == null) {
            final group = entry.group!;
            final isExpanded = _expandedGroups[group] ?? query.isNotEmpty;
            void toggleGroup() {
              setState(() => _expandedGroups[group] = !isExpanded);
            }

            return Padding(
              padding: const EdgeInsets.only(top: 8, bottom: 2),
              child: Semantics(
                button: true,
                expanded: isExpanded,
                label: '$group，${entry.parameterCount} 个参数',
                child: Material(
                  color: context.mgColors.transparent,
                  child: InkWell(
                    onTap: toggleGroup,
                    borderRadius: BorderRadius.circular(
                      MgMetrics.controlRadius,
                    ),
                    child: SizedBox(
                      height: 34,
                      child: Row(
                        children: [
                          MacosDisclosureButton(
                            isPressed: isExpanded,
                            semanticLabel: isExpanded ? '收起$group' : '展开$group',
                            onPressed: toggleGroup,
                          ),
                          const SizedBox(width: 7),
                          Expanded(
                            child: Text(
                              group,
                              style: MacosTheme.of(context).typography.callout
                                  .copyWith(fontWeight: FontWeight.w600),
                            ),
                          ),
                          Text(
                            '${entry.parameterCount}',
                            style: MacosTheme.of(context).typography.caption1
                                .copyWith(
                                  color: context.mgColors.textSecondary,
                                ),
                          ),
                          const SizedBox(width: 8),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
            );
          }
          final parameter = entry.parameter!;
          return _ParameterSummaryRow(
            parameter: parameter,
            controller: controller,
          );
        },
      );
    },
  );

  String _parameterGroup(VehicleParameter parameter) {
    final descriptorGroup = parameter.group;
    if (descriptorGroup != null &&
        descriptorGroup.isNotEmpty &&
        descriptorGroup != 'Standard' &&
        descriptorGroup != 'Advanced') {
      return descriptorGroup;
    }
    final prefix = parameter.name.split('_').first;
    return switch (prefix) {
      'SERIAL' => '串口配置',
      'SERVO' || 'MOT' => '输出与执行器',
      'RC' || 'FLTMODE' => '遥控与飞行模式',
      'ATC' || 'PSC' || 'INS' => '飞行控制',
      _ => prefix,
    };
  }
}

class _ParameterTreeEntry {
  const _ParameterTreeEntry.group(this.group, this.parameterCount)
    : parameter = null;
  const _ParameterTreeEntry.parameter(VehicleParameter this.parameter)
    : group = null,
      parameterCount = 0;

  final String? group;
  final VehicleParameter? parameter;
  final int parameterCount;
}

class _SystemStatusPage extends StatelessWidget {
  const _SystemStatusPage({required this.controller});

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
        padding: const EdgeInsets.fromLTRB(_pageMargin, 22, _pageMargin, 32),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _SystemOverviewCard(
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
            _SystemSectionTitle(
              title: '传感器与子系统',
              trailing: _HealthSummary(summary: summary),
            ),
            const SizedBox(height: 10),
            if (!connected || health == null)
              _SensorDataPendingCard(
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
                          child: _SensorStatusCard(
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
            _SystemSectionTitle(title: '实时状态'),
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
                      child: _TelemetryStatusCard(
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
                      child: _TelemetryStatusCard(
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
                      child: _TelemetryStatusCard(
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

class _SystemOverviewCard extends StatelessWidget {
  const _SystemOverviewCard({
    required this.connected,
    required this.connectionState,
    required this.firmware,
    required this.vehicle,
    required this.systemId,
    required this.firmwareVersion,
    required this.background,
    required this.borderColor,
  });

  final bool connected;
  final String connectionState;
  final String firmware;
  final String vehicle;
  final int? systemId;
  final String firmwareVersion;
  final Color background;
  final Color borderColor;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(18),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(12),
    ),
    child: LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth >= 850 ? 4 : 2;
        final itemWidth = (constraints.maxWidth - (columns - 1) * 20) / columns;
        return Wrap(
          spacing: 20,
          runSpacing: 16,
          children: [
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '连接状态',
                value: connectionState,
                icon: connected ? CupertinoIcons.link : CupertinoIcons.link,
                valueColor: connected
                    ? context.mgColors.success
                    : context.mgColors.textSecondary,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '固件类型',
                value: firmware,
                icon: CupertinoIcons.antenna_radiowaves_left_right,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '机型',
                value: vehicle,
                icon: CupertinoIcons.airplane,
              ),
            ),
            SizedBox(
              width: itemWidth,
              child: _OverviewValue(
                label: '飞控信息',
                value: [
                  if (systemId != null) '系统 ID $systemId',
                  if (firmwareVersion != '尚未上报') firmwareVersion,
                  if (systemId == null && firmwareVersion == '尚未上报') '等待数据',
                ].join(' · '),
                icon: CupertinoIcons.info_circle,
              ),
            ),
          ],
        );
      },
    ),
  );
}

class _OverviewValue extends StatelessWidget {
  const _OverviewValue({
    required this.label,
    required this.value,
    required this.icon,
    this.valueColor,
  });

  final String label;
  final String value;
  final IconData icon;
  final Color? valueColor;

  @override
  Widget build(BuildContext context) {
    final typography = MacosTheme.of(context).typography;
    final colors = context.mgColors;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(top: 2),
          child: MacosIcon(icon, size: 17, color: colors.accent),
        ),
        const SizedBox(width: 9),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                label,
                style: typography.caption1.copyWith(
                  color: colors.textSecondary,
                ),
              ),
              const SizedBox(height: 3),
              Text(
                value,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: typography.body.copyWith(
                  color: valueColor ?? _adaptiveLabelColor(context),
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _SystemSectionTitle extends StatelessWidget {
  const _SystemSectionTitle({required this.title, this.trailing});

  final String title;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      Text(
        title,
        style: MacosTheme.of(
          context,
        ).typography.title3.copyWith(fontWeight: FontWeight.w600),
      ),
      const Spacer(),
      ?trailing,
    ],
  );
}

class _HealthSummary extends StatelessWidget {
  const _HealthSummary({required this.summary});

  final Map<String, Object?>? summary;

  @override
  Widget build(BuildContext context) {
    if (summary == null) return const SizedBox.shrink();
    final colors = context.mgColors;
    final faults = summary!['fault_count'] as int? ?? 0;
    final healthy = summary!['healthy_count'] as int? ?? 0;
    final label = faults > 0 ? '异常 $faults' : '正常 $healthy';
    final color = faults > 0 ? colors.danger : colors.success;
    return Text(
      label,
      style: MacosTheme.of(
        context,
      ).typography.callout.copyWith(color: color, fontWeight: FontWeight.w600),
    );
  }
}

class _SensorDataPendingCard extends StatelessWidget {
  const _SensorDataPendingCard({
    required this.connected,
    required this.borderColor,
    required this.background,
  });

  final bool connected;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(16),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
    ),
    child: MacosListTile(
      leading: MacosIcon(
        connected ? CupertinoIcons.waveform_path : CupertinoIcons.info_circle,
        size: 18,
      ),
      title: Text(connected ? '等待传感器状态数据' : '尚未连接飞控'),
      subtitle: Text(
        connected
            ? '飞控尚未上报 SYS_STATUS，收到状态后会显示各传感器是否存在、启用及健康。'
            : '连接飞控后显示固件、机型和传感器健康状态。',
      ),
    ),
  );
}

class _SensorStatusCard extends StatelessWidget {
  const _SensorStatusCard({
    required this.sensor,
    required this.icon,
    required this.status,
    required this.borderColor,
    required this.background,
  });

  final String sensor;
  final IconData icon;
  final String status;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final (label, color) = switch (status) {
      'healthy' => ('正常', colors.success),
      'fault' => ('异常', colors.danger),
      'disabled' => ('未启用', colors.warning),
      'missing' => ('未检测到', colors.textSecondary),
      _ => ('未知', colors.textSecondary),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: background,
        border: Border.all(color: borderColor),
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
      ),
      child: Row(
        children: [
          MacosIcon(icon, size: 18),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              sensor,
              style: MacosTheme.of(context).typography.body,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          const SizedBox(width: 8),
          Text(
            label,
            style: MacosTheme.of(context).typography.callout.copyWith(
              color: color,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}

class _TelemetryStatusCard extends StatelessWidget {
  const _TelemetryStatusCard({
    required this.label,
    required this.icon,
    required this.value,
    required this.detail,
    required this.fresh,
    required this.borderColor,
    required this.background,
  });

  final String label;
  final IconData icon;
  final String value;
  final String detail;
  final bool fresh;
  final Color borderColor;
  final Color background;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(14),
    decoration: BoxDecoration(
      color: background,
      border: Border.all(color: borderColor),
      borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
    ),
    child: Row(
      children: [
        MacosIcon(icon, size: 21, color: context.mgColors.accent),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(label, style: MacosTheme.of(context).typography.caption1),
              const SizedBox(height: 3),
              Text(
                value,
                style: MacosTheme.of(context).typography.body.copyWith(
                  color: fresh
                      ? _adaptiveLabelColor(context)
                      : context.mgColors.textSecondary,
                  fontWeight: FontWeight.w600,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                detail,
                style: MacosTheme.of(context).typography.caption1.copyWith(
                  color: context.mgColors.textSecondary,
                ),
              ),
            ],
          ),
        ),
      ],
    ),
  );
}

Color _adaptiveLabelColor(BuildContext context) => context.mgColors.textPrimary;
