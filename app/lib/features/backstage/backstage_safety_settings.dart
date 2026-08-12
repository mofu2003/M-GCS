part of 'backstage_page.dart';

class _SafetySettingsPage extends StatefulWidget {
  const _SafetySettingsPage({required this.controller});

  final ParameterController controller;

  @override
  State<_SafetySettingsPage> createState() => _SafetySettingsPageState();
}

class _SafetySettingsPageState extends State<_SafetySettingsPage>
    with SingleTickerProviderStateMixin {
  late final TabController _tabController;
  int _selectedTab = 0;

  ParameterController get controller => widget.controller;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 3, vsync: this);
    _tabController.addListener(() {
      if (mounted && _selectedTab != _tabController.index) {
        setState(() => _selectedTab = _tabController.index);
      }
    });
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  FlightControllerParameter? _parameter(String name) => controller.parameters
      .where((parameter) => parameter.name == name)
      .firstOrNull;

  Future<bool> _write(BuildContext context, String name, double value) async {
    final result = await controller.writeValueImmediately(name, value);
    if (!result.success && context.mounted) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
    }
    return result.success;
  }

  @override
  Widget build(BuildContext context) {
    final enabled = controller.canEdit;
    return _GlassPanel(
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 10),
            child: Align(
              alignment: Alignment.center,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 480),
                child: SizedBox(
                  width: double.infinity,
                  height: 34,
                  child: cupertino.CupertinoSlidingSegmentedControl<int>(
                    groupValue: _selectedTab,
                    onValueChanged: (tab) {
                      if (tab == null || tab == _selectedTab) return;
                      setState(() => _selectedTab = tab);
                      _tabController.animateTo(tab);
                    },
                    padding: const EdgeInsets.all(2),
                    backgroundColor: MacGlassTokens.ink.withValues(alpha: 0.055),
                    thumbColor: Colors.white.withValues(alpha: 0.94),
                    children: const {
                      0: _SafetyTabLabel(label: '电子围栏'),
                      1: _SafetyTabLabel(label: '解锁检查'),
                      2: _SafetyTabLabel(label: '电池检查'),
                    },
                  ),
                ),
              ),
            ),
          ),
          Divider(
            height: 1,
            thickness: 1,
            color: MacGlassTokens.ink.withValues(alpha: 0.065),
          ),
          Expanded(
            child: TabBarView(
              controller: _tabController,
              children: [
                _CalibrationPane(
                  pageKey: const ValueKey('mg-safety-fence'),
                  children: [
                    const _CalibrationSectionHeader(
                      title: '电子围栏',
                      subtitle: '高度、距离与越界动作；围栏点请在任务地图中保存。',
                    ),
                    const SizedBox(height: 12),
                    _CalibrationSettingsGroup(children: [
              _SensorParameterSwitch(
                parameter: _parameter('FENCE_ENABLE'),
                title: '启用电子围栏',
                subtitle: 'FENCE_ENABLE · Enable fence',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SafetyFenceTypeRow(
                controller: controller,
                parameter: _parameter('FENCE_TYPE'),
                title: '围栏类型',
                subtitle: 'FENCE_TYPE · Fence type',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterPickerRow(
                controller: controller,
                parameter: _parameter('FENCE_ACTION'),
                title: '越界动作',
                subtitle: 'FENCE_ACTION · Breach action',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-fence-alt-max'),
                controller: controller,
                parameter: _parameter('FENCE_ALT_MAX'),
                title: '最大高度',
                subtitle: 'FENCE_ALT_MAX · Maximum altitude',
                unit: 'm',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-fence-alt-min'),
                controller: controller,
                parameter: _parameter('FENCE_ALT_MIN'),
                title: '最小高度',
                subtitle: 'FENCE_ALT_MIN · Minimum altitude',
                unit: 'm',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-fence-radius'),
                controller: controller,
                parameter: _parameter('FENCE_RADIUS'),
                title: '圆形围栏半径',
                subtitle: 'FENCE_RADIUS · Home radius',
                unit: 'm',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-fence-margin'),
                controller: controller,
                parameter: _parameter('FENCE_MARGIN'),
                title: '安全余量',
                subtitle: 'FENCE_MARGIN · Fence margin',
                unit: 'm',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
                    ]),
                  ],
                ),
                _CalibrationPane(
                  pageKey: const ValueKey('mg-safety-arming'),
                  children: [
                    const _CalibrationSectionHeader(
                      title: '解锁检查',
                      subtitle: '飞控在解锁前执行传感器、GPS、遥控器和电源预检查。',
                    ),
                    const SizedBox(height: 12),
                    _ArmingChecksPanel(
                      controller: controller,
                      parameter:
                          _parameter('ARMING_SKIPCHK') ?? _parameter('ARMING_CHECK'),
                      usesSkipMask: _parameter('ARMING_SKIPCHK') != null,
                      enabled: enabled,
                      onWrite: (name, value) => _write(context, name, value),
                    ),
                    const SizedBox(height: 24),
                    const _CalibrationSectionHeader(
                      title: '机载日志',
                      subtitle: '解锁前检查可用性与未解锁记录设置。',
                    ),
                    const SizedBox(height: 12),
                    _CalibrationSettingsGroup(
                      children: [
                        _SensorParameterSwitch(
                          parameter: _parameter('ARMING_LOG'),
                          title: '解锁日志检查',
                          subtitle: 'ARMING_LOG · Arming log check',
                          enabled: enabled,
                          onWrite: (name, value) => _write(context, name, value),
                        ),
                        _SensorParameterPickerRow(
                          controller: controller,
                          parameter: _parameter('LOG_DISARMED'),
                          title: '未解锁时记录',
                          subtitle: 'LOG_DISARMED · Log while disarmed',
                          enabled: enabled,
                          onWrite: (name, value) => _write(context, name, value),
                        ),
                        _SensorParameterSwitch(
                          parameter: _parameter('LOG_FILE_DSRMROT'),
                          title: '解锁后轮换日志文件',
                          subtitle: 'LOG_FILE_DSRMROT · Rotate log on disarm',
                          enabled: enabled,
                          onWrite: (name, value) => _write(context, name, value),
                        ),
                        _SensorParameterSwitch(
                          parameter: _parameter('LOG_REPLAY'),
                          title: 'EKF 回放记录',
                          subtitle: 'LOG_REPLAY · EKF replay data',
                          enabled: enabled,
                          onWrite: (name, value) => _write(context, name, value),
                        ),
                        _SensorParameterValueRow(
                          rowKey: const ValueKey('mg-safety-log-file-bufsize'),
                          controller: controller,
                          parameter: _parameter('LOG_FILE_BUFSIZE'),
                          title: '日志文件缓冲区',
                          subtitle: 'LOG_FILE_BUFSIZE · File buffer size',
                          unit: 'KiB',
                          enabled: enabled,
                          onWrite: (name, value) => _write(context, name, value),
                        ),
                      ],
                    ),
                  ],
                ),
                _CalibrationPane(
                  pageKey: const ValueKey('mg-safety-battery'),
                  children: [
                    const _CalibrationSectionHeader(
                      title: '电池检查',
                      subtitle: '解锁门槛、低电与严重低电保护动作会立即写入飞控。',
                    ),
                    const SizedBox(height: 12),
                    _CalibrationSettingsGroup(children: [
              _SensorParameterPickerRow(
                controller: controller,
                parameter: _parameter('BATT_MONITOR'),
                title: '电池监测器',
                subtitle: 'BATT_MONITOR · Battery monitor',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-battery-arm-volt'),
                controller: controller,
                parameter: _parameter('BATT_ARM_VOLT'),
                title: '解锁所需电压',
                subtitle: 'BATT_ARM_VOLT · Arm voltage',
                unit: 'V',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-battery-arm-mah'),
                controller: controller,
                parameter: _parameter('BATT_ARM_MAH'),
                title: '解锁所需剩余容量',
                subtitle: 'BATT_ARM_MAH · Arm capacity',
                unit: 'mAh',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-battery-low-volt'),
                controller: controller,
                parameter: _parameter('BATT_LOW_VOLT'),
                title: '低电压阈值',
                subtitle: 'BATT_LOW_VOLT · Low voltage',
                unit: 'V',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterValueRow(
                rowKey: const ValueKey('mg-safety-battery-critical-volt'),
                controller: controller,
                parameter: _parameter('BATT_CRT_VOLT'),
                title: '严重低电压阈值',
                subtitle: 'BATT_CRT_VOLT · Critical voltage',
                unit: 'V',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterPickerRow(
                controller: controller,
                parameter: _parameter('BATT_FS_LOW_ACT'),
                title: '低电保护动作',
                subtitle: 'BATT_FS_LOW_ACT · Low failsafe action',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
              _SensorParameterPickerRow(
                controller: controller,
                parameter: _parameter('BATT_FS_CRT_ACT'),
                title: '严重低电保护动作',
                subtitle: 'BATT_FS_CRT_ACT · Critical failsafe action',
                enabled: enabled,
                onWrite: (name, value) => _write(context, name, value),
              ),
                    ]),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _SafetyTabLabel extends StatelessWidget {
  const _SafetyTabLabel({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => Center(
    child: Text(
      label,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: const TextStyle(
        color: MacGlassTokens.ink,
        fontSize: 11.5,
        fontWeight: FontWeight.w600,
      ),
    ),
  );
}

class _ArmingChecksPanel extends StatelessWidget {
  const _ArmingChecksPanel({
    required this.controller,
    required this.parameter,
    required this.usesSkipMask,
    required this.enabled,
    required this.onWrite,
  });

  final ParameterController controller;
  final FlightControllerParameter? parameter;
  final bool usesSkipMask;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final descriptor = parameter == null
        ? null
        : controller.descriptorFor(parameter!.name);
    final checks = _distinctParameterOptions(descriptor)
        .where((option) => usesSkipMask ? option.value > 0 : option.value > 1)
        .toList(growable: false);
    final knownMask = checks.fold(0, (mask, option) => mask | option.value);
    final value = parameter?.value.round() ?? 0;
    final allChecks = usesSkipMask ? value == 0 : value == 1;
    final selectedMask = usesSkipMask
        ? knownMask & ~value
        : allChecks
        ? knownMask
        : value;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.52),
        border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.075)),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                const Icon(Icons.health_and_safety_outlined, color: MacGlassTokens.blue),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    usesSkipMask ? 'ARMING_SKIPCHK' : 'ARMING_CHECK',
                    style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w700),
                  ),
                ),
                cupertino.CupertinoSwitch(
                  value: allChecks,
                  activeTrackColor: MacGlassTokens.blue,
                  onChanged: parameter == null || !enabled
                      ? null
                      : (selected) => unawaited(
                          onWrite(
                            parameter!.name,
                            usesSkipMask
                                ? (selected ? 0 : knownMask).toDouble()
                                : (selected ? 1 : 0).toDouble(),
                          ),
                        ),
                ),
              ],
            ),
            const SizedBox(height: 6),
            Text(
              parameter == null
                  ? '飞控未提供 ARMING_SKIPCHK 或 ARMING_CHECK'
                  : allChecks
                  ? '全部检查已执行。关闭“全部”后可按项目单独选择。'
                  : usesSkipMask
                  ? '跳过检查掩码 $value；开关表示该项是否执行。'
                  : value == 0
                  ? '未选择任何可选检查。'
                  : '自定义检查掩码 $value。',
              style: const TextStyle(color: MacGlassTokens.muted, fontSize: 11.5),
            ),
            const SizedBox(height: 14),
            if (checks.isEmpty)
              const Text(
                '尚未取得该飞控版本的检查项元数据。',
                style: TextStyle(color: MacGlassTokens.muted, fontSize: 11.5),
              )
            else
              _CalibrationSettingsGroup(
                children: [
                  for (final check in checks)
                    _ArmingCheckToggleRow(
                      option: check,
                      selected: selectedMask & check.value != 0,
                      enabled: enabled && parameter != null,
                      onChanged: (selected) {
                        final nextMask = usesSkipMask
                            ? selected
                                ? value & ~check.value
                                : value | check.value
                            : selected
                            ? (allChecks ? knownMask : value) | check.value
                            : (allChecks ? knownMask : value) & ~check.value;
                        unawaited(onWrite(parameter!.name, nextMask.toDouble()));
                      },
                    ),
                ],
              ),
          ],
        ),
      ),
    );
  }
}

