part of 'backstage_page.dart';

class _CalibrationPage extends StatefulWidget {
  const _CalibrationPage({required this.controller, required this.active});

  final ParameterController controller;
  final bool active;

  @override
  State<_CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<_CalibrationPage>
    with SingleTickerProviderStateMixin {
  late final TabController _tabController;
  late final PollingTask _poller;
  int _selectedTab = 0;
  CalibrationStatusSnapshot _status = const CalibrationStatusSnapshot();
  final Map<SensorCalibrationType, List<CalibrationResultSnapshot>>
  _calibrationHistory = {};
  CalibrationRetrySnapshot _retryInfo = const CalibrationRetrySnapshot();
  bool _busy = false;
  bool _airspeedCommandSent = false;

  CalibrationSessionBackend? get _backend {
    return widget.controller.capabilities.calibration;
  }

  CalibrationHistorySessionBackend? get _historyBackend {
    return widget.controller.capabilities.calibrationHistory;
  }

  ConnectionSessionBackend? get _connectionBackend {
    return widget.controller.capabilities.connection;
  }

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 4, vsync: this);
    _tabController.addListener(_syncSelectedTab);
    _poller = PollingTask(
      interval: const Duration(milliseconds: 350),
      onTick: _pollStatus,
    );
    if (widget.active) {
      _poller.start();
      unawaited(_loadCalibrationEvidence());
    }
  }

  @override
  void didUpdateWidget(covariant _CalibrationPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.active == widget.active) return;
    if (widget.active) {
      _poller.resume();
      unawaited(_loadCalibrationEvidence());
    } else {
      _poller.pause();
    }
  }

  @override
  void dispose() {
    _poller.dispose();
    _tabController.removeListener(_syncSelectedTab);
    _tabController.dispose();
    super.dispose();
  }

  void _syncSelectedTab() {
    if (!mounted || _selectedTab == _tabController.index) return;
    setState(() => _selectedTab = _tabController.index);
  }

  void _selectTab(int? index) {
    if (index == null || index == _selectedTab) return;
    setState(() => _selectedTab = index);
    _tabController.animateTo(index);
  }

  Future<void> _pollStatus() async {
    final backend = _backend;
    if (backend == null || _busy) return;
    try {
      final previousStage = _status.stage;
      final wasTerminal = _status.isTerminal;
      final status = await backend.loadCalibrationStatus();
      if (!mounted) return;
      setState(() {
        _status = status;
      });
      if (status.isTerminal &&
          (!wasTerminal || status.stage != previousStage)) {
        unawaited(_loadCalibrationEvidence());
      }
    } on Object {
      // A temporary status read failure must not interrupt an active workflow.
    }
  }

  Future<void> _loadCalibrationEvidence() async {
    final backend = _historyBackend;
    if (backend == null) return;
    try {
      final values = await Future.wait([
        backend.loadCalibrationHistory(SensorCalibrationType.accelerometer),
        backend.loadCalibrationHistory(SensorCalibrationType.magnetometer),
        backend.loadCalibrationRetryInfo(),
      ]);
      if (!mounted) return;
      final accelerometer = values[0] as List<CalibrationResultSnapshot>;
      final magnetometer = values[1] as List<CalibrationResultSnapshot>;
      setState(() {
        _calibrationHistory[SensorCalibrationType.accelerometer] = _newestFirst(
          accelerometer,
        );
        _calibrationHistory[SensorCalibrationType.magnetometer] = _newestFirst(
          magnetometer,
        );
        _retryInfo = values[2] as CalibrationRetrySnapshot;
      });
    } on Object {
      // Audit data must not block an otherwise usable calibration workflow.
    }
  }

  List<CalibrationResultSnapshot> _newestFirst(
    List<CalibrationResultSnapshot> values,
  ) {
    final ordered = List<CalibrationResultSnapshot>.of(values)
      ..sort(
        (a, b) => b.timestampMilliseconds.compareTo(a.timestampMilliseconds),
      );
    return List.unmodifiable(ordered);
  }

  Future<void> _start(
    SensorCalibrationType type, {
    AccelerometerCalibrationMode mode = AccelerometerCalibrationMode.sixFace,
  }) async {
    final backend = _backend;
    if (backend == null || _busy || _status.active) return;
    final label = switch (type) {
      SensorCalibrationType.accelerometer =>
        mode == AccelerometerCalibrationMode.sixFace ? '六面校准' : '水平校准',
      SensorCalibrationType.magnetometer => '磁力计校准',
      SensorCalibrationType.airspeed => '空速计校准',
    };
    final confirmed = await cupertino.showCupertinoDialog<bool>(
      context: context,
      builder: (context) => cupertino.CupertinoAlertDialog(
        title: Text('开始$label？'),
        content: const Text('请确认飞行器已解除螺旋桨动力并处于安全、静止的校准环境。'),
        actions: [
          cupertino.CupertinoDialogAction(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('取消'),
          ),
          cupertino.CupertinoDialogAction(
            isDefaultAction: true,
            onPressed: () => Navigator.pop(context, true),
            child: const Text('开始'),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    setState(() => _busy = true);
    final result = await backend.startCalibration(
      type,
      accelerometerMode: mode,
    );
    if (!mounted) return;
    setState(() {
      _busy = false;
      if (type == SensorCalibrationType.airspeed && result.success) {
        _airspeedCommandSent = true;
      }
    });
    if (!result.success) {
      _showError('$label启动失败：${result.message}');
      return;
    }
    await _poller.runOnce();
  }

  Future<void> _confirmOrientation() async {
    final backend = _backend;
    final step = _status.currentStep;
    if (backend == null || _busy || step < 1 || step > 6) return;
    setState(() => _busy = true);
    final result = await backend.confirmAccelerometerOrientation(step);
    if (!mounted) return;
    setState(() => _busy = false);
    if (!result.success) {
      _showError('姿态确认失败：${result.message}');
    }
  }

  Future<void> _cancelCalibration() async {
    final backend = _backend;
    if (backend == null || _busy || !_status.active) return;
    final confirmed = await cupertino.showCupertinoDialog<bool>(
      context: context,
      builder: (context) => cupertino.CupertinoAlertDialog(
        title: const Text('取消当前校准？'),
        content: const Text('本次尚未完成的校准数据不会作为有效结果使用。'),
        actions: [
          cupertino.CupertinoDialogAction(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('继续校准'),
          ),
          cupertino.CupertinoDialogAction(
            isDestructiveAction: true,
            onPressed: () => Navigator.pop(context, true),
            child: const Text('取消校准'),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    setState(() => _busy = true);
    final result = await backend.cancelCalibration();
    if (!mounted) return;
    setState(() => _busy = false);
    if (!result.success) _showError('取消失败：${result.message}');
    await _poller.runOnce();
  }

  void _showError(String message) {
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  Future<bool> _writeParameter(String name, double value) async {
    final result = await widget.controller.writeValueImmediately(name, value);
    if (!result.success && mounted) _showError('$name 写入失败：${result.message}');
    return result.success;
  }

  @override
  Widget build(BuildContext context) {
    final supported = _backend != null;
    final enabled = supported && widget.controller.canEdit && !_busy;
    return _GlassPanel(
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 10),
            child: Align(
              alignment: Alignment.center,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 580),
                child: SizedBox(
                  width: double.infinity,
                  height: 34,
                  child: cupertino.CupertinoSlidingSegmentedControl<int>(
                    groupValue: _selectedTab,
                    onValueChanged: _selectTab,
                    padding: const EdgeInsets.all(2),
                    backgroundColor: MacGlassTokens.ink.withValues(
                      alpha: 0.055,
                    ),
                    thumbColor: Colors.white.withValues(alpha: 0.94),
                    children: const {
                      0: _CalibrationSegmentLabel(
                        key: ValueKey('mg-calibration-tab-accelerometer'),
                        label: 'IMU',
                      ),
                      1: _CalibrationSegmentLabel(
                        key: ValueKey('mg-calibration-tab-magnetometer'),
                        label: '罗盘',
                      ),
                      2: _CalibrationSegmentLabel(
                        key: ValueKey('mg-calibration-tab-gps'),
                        label: 'GPS',
                      ),
                      3: _CalibrationSegmentLabel(
                        key: ValueKey('mg-calibration-tab-airspeed'),
                        label: '空速计',
                      ),
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
          if (!supported)
            const Expanded(
              child: Center(
                child: Text(
                  '当前连接后端不支持传感器校准',
                  style: TextStyle(color: MacGlassTokens.muted),
                ),
              ),
            )
          else
            Expanded(
              child: TabBarView(
                controller: _tabController,
                children: [
                  _AccelerometerCalibrationView(
                    status: _status,
                    sixFaceResult:
                        _calibrationHistory[SensorCalibrationType.accelerometer]
                            ?.where(
                              (result) =>
                                  result.accelerometerMode.isEmpty ||
                                  result.accelerometerMode == 'six_face',
                            )
                            .firstOrNull,
                    levelResult:
                        _calibrationHistory[SensorCalibrationType.accelerometer]
                            ?.where(
                              (result) => result.accelerometerMode == 'level',
                            )
                            .firstOrNull,
                    retryInfo: _retryInfo,
                    enabled: enabled,
                    busy: _busy,
                    onStartSixFace: () =>
                        _start(SensorCalibrationType.accelerometer),
                    onStartLevel: () => _start(
                      SensorCalibrationType.accelerometer,
                      mode: AccelerometerCalibrationMode.level,
                    ),
                    onConfirmOrientation: _confirmOrientation,
                    onCancel: _cancelCalibration,
                  ),
                  _MagnetometerCalibrationView(
                    controller: widget.controller,
                    status: _status,
                    latestResult:
                        _calibrationHistory[SensorCalibrationType.magnetometer]
                            ?.firstOrNull,
                    retryInfo: _retryInfo,
                    enabled: enabled,
                    busy: _busy,
                    onWriteParameter: _writeParameter,
                    onStart: () => _start(SensorCalibrationType.magnetometer),
                    onCancel: _cancelCalibration,
                  ),
                  _GpsConfigurationView(
                    controller: widget.controller,
                    connectionBackend: _connectionBackend,
                    enabled: enabled,
                    onWriteParameter: _writeParameter,
                  ),
                  _AirspeedCalibrationView(
                    controller: widget.controller,
                    enabled: enabled,
                    commandSent: _airspeedCommandSent,
                    onStart: () => _start(SensorCalibrationType.airspeed),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }
}

class _CalibrationSegmentLabel extends StatelessWidget {
  const _CalibrationSegmentLabel({required this.label, super.key});

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

class _CalibrationPane extends StatelessWidget {
  const _CalibrationPane({required this.pageKey, required this.children});

  final Key pageKey;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final inset = constraints.maxWidth < 600 ? 16.0 : 28.0;
      return ListView(
        key: pageKey,
        padding: EdgeInsets.fromLTRB(inset, 22, inset, 28),
        children: [
          SizedBox(
            width: double.infinity,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: children,
            ),
          ),
        ],
      );
    },
  );
}

class _AccelerometerCalibrationView extends StatelessWidget {
  const _AccelerometerCalibrationView({
    required this.status,
    required this.sixFaceResult,
    required this.levelResult,
    required this.retryInfo,
    required this.enabled,
    required this.busy,
    required this.onStartSixFace,
    required this.onStartLevel,
    required this.onConfirmOrientation,
    required this.onCancel,
  });

  final CalibrationStatusSnapshot status;
  final CalibrationResultSnapshot? sixFaceResult;
  final CalibrationResultSnapshot? levelResult;
  final CalibrationRetrySnapshot retryInfo;
  final bool enabled;
  final bool busy;
  final VoidCallback onStartSixFace;
  final VoidCallback onStartLevel;
  final VoidCallback onConfirmOrientation;
  final VoidCallback onCancel;

  bool get _active => status.active && status.type == 'accelerometer';
  bool get _activeSixFace => _active && status.accelerometerMode == 'six_face';
  bool get _activeLevel => _active && status.accelerometerMode == 'level';

  @override
  Widget build(BuildContext context) => _CalibrationPane(
    pageKey: const ValueKey('mg-calibration-accelerometer'),
    children: [
      _CalibrationSectionHeader(
        title: '六面校准',
        subtitle: _activeSixFace
            ? _calibrationStatusLabel(status)
            : '水平、左侧、右侧、机头向下、机头向上、背面',
      ),
      const SizedBox(height: 14),
      _CalibrationResultCard(
        title: '最近一次六面校准',
        result: sixFaceResult,
        retryInfo: retryInfo,
        showScale: true,
      ),
      const SizedBox(height: 14),
      _AccelOrientationTable(
        status: status,
        active: _activeSixFace,
        completed:
            status.type == 'accelerometer' &&
            status.accelerometerMode == 'six_face' &&
            status.stage == 'success',
      ),
      const SizedBox(height: 14),
      if (_activeSixFace) ...[
        _CalibrationProgress(status: status),
        const SizedBox(height: 12),
        _CalibrationActionBar(
          children: [
            _CalibrationButton(
              key: const ValueKey('mg-calibration-accel-confirm'),
              primary: true,
              icon: Icons.check_rounded,
              label: '当前姿态已放好',
              onPressed:
                  enabled &&
                      status.stage == 'waiting_orientation' &&
                      status.currentStep > 0
                  ? onConfirmOrientation
                  : null,
            ),
            _CalibrationButton(
              icon: Icons.close_rounded,
              label: '取消',
              destructive: true,
              onPressed: enabled ? onCancel : null,
            ),
          ],
        ),
      ] else if (!_active)
        _CalibrationActionBar(
          children: [
            _CalibrationButton(
              key: const ValueKey('mg-calibration-accel-six-face'),
              primary: true,
              icon: Icons.play_arrow_rounded,
              label: '开始六面校准',
              onPressed: enabled && !busy && !status.active
                  ? onStartSixFace
                  : null,
            ),
          ],
        ),
      const SizedBox(height: 28),
      const _CalibrationHairline(),
      const SizedBox(height: 24),
      const _CalibrationSectionHeader(
        title: '水平校准',
        subtitle: '校正飞控安装产生的水平姿态偏差',
      ),
      const SizedBox(height: 12),
      _CalibrationResultCard(
        title: '最近一次水平校准',
        result: levelResult,
        retryInfo: retryInfo,
        showScale: true,
      ),
      const SizedBox(height: 12),
      _CalibrationSettingsGroup(
        children: [
          _CalibrationSettingsRow(
            icon: Icons.align_horizontal_center_rounded,
            title: '水平姿态',
            subtitle: '将飞行器放置在平整、静止的表面上',
            trailing: _CalibrationButton(
              key: const ValueKey('mg-calibration-accel-level'),
              primary: true,
              icon: Icons.tune_rounded,
              label: '校准水平姿态',
              onPressed: enabled && !_active && !busy && !status.active
                  ? onStartLevel
                  : null,
            ),
          ),
        ],
      ),
      if (_activeLevel) ...[
        const SizedBox(height: 14),
        _CalibrationProgress(status: status),
        const SizedBox(height: 12),
        _CalibrationActionBar(
          children: [
            _CalibrationButton(
              icon: Icons.close_rounded,
              label: '取消',
              destructive: true,
              onPressed: enabled ? onCancel : null,
            ),
          ],
        ),
      ],
    ],
  );
}

class _AccelOrientationTable extends StatelessWidget {
  const _AccelOrientationTable({
    required this.status,
    required this.active,
    required this.completed,
  });

  final CalibrationStatusSnapshot status;
  final bool active;
  final bool completed;

  static const _orientations = [
    ('水平', Icons.flight_rounded),
    ('左侧', Icons.rotate_90_degrees_ccw_rounded),
    ('右侧', Icons.rotate_90_degrees_cw_rounded),
    ('机头向下', Icons.south_rounded),
    ('机头向上', Icons.north_rounded),
    ('背面', Icons.flip_rounded),
  ];

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth < 520 ? 2 : 3;
      final line = BorderSide(
        color: MacGlassTokens.ink.withValues(alpha: 0.075),
      );
      final complete = completed;
      return ClipRRect(
        borderRadius: BorderRadius.circular(8),
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.24),
            border: Border.all(color: line.color),
            borderRadius: BorderRadius.circular(8),
          ),
          child: GridView.builder(
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: columns,
              mainAxisExtent: 58,
            ),
            itemCount: _orientations.length,
            itemBuilder: (context, index) {
              final step = index + 1;
              final done = complete || (active && status.currentStep > step);
              final current = active && status.currentStep == step;
              final right = (index + 1) % columns != 0;
              final bottom = index < _orientations.length - columns;
              final stateColor = current
                  ? MacGlassTokens.blue
                  : done
                  ? MacGlassTokens.green
                  : MacGlassTokens.muted;
              return Container(
                key: ValueKey('mg-calibration-accel-step-$step'),
                padding: const EdgeInsets.symmetric(horizontal: 12),
                decoration: BoxDecoration(
                  color: current
                      ? MacGlassTokens.blue.withValues(alpha: 0.1)
                      : done
                      ? MacGlassTokens.green.withValues(alpha: 0.035)
                      : Colors.transparent,
                  border: Border(
                    right: right ? line : BorderSide.none,
                    bottom: bottom ? line : BorderSide.none,
                  ),
                ),
                child: Row(
                  children: [
                    Icon(
                      done ? Icons.check_rounded : _orientations[index].$2,
                      size: 18,
                      color: stateColor,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        _orientations[index].$1,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          color: current
                              ? MacGlassTokens.blue
                              : MacGlassTokens.ink,
                          fontSize: 12.5,
                          fontWeight: current
                              ? FontWeight.w700
                              : FontWeight.w600,
                        ),
                      ),
                    ),
                    Text(
                      done
                          ? '完成'
                          : current
                          ? '当前'
                          : '$step',
                      style: TextStyle(
                        color: stateColor,
                        fontFamily: 'monospace',
                        fontSize: 10.5,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                ),
              );
            },
          ),
        ),
      );
    },
  );
}

class _MagnetometerCalibrationView extends StatelessWidget {
  const _MagnetometerCalibrationView({
    required this.controller,
    required this.status,
    required this.latestResult,
    required this.retryInfo,
    required this.enabled,
    required this.busy,
    required this.onWriteParameter,
    required this.onStart,
    required this.onCancel,
  });

  final ParameterController controller;
  final CalibrationStatusSnapshot status;
  final CalibrationResultSnapshot? latestResult;
  final CalibrationRetrySnapshot retryInfo;
  final bool enabled;
  final bool busy;
  final Future<bool> Function(String name, double value) onWriteParameter;
  final VoidCallback onStart;
  final VoidCallback onCancel;

  @override
  Widget build(BuildContext context) {
    final active = status.active && status.type == 'magnetometer';
    final statusColor = _calibrationStageColor(status);
    final autoRotation = _sensorParameter(controller, 'COMPASS_AUTO_ROT');
    final autoDeclination = _sensorParameter(controller, 'COMPASS_AUTODEC');
    final fieldTolerance = _sensorParameter(controller, 'COMPASS_FLTR_RNG');
    final fitTolerance = _sensorParameter(controller, 'COMPASS_CAL_FIT');
    return _CalibrationPane(
      pageKey: const ValueKey('mg-calibration-magnetometer'),
      children: [
        const _CalibrationSectionHeader(
          title: '旋转校准',
          subtitle: '采集飞行器绕三个轴旋转时的磁场样本',
        ),
        const SizedBox(height: 16),
        _CalibrationSettingsGroup(
          children: [
            _CalibrationSettingsRow(
              icon: Icons.explore_rounded,
              iconColor: statusColor,
              title: '校准状态',
              subtitle: active ? _calibrationStatusLabel(status) : '尚未开始磁力计校准',
              trailing: Text(
                active ? '进行中' : '待校准',
                style: TextStyle(
                  color: statusColor,
                  fontFamily: 'monospace',
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            const _CalibrationSettingsRow(
              icon: Icons.threesixty_rounded,
              title: '操作要求',
              subtitle: '按照提示缓慢旋转飞行器，保持远离磁性物体',
            ),
          ],
        ),
        if (active) ...[
          const SizedBox(height: 16),
          _CalibrationProgress(status: status),
        ],
        const SizedBox(height: 22),
        _CalibrationResultCard(
          title: '最近一次罗盘校准结果',
          result: latestResult,
          retryInfo: retryInfo,
          showFitness: true,
        ),
        const SizedBox(height: 22),
        const _CalibrationSectionHeader(
          title: '校准限制',
          subtitle: '这些设置会立即写入飞控，用于安装检测和磁场质量检查。',
        ),
        const SizedBox(height: 12),
        LayoutBuilder(
          builder: (context, constraints) {
            final tiles = [
              _CompassToggleTile(
                parameter: autoRotation,
                title: '自动检测安装方向',
                subtitle: 'COMPASS_AUTO_ROT',
                enabled: enabled,
                onWrite: onWriteParameter,
              ),
              _CompassToggleTile(
                parameter: autoDeclination,
                title: '自动磁偏角',
                subtitle: 'COMPASS_AUTODEC',
                enabled: enabled,
                onWrite: onWriteParameter,
              ),
            ];
            if (constraints.maxWidth < 720) {
              return Column(
                children: [tiles.first, const SizedBox(height: 10), tiles.last],
              );
            }
            return Row(
              children: [
                Expanded(child: tiles.first),
                const SizedBox(width: 10),
                Expanded(child: tiles.last),
              ],
            );
          },
        ),
        const SizedBox(height: 10),
        _CalibrationSettingsGroup(
          children: [
            _SensorParameterValueRow(
              rowKey: const ValueKey('mg-calibration-compass-field-tolerance'),
              controller: controller,
              parameter: fieldTolerance,
              title: '磁场强度容差',
              subtitle: 'COMPASS_FLTR_RNG · 0 表示关闭强度检查',
              unit: '%',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
            _SensorParameterValueRow(
              rowKey: const ValueKey('mg-calibration-compass-fit-tolerance'),
              controller: controller,
              parameter: fitTolerance,
              title: '校准容差等级',
              subtitle: 'COMPASS_CAL_FIT · 数值越低要求越严格',
              unit: '%',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
          ],
        ),
        const SizedBox(height: 18),
        _CalibrationActionBar(
          children: [
            if (!active)
              _CalibrationButton(
                key: const ValueKey('mg-calibration-mag-start'),
                primary: true,
                icon: Icons.play_arrow_rounded,
                label: '开始磁力计校准',
                onPressed: enabled && !busy && !status.active ? onStart : null,
              )
            else
              _CalibrationButton(
                icon: Icons.close_rounded,
                label: '取消校准',
                destructive: true,
                onPressed: enabled ? onCancel : null,
              ),
          ],
        ),
      ],
    );
  }
}

class _GpsConfigurationView extends StatelessWidget {
  const _GpsConfigurationView({
    required this.controller,
    required this.connectionBackend,
    required this.enabled,
    required this.onWriteParameter,
  });

  final ParameterController controller;
  final ConnectionSessionBackend? connectionBackend;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWriteParameter;

  @override
  Widget build(BuildContext context) {
    final primaryType =
        _sensorParameter(controller, 'GPS_TYPE') ??
        _sensorParameter(controller, 'GPS1_TYPE');
    final secondaryType =
        _sensorParameter(controller, 'GPS_TYPE2') ??
        _sensorParameter(controller, 'GPS2_TYPE');
    final autoConfig = _sensorParameter(controller, 'GPS_AUTO_CONFIG');
    final autoSwitch = _sensorParameter(controller, 'GPS_AUTO_SWITCH');
    final sbas = _sensorParameter(controller, 'GPS_SBAS_MODE');
    final navFilter = _sensorParameter(controller, 'GPS_NAVFILTER');
    final detected = primaryType != null || secondaryType != null;
    return _CalibrationPane(
      pageKey: const ValueKey('mg-calibration-gps'),
      children: [
        _SensorStatusSummary(
          icon: Icons.satellite_alt_rounded,
          title: 'GPS 状态',
          subtitle: detected ? '已从飞控参数检测到 GPS 接收机配置' : '当前参数集中未检测到 GPS 接收机',
          status: detected ? '已检测' : '等待',
          tone: detected ? MacGlassTokens.blue : MacGlassTokens.muted,
        ),
        const SizedBox(height: 22),
        const _CalibrationSectionHeader(
          title: '接收机配置',
          subtitle: '配置主、副 GPS 的接收机类型与自动切换策略。修改后立即写入飞控。',
        ),
        const SizedBox(height: 12),
        _CalibrationSettingsGroup(
          children: [
            _SensorParameterPickerRow(
              controller: controller,
              parameter: primaryType,
              title: '主 GPS 接收机',
              subtitle: primaryType?.name ?? 'GPS_TYPE / GPS1_TYPE',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
            _SensorParameterPickerRow(
              controller: controller,
              parameter: secondaryType,
              title: '副 GPS 接收机',
              subtitle: secondaryType?.name ?? 'GPS_TYPE2 / GPS2_TYPE',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
            _SensorParameterSwitch(
              parameter: autoConfig,
              title: '自动配置 GPS',
              subtitle: 'GPS_AUTO_CONFIG',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
            _SensorParameterSwitch(
              parameter: autoSwitch,
              title: '自动切换接收机',
              subtitle: 'GPS_AUTO_SWITCH',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
          ],
        ),
        const SizedBox(height: 22),
        const _CalibrationSectionHeader(
          title: '导航限制',
          subtitle: '卫星增强与导航动态模型会影响定位稳定性和首次定位时间。',
        ),
        const SizedBox(height: 12),
        _CalibrationSettingsGroup(
          children: [
            _SensorParameterPickerRow(
              controller: controller,
              parameter: sbas,
              title: 'SBAS 卫星增强',
              subtitle: sbas?.name ?? 'GPS_SBAS_MODE',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
            _SensorParameterPickerRow(
              controller: controller,
              parameter: navFilter,
              title: '导航动态模型',
              subtitle: navFilter?.name ?? 'GPS_NAVFILTER',
              enabled: enabled,
              onWrite: onWriteParameter,
            ),
          ],
        ),
      ],
    );
  }
}

class _SensorStatusSummary extends StatelessWidget {
  const _SensorStatusSummary({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.status,
    required this.tone,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final String status;
  final Color tone;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.52),
      border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.075)),
      borderRadius: BorderRadius.circular(8),
    ),
    child: Padding(
      padding: const EdgeInsets.all(16),
      child: Row(
        children: [
          DecoratedBox(
            decoration: BoxDecoration(
              color: tone.withValues(alpha: 0.12),
              borderRadius: BorderRadius.circular(8),
            ),
            child: SizedBox.square(
              dimension: 38,
              child: Icon(icon, color: tone, size: 20),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 14,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  subtitle,
                  style: const TextStyle(
                    fontSize: 11.5,
                    color: MacGlassTokens.muted,
                  ),
                ),
              ],
            ),
          ),
          Text(
            status,
            style: TextStyle(
              color: tone,
              fontSize: 12,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    ),
  );
}

class _CompassToggleTile extends StatelessWidget {
  const _CompassToggleTile({
    required this.parameter,
    required this.title,
    required this.subtitle,
    required this.enabled,
    required this.onWrite,
  });

  final FlightControllerParameter? parameter;
  final String title;
  final String subtitle;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.64),
      border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.075)),
      borderRadius: BorderRadius.circular(8),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 12.5,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  parameter == null ? '$subtitle · 未提供' : subtitle,
                  style: const TextStyle(
                    fontSize: 10.5,
                    color: MacGlassTokens.muted,
                  ),
                ),
              ],
            ),
          ),
          cupertino.CupertinoSwitch(
            value: parameter?.value != 0,
            activeTrackColor: MacGlassTokens.blue,
            onChanged: parameter == null || !enabled
                ? null
                : (value) => unawaited(onWrite(parameter!.name, value ? 1 : 0)),
          ),
        ],
      ),
    ),
  );
}

class _SensorParameterSwitch extends StatelessWidget {
  const _SensorParameterSwitch({
    required this.parameter,
    required this.title,
    required this.subtitle,
    required this.enabled,
    required this.onWrite,
  });

  final FlightControllerParameter? parameter;
  final String title;
  final String subtitle;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => _CalibrationSettingsRow(
    icon: Icons.auto_awesome_rounded,
    title: title,
    subtitle: parameter == null ? '$subtitle · 飞控未提供' : subtitle,
    trailing: cupertino.CupertinoSwitch(
      value: parameter?.value != 0,
      activeTrackColor: MacGlassTokens.blue,
      onChanged: parameter == null || !enabled
          ? null
          : (value) => unawaited(onWrite(parameter!.name, value ? 1 : 0)),
    ),
  );
}

class _SensorParameterValueRow extends StatelessWidget {
  const _SensorParameterValueRow({
    required this.rowKey,
    required this.controller,
    required this.parameter,
    required this.title,
    required this.subtitle,
    required this.unit,
    required this.enabled,
    required this.onWrite,
  });

  final Key rowKey;
  final ParameterController controller;
  final FlightControllerParameter? parameter;
  final String title;
  final String subtitle;
  final String unit;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final descriptor = parameter == null
        ? null
        : controller.descriptorFor(parameter!.name);
    final increment = descriptor?.increment ?? 1;
    final min = descriptor?.minValue ?? double.negativeInfinity;
    final max = descriptor?.maxValue ?? double.infinity;
    return _CalibrationSettingsRow(
      key: rowKey,
      icon: Icons.rule_rounded,
      title: title,
      subtitle: parameter == null ? '$subtitle · 飞控未提供' : subtitle,
      trailing: parameter == null
          ? const Text('--', style: TextStyle(color: MacGlassTokens.muted))
          : Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                _SensorStepperButton(
                  icon: Icons.remove_rounded,
                  enabled: enabled && parameter!.value - increment >= min,
                  onPressed: () => unawaited(
                    onWrite(parameter!.name, parameter!.value - increment),
                  ),
                ),
                SizedBox(
                  width: 76,
                  child: Text(
                    '${_formatParameterNumber(parameter!.value)} $unit',
                    textAlign: TextAlign.center,
                    style: const TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
                _SensorStepperButton(
                  icon: Icons.add_rounded,
                  enabled: enabled && parameter!.value + increment <= max,
                  onPressed: () => unawaited(
                    onWrite(parameter!.name, parameter!.value + increment),
                  ),
                ),
              ],
            ),
    );
  }
}

class _SensorStepperButton extends StatelessWidget {
  const _SensorStepperButton({
    required this.icon,
    required this.enabled,
    required this.onPressed,
  });

  final IconData icon;
  final bool enabled;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => IconButton(
    tooltip: icon == Icons.add_rounded ? '增加' : '减少',
    onPressed: enabled ? onPressed : null,
    visualDensity: VisualDensity.compact,
    iconSize: 16,
    style: IconButton.styleFrom(
      minimumSize: const Size(28, 28),
      backgroundColor: MacGlassTokens.ink.withValues(alpha: 0.055),
      foregroundColor: MacGlassTokens.ink,
      disabledForegroundColor: MacGlassTokens.muted,
    ),
    icon: Icon(icon),
  );
}

class _SensorParameterPickerRow extends StatelessWidget {
  const _SensorParameterPickerRow({
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
    return _CalibrationSettingsRow(
      icon: Icons.settings_input_antenna_rounded,
      title: title,
      subtitle: parameter == null ? '$subtitle · 飞控未提供' : subtitle,
      trailing: parameter == null || options.isEmpty
          ? const Text('--', style: TextStyle(color: MacGlassTokens.muted))
          : SizedBox(
              width: 238,
              child: _MacParameterPicker(
                value: parameter!.value.round(),
                options: options,
                enabled: enabled,
                compact: true,
                inset: true,
                showValue: false,
                label: '',
                onChanged: (value) =>
                    unawaited(onWrite(parameter!.name, value.toDouble())),
              ),
            ),
    );
  }
}

class _AirspeedCalibrationView extends StatelessWidget {
  const _AirspeedCalibrationView({
    required this.controller,
    required this.enabled,
    required this.commandSent,
    required this.onStart,
  });

  final ParameterController controller;
  final bool enabled;
  final bool commandSent;
  final VoidCallback onStart;

  @override
  Widget build(BuildContext context) {
    final offset = controller.parameters
        .where((parameter) => parameter.name == 'ARSPD_OFFSET')
        .firstOrNull;
    final sensorAvailable = controller.parameters.any(
      (parameter) =>
          parameter.name == 'ARSPD_TYPE' || parameter.name == 'ARSPD_ENABLE',
    );
    final statusColor = commandSent
        ? MacGlassTokens.green
        : sensorAvailable
        ? MacGlassTokens.blue
        : MacGlassTokens.muted;
    return _CalibrationPane(
      pageKey: const ValueKey('mg-calibration-airspeed'),
      children: [
        const _CalibrationSectionHeader(title: '零点校准', subtitle: '校准皮托管静压差零点'),
        const SizedBox(height: 16),
        _CalibrationSettingsGroup(
          children: [
            _CalibrationSettingsRow(
              icon: Icons.air_rounded,
              iconColor: statusColor,
              title: '传感器状态',
              subtitle: sensorAvailable ? '已从参数树检测到空速计' : '当前参数集中未检测到空速计',
              trailing: Text(
                sensorAvailable ? '已检测' : '未检测',
                style: TextStyle(
                  color: statusColor,
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            _CalibrationSettingsRow(
              icon: Icons.speed_rounded,
              title: offset == null
                  ? '偏移量 --'
                  : '当前偏移量 ${_formatParameterNumber(offset.value)}',
              subtitle: 'ARSPD_OFFSET',
              trailing: const Text(
                '当前值',
                style: TextStyle(color: MacGlassTokens.muted, fontSize: 11.5),
              ),
            ),
            _CalibrationSettingsRow(
              icon: commandSent
                  ? Icons.check_rounded
                  : Icons.info_outline_rounded,
              iconColor: commandSent
                  ? MacGlassTokens.green
                  : MacGlassTokens.muted,
              title: '操作要求',
              subtitle: commandSent ? '校准命令已发送' : '保持皮托管开口无遮挡且周围空气静止',
              trailing: commandSent
                  ? const Text(
                      '已发送',
                      style: TextStyle(
                        color: MacGlassTokens.green,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                      ),
                    )
                  : null,
            ),
          ],
        ),
        const SizedBox(height: 18),
        _CalibrationActionBar(
          children: [
            _CalibrationButton(
              key: const ValueKey('mg-calibration-airspeed-start'),
              primary: true,
              icon: Icons.tune_rounded,
              label: '校准空速计',
              onPressed: enabled && sensorAvailable && !commandSent
                  ? onStart
                  : null,
            ),
          ],
        ),
      ],
    );
  }
}

class _CalibrationSectionHeader extends StatelessWidget {
  const _CalibrationSectionHeader({
    required this.title,
    required this.subtitle,
  });

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) =>
      Text(title, style: MacGlassTypography.sectionTitle);
}

class _CalibrationResultCard extends StatelessWidget {
  const _CalibrationResultCard({
    required this.title,
    required this.result,
    required this.retryInfo,
    this.showScale = false,
    this.showFitness = false,
  });

  final String title;
  final CalibrationResultSnapshot? result;
  final CalibrationRetrySnapshot retryInfo;
  final bool showScale;
  final bool showFitness;

  @override
  Widget build(BuildContext context) {
    final value = result;
    final retryFailure = retryInfo.lastFailure.isNotEmpty
        ? retryInfo.lastFailure
        : value?.failureReason ?? '';
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.25),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.075)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(
                  Icons.fact_check_outlined,
                  size: 17,
                  color: MacGlassTokens.blue,
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    title,
                    style: const TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
                if (value != null)
                  _CalibrationQualityBadge(quality: value.quality),
              ],
            ),
            const SizedBox(height: 9),
            if (value == null)
              const Text(
                '暂无可用的已完成校准记录',
                style: TextStyle(color: MacGlassTokens.muted, fontSize: 12),
              )
            else ...[
              Text(
                '${_calibrationResultStageLabel(value)}${_formatCalibrationTimestamp(value.timestampMilliseconds)}',
                style: TextStyle(
                  color: value.succeeded
                      ? MacGlassTokens.green
                      : MacGlassTokens.red,
                  fontSize: 11.5,
                  fontWeight: FontWeight.w600,
                ),
              ),
              const SizedBox(height: 10),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  _CalibrationMetric(
                    label: '偏移 Offset',
                    value: _formatCalibrationVector(value.offsets),
                  ),
                  if (showScale && value.scale != null)
                    _CalibrationMetric(
                      label: '缩放 Scale',
                      value: _formatCalibrationVector(value.scale!),
                    ),
                  if (showFitness && value.fitness != null)
                    _CalibrationMetric(
                      label: '拟合 Fitness',
                      value: _formatSensorNumber(value.fitness!),
                    ),
                ],
              ),
            ],
            if (retryInfo.retryCount > 0 || retryFailure.isNotEmpty) ...[
              const SizedBox(height: 10),
              const _CalibrationHairline(),
              const SizedBox(height: 8),
              Text(
                retryFailure.isEmpty
                    ? '自动重试 ${retryInfo.retryCount}/${retryInfo.maxRetries}'
                    : '重试 ${retryInfo.retryCount}/${retryInfo.maxRetries}：$retryFailure',
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  color: MacGlassTokens.red,
                  fontSize: 11.5,
                  height: 1.3,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _CalibrationQualityBadge extends StatelessWidget {
  const _CalibrationQualityBadge({required this.quality});

  final String quality;

  @override
  Widget build(BuildContext context) {
    final color = switch (quality) {
      'excellent' => MacGlassTokens.green,
      'good' => MacGlassTokens.blue,
      'marginal' => MacGlassTokens.amber,
      _ => MacGlassTokens.red,
    };
    return DecoratedBox(
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(5),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 4),
        child: Text(
          _calibrationQualityLabel(quality),
          style: TextStyle(
            color: color,
            fontSize: 10.5,
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
    );
  }
}

class _CalibrationMetric extends StatelessWidget {
  const _CalibrationMetric({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) => Semantics(
    label: '$label $value',
    child: DecoratedBox(
      decoration: BoxDecoration(
        color: MacGlassTokens.ink.withValues(alpha: 0.045),
        borderRadius: BorderRadius.circular(5),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
        child: RichText(
          text: TextSpan(
            style: const TextStyle(
              color: MacGlassTokens.muted,
              fontSize: 10.5,
              fontFamily: 'monospace',
            ),
            children: [
              TextSpan(text: '$label  '),
              TextSpan(
                text: value,
                style: const TextStyle(
                  color: MacGlassTokens.ink,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
        ),
      ),
    ),
  );
}

class _CalibrationSettingsGroup extends StatelessWidget {
  const _CalibrationSettingsGroup({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) => _BackstageFeatureCard(
    child: Column(
      children: [
        for (var index = 0; index < children.length; index++) ...[
          if (index > 0) const _CalibrationHairline(indent: 52),
          children[index],
        ],
      ],
    ),
  );
}

class _CalibrationSettingsRow extends StatelessWidget {
  const _CalibrationSettingsRow({
    required this.icon,
    required this.title,
    required this.subtitle,
    this.trailing,
    this.iconColor,
    super.key,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final Widget? trailing;
  final Color? iconColor;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final stackTrailing = trailing != null && constraints.maxWidth < 500;
      final leading = DecoratedBox(
        decoration: BoxDecoration(
          color: (iconColor ?? MacGlassTokens.blue).withValues(alpha: 0.11),
          borderRadius: BorderRadius.circular(6),
        ),
        child: SizedBox.square(
          dimension: 28,
          child: Icon(icon, size: 17, color: iconColor ?? MacGlassTokens.blue),
        ),
      );
      final copy = Text(
        title,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: MacGlassTypography.cardTitle,
      );
      final main = Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          leading,
          const SizedBox(width: 12),
          Expanded(child: copy),
          if (!stackTrailing && trailing != null) ...[
            const SizedBox(width: 14),
            trailing!,
          ],
        ],
      );
      return Padding(
        padding: EdgeInsets.fromLTRB(14, stackTrailing ? 12 : 10, 14, 10),
        child: stackTrailing
            ? Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  main,
                  const SizedBox(height: 9),
                  Align(alignment: Alignment.centerRight, child: trailing),
                ],
              )
            : main,
      );
    },
  );
}

class _CalibrationActionBar extends StatelessWidget {
  const _CalibrationActionBar({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) => Align(
    alignment: Alignment.centerRight,
    child: Wrap(spacing: 8, runSpacing: 8, children: children),
  );
}

class _CalibrationButton extends StatelessWidget {
  const _CalibrationButton({
    required this.label,
    required this.onPressed,
    this.icon,
    this.primary = false,
    this.destructive = false,
    Key? key,
  }) : buttonKey = key;

  final String label;
  final VoidCallback? onPressed;
  final IconData? icon;
  final bool primary;
  final bool destructive;
  final Key? buttonKey;

  static final _shape = RoundedRectangleBorder(
    borderRadius: BorderRadius.circular(7),
  );

  @override
  Widget build(BuildContext context) {
    final foreground = destructive ? MacGlassTokens.red : MacGlassTokens.ink;
    final child = Text(label);
    final style = primary
        ? FilledButton.styleFrom(
            minimumSize: const Size(0, 32),
            padding: const EdgeInsets.symmetric(horizontal: 12),
            visualDensity: VisualDensity.compact,
            tapTargetSize: MaterialTapTargetSize.shrinkWrap,
            backgroundColor: MacGlassTokens.blue,
            foregroundColor: Colors.white,
            disabledBackgroundColor: MacGlassTokens.ink.withValues(alpha: 0.08),
            disabledForegroundColor: MacGlassTokens.muted,
            textStyle: const TextStyle(
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
            shape: _shape,
          )
        : OutlinedButton.styleFrom(
            minimumSize: const Size(0, 32),
            padding: const EdgeInsets.symmetric(horizontal: 12),
            visualDensity: VisualDensity.compact,
            tapTargetSize: MaterialTapTargetSize.shrinkWrap,
            foregroundColor: foreground,
            disabledForegroundColor: MacGlassTokens.muted,
            side: BorderSide(color: foreground.withValues(alpha: 0.22)),
            textStyle: const TextStyle(
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
            shape: _shape,
          );
    if (icon == null) {
      return primary
          ? FilledButton(
              key: buttonKey,
              onPressed: onPressed,
              style: style,
              child: child,
            )
          : OutlinedButton(
              key: buttonKey,
              onPressed: onPressed,
              style: style,
              child: child,
            );
    }
    return primary
        ? FilledButton.icon(
            key: buttonKey,
            onPressed: onPressed,
            style: style,
            icon: Icon(icon, size: 15),
            label: child,
          )
        : OutlinedButton.icon(
            key: buttonKey,
            onPressed: onPressed,
            style: style,
            icon: Icon(icon, size: 15),
            label: child,
          );
  }
}

class _CalibrationHairline extends StatelessWidget {
  const _CalibrationHairline({this.indent = 0});

  final double indent;

  @override
  Widget build(BuildContext context) => Divider(
    height: 1,
    thickness: 1,
    indent: indent,
    color: MacGlassTokens.ink.withValues(alpha: 0.075),
  );
}

class _CalibrationProgress extends StatelessWidget {
  const _CalibrationProgress({required this.status});

  final CalibrationStatusSnapshot status;

  @override
  Widget build(BuildContext context) {
    final progress = status.progress.clamp(0, 100).toDouble() / 100;
    final color = _calibrationStageColor(status);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                _calibrationStatusLabel(status),
                style: TextStyle(
                  color: color,
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            Text(
              '${status.progress}%',
              style: TextStyle(
                color: color,
                fontFamily: 'monospace',
                fontSize: 12,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
        const SizedBox(height: 7),
        TweenAnimationBuilder<double>(
          tween: Tween<double>(end: progress),
          duration: const Duration(milliseconds: 240),
          curve: Curves.easeOutCubic,
          builder: (context, value, child) => ClipRRect(
            borderRadius: BorderRadius.circular(2),
            child: LinearProgressIndicator(
              minHeight: 4,
              value: value,
              color: color,
              backgroundColor: MacGlassTokens.ink.withValues(alpha: 0.07),
            ),
          ),
        ),
      ],
    );
  }
}

Color _calibrationStageColor(CalibrationStatusSnapshot status) {
  if (status.failureReason.isNotEmpty || status.stage == 'failed') {
    return MacGlassTokens.red;
  }
  if (status.stage == 'success') return MacGlassTokens.green;
  if (status.stage == 'waiting_orientation') return MacGlassTokens.amber;
  if (status.active ||
      status.stage == 'calibrating' ||
      status.stage == 'verifying') {
    return MacGlassTokens.blue;
  }
  return MacGlassTokens.muted;
}

FlightControllerParameter? _sensorParameter(
  ParameterController controller,
  String name,
) => controller.parameters
    .where((parameter) => parameter.name == name)
    .firstOrNull;

String _formatSensorNumber(double value) {
  final precision = value.abs() >= 100
      ? 0
      : value.abs() >= 10
      ? 1
      : 2;
  return value.toStringAsFixed(precision);
}

String _formatCalibrationVector(SensorVector3 value) =>
    '${_formatSensorNumber(value.x)}, ${_formatSensorNumber(value.y)}, ${_formatSensorNumber(value.z)}';

String _formatCalibrationTimestamp(int timestampMilliseconds) {
  if (timestampMilliseconds < 946684800000) return ' · 时间未记录';
  final value = DateTime.fromMillisecondsSinceEpoch(
    timestampMilliseconds,
  ).toLocal();
  String two(int number) => number.toString().padLeft(2, '0');
  return ' · ${value.year}-${two(value.month)}-${two(value.day)} ${two(value.hour)}:${two(value.minute)}';
}

String _calibrationResultStageLabel(CalibrationResultSnapshot result) {
  if (result.succeeded) return '校准完成 Completed';
  if (result.failureReason.isNotEmpty || result.stage == 'failed') {
    return '校准未通过 Failed';
  }
  return '结果状态 ${result.stage}';
}

String _calibrationQualityLabel(String quality) => switch (quality) {
  'excellent' => '极佳 Excellent',
  'good' => '良好 Good',
  'marginal' => '临界 Marginal',
  _ => '较差 Poor',
};

String _calibrationStatusLabel(CalibrationStatusSnapshot status) {
  if (status.failureReason.isNotEmpty) return '校准失败：${status.failureReason}';
  return switch (status.stage) {
    'waiting_for_start' => '正在启动校准',
    'waiting_orientation' => '请放置到第 ${status.currentStep} 个姿态',
    'calibrating' => '正在采集校准数据',
    'verifying' => '正在验证校准结果',
    'success' => '校准成功',
    'failed' => '校准失败',
    'cancelled' => '校准已取消',
    _ => status.message.isEmpty ? '待校准' : status.message,
  };
}