class _ArmingCheckToggleRow extends StatelessWidget {
  const _ArmingCheckToggleRow({
    required this.option,
    required this.selected,
    required this.enabled,
    required this.onChanged,
  });

  final ParameterOption option;
  final bool selected;
  final bool enabled;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => _CalibrationSettingsRow(
    icon: Icons.verified_outlined,
    title: option.label,
    subtitle: 'Bit ${option.value.bitLength - 1}',
    trailing: cupertino.CupertinoSwitch(
      value: selected,
      activeTrackColor: MacGlassTokens.blue,
      onChanged: enabled ? onChanged : null,
    ),
  );
}

class _SafetyFenceTypeRow extends StatelessWidget {
  const _SafetyFenceTypeRow({
    required this.controller,
    required this.parameter,
    required this.title,
    required this.subtitle,
    required this.enabled,
    required this.onWrite,
  });

  final ParameterController controller;
  final FlightControllerParameter? parameter;
  final String title;
  final String subtitle;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final descriptor = parameter == null
        ? null
        : controller.descriptorFor(parameter!.name);
    final options = _distinctParameterOptions(descriptor);
    if (parameter == null || options.isEmpty) {
      return _CalibrationSettingsRow(
        icon: Icons.fence_rounded,
        title: title,
        subtitle: '$subtitle · 飞控未提供类型元数据',
        trailing: const Text(
          '--',
          style: TextStyle(color: MacGlassTokens.muted),
        ),
      );
    }
    final mask = parameter!.value.round();
    return _CalibrationSettingsRow(
      icon: Icons.fence_rounded,
      title: title,
      subtitle: subtitle,
      trailing: SizedBox(
        width: 390,
        child: Wrap(
          alignment: WrapAlignment.end,
          spacing: 6,
          runSpacing: 6,
          children: [
            for (final option in options)
              _SafetyFenceTypeToggle(
                label: option.label,
                selected: mask & option.value != 0,
                enabled: enabled,
                onPressed: () {
                  final next = mask & option.value != 0
                      ? mask & ~option.value
                      : mask | option.value;
                  unawaited(onWrite(parameter!.name, next.toDouble()));
                },
              ),
          ],
        ),
      ),
    );
  }
}

class _SafetyFenceTypeToggle extends StatelessWidget {
  const _SafetyFenceTypeToggle({
    required this.label,
    required this.selected,
    required this.enabled,
    required this.onPressed,
  });

  final String label;
  final bool selected;
  final bool enabled;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => FilterChip(
    label: Text(label, maxLines: 1, overflow: TextOverflow.ellipsis),
    selected: selected,
    onSelected: enabled ? (_) => onPressed() : null,
    showCheckmark: false,
    visualDensity: VisualDensity.compact,
    side: BorderSide(
      color: (selected ? MacGlassTokens.blue : MacGlassTokens.ink).withValues(
        alpha: selected ? 0.3 : 0.11,
      ),
    ),
    selectedColor: MacGlassTokens.blue.withValues(alpha: 0.12),
    labelStyle: TextStyle(
      color: selected ? MacGlassTokens.blue : MacGlassTokens.ink,
      fontSize: 11,
      fontWeight: FontWeight.w600,
    ),
  );
}
