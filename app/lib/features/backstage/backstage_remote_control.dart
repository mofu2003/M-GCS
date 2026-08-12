part of 'backstage_page.dart';

enum _RemoteControlConfigView { mapping, calibration }

class _RemoteAxisDefinition {
  const _RemoteAxisDefinition({
    required this.label,
    required this.surfaceLabel,
    required this.mappingName,
    required this.defaultChannel,
    required this.color,
  });

  final String label;
  final String surfaceLabel;
  final String mappingName;
  final int defaultChannel;
  final Color color;
}

const _remoteAxes = <_RemoteAxisDefinition>[
  _RemoteAxisDefinition(
    label: '横滚',
    surfaceLabel: '副翼',
    mappingName: 'RCMAP_ROLL',
    defaultChannel: 1,
    color: MacGlassTokens.blue,
  ),
  _RemoteAxisDefinition(
    label: '俯仰',
    surfaceLabel: '升降舵',
    mappingName: 'RCMAP_PITCH',
    defaultChannel: 2,
    color: MacGlassTokens.green,
  ),
  _RemoteAxisDefinition(
    label: '油门',
    surfaceLabel: '动力',
    mappingName: 'RCMAP_THROTTLE',
    defaultChannel: 3,
    color: MacGlassTokens.amber,
  ),
  _RemoteAxisDefinition(
    label: '偏航',
    surfaceLabel: '方向舵',
    mappingName: 'RCMAP_YAW',
    defaultChannel: 4,
    color: MacGlassTokens.red,
  ),
];

class _RemoteControlConfigPage extends StatefulWidget {
  const _RemoteControlConfigPage({required this.controller});

  final ParameterController controller;

  @override
  State<_RemoteControlConfigPage> createState() =>
      _RemoteControlConfigPageState();
}

class _RemoteControlConfigPageState extends State<_RemoteControlConfigPage> {
  late final List<ValueNotifier<int>> _inputs = List.generate(
    18,
    (_) => ValueNotifier<int>(0),
  );
  final ValueNotifier<int> _noInput = ValueNotifier<int>(0);
  final List<int> _sampleMins = List<int>.filled(18, 3000);
  final List<int> _sampleMaxs = List<int>.filled(18, 0);
  late final PollingTask _poller;
  int _channelCount = 0;
  _RemoteControlConfigView _view = _RemoteControlConfigView.mapping;
  bool _capturing = false;
  bool _awaitingCenter = false;
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    _poller = PollingTask(
      interval: const Duration(milliseconds: 50),
      onTick: _pollInputs,
    );
    _poller.start();
  }

  @override
  void dispose() {
    _poller.dispose();
    for (final input in _inputs) {
      input.dispose();
    }
    _noInput.dispose();
    super.dispose();
  }

  Future<void> _pollInputs() async {
    final backend = widget.controller.backend;
    if (backend is! RemoteControlInputSessionBackend) return;
    final remoteBackend = backend as RemoteControlInputSessionBackend;
    try {
      final snapshot = await remoteBackend.loadRemoteControlInputs();
      if (!mounted) return;
      for (var index = 0; index < _inputs.length; index++) {
        final value = snapshot.valid ? snapshot.values[index] : 0;
        if (_inputs[index].value != value) _inputs[index].value = value;
      }
      if (_channelCount != snapshot.channelCount) {
        setState(() => _channelCount = snapshot.channelCount);
      }
      if (_capturing && snapshot.valid) {
        var rangeChanged = false;
        for (
          var index = 0;
          index < snapshot.channelCount && index < _inputs.length;
          index++
        ) {
          final value = snapshot.values[index];
          if (value < 800 || value > 2200) continue;
          final nextMin = value < _sampleMins[index]
              ? value
              : _sampleMins[index];
          final nextMax = value > _sampleMaxs[index]
              ? value
              : _sampleMaxs[index];
          rangeChanged =
              rangeChanged ||
              nextMin != _sampleMins[index] ||
              nextMax != _sampleMaxs[index];
          _sampleMins[index] = nextMin;
          _sampleMaxs[index] = nextMax;
        }
        if (rangeChanged) setState(() {});
      }
    } on Object {
      if (!mounted) return;
      for (final input in _inputs) {
        if (input.value != 0) input.value = 0;
      }
    }
  }

  Map<String, FlightControllerParameter> get _parameters => {
    for (final parameter in widget.controller.parameters)
      parameter.name: parameter,
  };

  Map<String, StagedParameterChange> get _staged => {
    for (final change in widget.controller.stagedChanges)
      change.parameter.name: change,
  };

  String? _axisMappingName(
    _RemoteAxisDefinition axis,
    Map<String, FlightControllerParameter> parameters,
  ) {
    final px4Name = axis.mappingName.replaceFirst('RCMAP_', 'RC_MAP_');
    return parameters.containsKey(axis.mappingName)
        ? axis.mappingName
        : parameters.containsKey(px4Name)
        ? px4Name
        : null;
  }

  int _axisChannel(
    _RemoteAxisDefinition axis,
    Map<String, FlightControllerParameter> parameters,
    Map<String, StagedParameterChange> staged,
  ) {
    final name = _axisMappingName(axis, parameters);
    final value = name == null
        ? axis.defaultChannel
        : (staged[name]?.stagedValue ?? parameters[name]!.value).round();
    return value.clamp(1, 16);
  }

  List<ParameterOption> _channelOptions(
    String? parameterName,
    int currentValue, {
    bool allowDisabled = false,
  }) {
    final descriptor = parameterName == null
        ? null
        : widget.controller.descriptorFor(parameterName);
    final byValue = <int, ParameterOption>{};
    for (final option in _distinctParameterOptions(descriptor)) {
      if (option.value < (allowDisabled ? 0 : 1) || option.value > 18) {
        continue;
      }
      byValue[option.value] = option;
    }

    final minValue = descriptor?.minValue;
    final maxValue = descriptor?.maxValue;
    final hasChannelRange =
        minValue != null &&
        maxValue != null &&
        minValue == minValue.roundToDouble() &&
        maxValue == maxValue.roundToDouble() &&
        minValue >= (allowDisabled ? 0 : 1) &&
        maxValue <= 18 &&
        minValue <= maxValue;
    if (byValue.isEmpty) {
      final first = hasChannelRange ? minValue.round() : 1;
      final last = hasChannelRange ? maxValue.round() : 16;
      for (var channel = first; channel <= last; channel++) {
        byValue[channel] = ParameterOption(
          value: channel,
          label: channel == 0 ? '禁用' : '通道$channel',
        );
      }
    }
    byValue.putIfAbsent(
      currentValue,
      () => ParameterOption(
        value: currentValue,
        label: currentValue == 0 ? '禁用' : '通道$currentValue',
      ),
    );
    final options = byValue.values.toList(growable: false);
    options.sort((left, right) => left.value.compareTo(right.value));
    return options;
  }

  String? _rcParameterName(
    int channel,
    String suffix,
    Map<String, FlightControllerParameter> parameters,
  ) {
    for (final name in ['RC${channel}_$suffix', 'CH${channel}_$suffix']) {
      if (parameters.containsKey(name)) return name;
    }
    return null;
  }

  String? _rcReverseParameterName(
    int channel,
    Map<String, FlightControllerParameter> parameters,
  ) {
    for (final name in [
      'RC${channel}_REVERSED',
      'RC${channel}_REV',
      'CH${channel}_REVERSED',
      'CH${channel}_REV',
    ]) {
      if (parameters.containsKey(name)) return name;
    }
    return null;
  }

  bool _reverseEnabled(String name, double value) {
    return name.endsWith('_REV') ? value < 0 : value != 0;
  }

  double _reverseParameterValue(String name, bool enabled) {
    if (name.endsWith('_REV')) return enabled ? -1 : 1;
    return enabled ? 1 : 0;
  }

  double? _parameterValue(
    String? name,
    Map<String, FlightControllerParameter> parameters,
    Map<String, StagedParameterChange> staged,
  ) {
    if (name == null) return null;
    return staged[name]?.stagedValue ?? parameters[name]?.value;
  }

  Future<bool> _writeImmediately(String name, double value) async {
    final result = await widget.controller.writeValueImmediately(name, value);
    if (!mounted || result.success) return result.success;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
    return false;
  }

  Future<void> _writeAxisReverse(String name, bool enabled) async {
    if (!await _writeImmediately(name, _reverseParameterValue(name, enabled))) {
      return;
    }

    final parameters = _parameters;
    final staged = _staged;
    final switchEnable = _parameterValue('SWITCH_ENABLE', parameters, staged);
    if (switchEnable == 1) {
      await _writeImmediately('SWITCH_ENABLE', 0);
    }
  }

  ParameterOption? _servoFunctionOption(
    Map<String, FlightControllerParameter> parameters,
    String label,
  ) {
    final expected = label.toLowerCase();
    for (var channel = 1; channel <= 16; channel++) {
      final name = 'SERVO${channel}_FUNCTION';
      if (!parameters.containsKey(name)) continue;
      final option =
          _distinctParameterOptions(widget.controller.descriptorFor(name))
              .where(
                (item) =>
                    item.label.toLowerCase().replaceAll(' ', '') == expected,
              )
              .firstOrNull;
      if (option != null) return option;
    }
    return null;
  }

  void _beginCalibration() {
    if (_channelCount == 0 || _inputs.every((input) => input.value == 0)) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('尚未收到遥控器通道数据')));
      return;
    }
    for (var index = 0; index < 18; index++) {
      _sampleMins[index] = 3000;
      _sampleMaxs[index] = 0;
    }
    setState(() {
      _capturing = true;
      _awaitingCenter = false;
    });
  }

  void _finishEndpointCapture() {
    final parameters = _parameters;
    final staged = _staged;
    for (final axis in _remoteAxes) {
      final index = _axisChannel(axis, parameters, staged) - 1;
      if (_sampleMins[index] > 2200 ||
          _sampleMaxs[index] < 800 ||
          _sampleMaxs[index] - _sampleMins[index] < 100) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('${axis.label}行程不足，请继续移动到两端')));
        return;
      }
    }
    setState(() {
      _capturing = false;
      _awaitingCenter = true;
    });
  }

  Future<void> _saveCalibration() async {
    if (_saving) return;
    final parameters = _parameters;
    final staged = _staged;
    setState(() => _saving = true);
    final writtenChannels = <int>{};
    try {
      for (final axis in _remoteAxes) {
        final channel = _axisChannel(axis, parameters, staged);
        if (!writtenChannels.add(channel)) continue;
        final index = channel - 1;
        final minName = _rcParameterName(channel, 'MIN', parameters);
        final trimName = _rcParameterName(channel, 'TRIM', parameters);
        final maxName = _rcParameterName(channel, 'MAX', parameters);
        if (minName == null || trimName == null || maxName == null) {
          throw StateError('飞控未提供 RC$channel 校准参数');
        }
        final current = _inputs[index].value;
        if (current < 800 || current > 2200) {
          throw StateError('RC$channel 当前输入无效');
        }
        final trim = current
            .clamp(_sampleMins[index], _sampleMaxs[index])
            .toDouble();
        for (final write in <(String, double)>[
          (minName, _sampleMins[index].toDouble()),
          (trimName, trim),
          (maxName, _sampleMaxs[index].toDouble()),
        ]) {
          if (!await _writeImmediately(write.$1, write.$2)) return;
        }
      }
      if (!mounted) return;
      setState(() {
        _awaitingCenter = false;
        _capturing = false;
      });
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('四轴校准已写入飞控')));
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('保存校准失败：$error')));
    } finally {
      if (mounted) setState(() => _saving = false);
    }
  }

  void _cancelCalibration() {
    setState(() {
      _capturing = false;
      _awaitingCenter = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Align(
          alignment: Alignment.center,
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 360),
            child: SizedBox(
              width: double.infinity,
              height: 36,
              child:
                  cupertino.CupertinoSlidingSegmentedControl<
                    _RemoteControlConfigView
                  >(
                    groupValue: _view,
                    padding: const EdgeInsets.all(2),
                    backgroundColor: MacGlassTokens.ink.withValues(
                      alpha: 0.055,
                    ),
                    thumbColor: Colors.white.withValues(alpha: 0.94),
                    children: const {
                      _RemoteControlConfigView.mapping: Center(
                        child: Text(
                          '通道映射',
                          style: TextStyle(
                            fontSize: 11.5,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                      _RemoteControlConfigView.calibration: Center(
                        child: Text(
                          '摇杆校准',
                          style: TextStyle(
                            fontSize: 11.5,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                    },
                    onValueChanged: (value) {
                      if (value == null) return;
                      if (_capturing || _awaitingCenter) _cancelCalibration();
                      setState(() => _view = value);
                    },
                  ),
            ),
          ),
        ),
        const SizedBox(height: 12),
        Expanded(
          child: _view == _RemoteControlConfigView.mapping
              ? _buildMappingView()
              : _buildCalibrationView(),
        ),
      ],
    );
  }

  Widget _buildMappingView() {
    final parameters = _parameters;
    final staged = _staged;
    final modePrefix = parameters.containsKey('FLTMODE1')
        ? 'FLTMODE'
        : parameters.containsKey('MODE1')
        ? 'MODE'
        : parameters.containsKey('COM_FLTMODE1')
        ? 'COM_FLTMODE'
        : null;
    final modeChannelName = parameters.containsKey('FLTMODE_CH')
        ? 'FLTMODE_CH'
        : parameters.containsKey('MODE_CH')
        ? 'MODE_CH'
        : null;
    final modeChannel =
        (_parameterValue(modeChannelName, parameters, staged) ?? 5).round();
    final modeChannelOptions = _channelOptions(
      modeChannelName,
      modeChannel,
      allowDisabled: true,
    );
    final flightModes = _RemoteFlightModePanel(
      controller: widget.controller,
      parameters: parameters,
      staged: staged,
      modePrefix: modePrefix,
      modeChannelName: modeChannelName,
      modeChannel: modeChannel,
      modeChannelOptions: modeChannelOptions,
      input: modeChannel >= 1 && modeChannel <= _inputs.length
          ? _inputs[modeChannel - 1]
          : _noInput,
      onWrite: _writeImmediately,
    );
    final channelOptions = _RemoteAuxOptionsPanel(
      controller: widget.controller,
      parameters: parameters,
      staged: staged,
      inputs: _inputs,
      onWrite: _writeImmediately,
    );
    final elevonLeft = _servoFunctionOption(parameters, 'elevonleft');
    final elevonRight = _servoFunctionOption(parameters, 'elevonright');
    final mappingContent = LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth >= 900) {
          return Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Expanded(flex: 10, child: flightModes),
              const SizedBox(width: 12),
              Expanded(flex: 11, child: channelOptions),
            ],
          );
        }
        final groupHeight = (constraints.maxHeight * 0.82)
            .clamp(420.0, 560.0)
            .toDouble();
        return SingleChildScrollView(
          child: Column(
            children: [
              SizedBox(height: groupHeight, child: flightModes),
              const SizedBox(height: 12),
              SizedBox(height: groupHeight, child: channelOptions),
            ],
          ),
        );
      },
    );
    if (elevonLeft == null || elevonRight == null) return mappingContent;
    return Column(
      children: [
        Expanded(child: mappingContent),
        const SizedBox(height: 10),
        _RemoteModernElevonConfigBar(
          parameters: parameters,
          staged: staged,
          enabled: widget.controller.canEdit,
          leftFunction: elevonLeft.value,
          rightFunction: elevonRight.value,
          onWrite: _writeImmediately,
        ),
      ],
    );
  }

  Widget _buildCalibrationView() {
    final parameters = _parameters;
    final staged = _staged;
    return Column(
      children: [
        Expanded(
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 960),
              child: SizedBox(
                width: double.infinity,
                child: LayoutBuilder(
                  builder: (context, constraints) {
                    final columns = constraints.maxWidth >= 700 ? 2 : 1;
                    return GridView.builder(
                      padding: const EdgeInsets.symmetric(horizontal: 2),
                      gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                        crossAxisCount: columns,
                        crossAxisSpacing: 12,
                        mainAxisSpacing: 12,
                        mainAxisExtent: 204,
                      ),
                      itemCount: _remoteAxes.length,
                      itemBuilder: (context, index) {
                        final axis = _remoteAxes[index];
                        final channel = _axisChannel(axis, parameters, staged);
                        final mappingName = _axisMappingName(axis, parameters);
                        final channelIndex = channel - 1;
                        final minName = _rcParameterName(
                          channel,
                          'MIN',
                          parameters,
                        );
                        final trimName = _rcParameterName(
                          channel,
                          'TRIM',
                          parameters,
                        );
                        final maxName = _rcParameterName(
                          channel,
                          'MAX',
                          parameters,
                        );
                        final reverseName = _rcReverseParameterName(
                          channel,
                          parameters,
                        );
                        final reverseValue = _parameterValue(
                          reverseName,
                          parameters,
                          staged,
                        );
                        final sampled =
                            _sampleMins[channelIndex] <= 2200 &&
                            _sampleMaxs[channelIndex] >= 800;
                        return _RemoteAxisCard(
                          axis: axis,
                          channel: channel,
                          channelOptions: _channelOptions(mappingName, channel),
                          mappingEnabled:
                              mappingName != null &&
                              widget.controller.canEdit &&
                              !_capturing &&
                              !_awaitingCenter &&
                              !_saving,
                          reverseEnabled:
                              reverseName != null &&
                              widget.controller.canEdit &&
                              !_capturing &&
                              !_awaitingCenter &&
                              !_saving,
                          onChannelChanged: mappingName == null
                              ? null
                              : (value) => unawaited(
                                  _writeImmediately(
                                    mappingName,
                                    value.toDouble(),
                                  ),
                                ),
                          input: _inputs[channelIndex],
                          reversed: reverseName != null && reverseValue != null
                              ? _reverseEnabled(reverseName, reverseValue)
                              : null,
                          onReversedChanged: reverseName == null
                              ? null
                              : (enabled) => unawaited(
                                  _writeAxisReverse(reverseName, enabled),
                                ),
                          minValue: sampled
                              ? _sampleMins[channelIndex]
                              : (_parameterValue(minName, parameters, staged) ??
                                        1000)
                                    .round(),
                          trimValue:
                              (_parameterValue(trimName, parameters, staged) ??
                                      1500)
                                  .round(),
                          maxValue: sampled
                              ? _sampleMaxs[channelIndex]
                              : (_parameterValue(maxName, parameters, staged) ??
                                        2000)
                                    .round(),
                        );
                      },
                    );
                  },
                ),
              ),
            ),
          ),
        ),
        const SizedBox(height: 12),
        Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 960),
            child: SizedBox(
              width: double.infinity,
              child: _RemoteCalibrationActionBar(
                capturing: _capturing,
                awaitingCenter: _awaitingCenter,
                saving: _saving,
                onBegin: widget.controller.canEdit ? _beginCalibration : null,
                onFinish: _finishEndpointCapture,
                onSave: _saveCalibration,
                onCancel: _cancelCalibration,
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _RemoteFlightModePanel extends StatelessWidget {
  const _RemoteFlightModePanel({
    required this.controller,
    required this.parameters,
    required this.staged,
    required this.modePrefix,
    required this.modeChannelName,
    required this.modeChannel,
    required this.modeChannelOptions,
    required this.input,
    required this.onWrite,
  });

  final ParameterController controller;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final String? modePrefix;
  final String? modeChannelName;
  final int modeChannel;
  final List<ParameterOption> modeChannelOptions;
  final ValueListenable<int> input;
  final Future<bool> Function(String name, double value) onWrite;

  static const _ranges = [
    '0-1230',
    '1231-1360',
    '1361-1490',
    '1491-1620',
    '1621-1749',
    '1750+',
  ];

  int _value(String name) =>
      (staged[name]?.stagedValue ?? parameters[name]?.value ?? 0).round();

  @override
  Widget build(BuildContext context) {
    return _GlassPanel(
      radius: 8,
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 13, 16, 12),
            child: Row(
              children: [
                Expanded(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const _RemotePanelTitle('飞行模式通道'),
                      const SizedBox(height: 2),
                      Text(
                        modeChannel > 0 ? 'RC$modeChannel' : '未启用',
                        style: const TextStyle(
                          color: MacGlassTokens.muted,
                          fontSize: 11,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ],
                  ),
                ),
                SizedBox(
                  width: 144,
                  child: modeChannelName == null
                      ? const _ServoUnavailableCell('飞控未提供通道参数')
                      : _MacParameterPicker(
                          key: const ValueKey('mg-remote-flight-mode-channel'),
                          value: modeChannel,
                          options: modeChannelOptions,
                          enabled: controller.canEdit,
                          label: '',
                          compact: true,
                          inset: true,
                          showValue: false,
                          onChanged: (value) => unawaited(
                            onWrite(modeChannelName!, value.toDouble()),
                          ),
                        ),
                ),
              ],
            ),
          ),
          Divider(height: 1, color: MacGlassTokens.ink.withValues(alpha: 0.08)),
          Expanded(
            child: modePrefix == null
                ? const Center(child: Text('飞控未提供六档飞行模式参数'))
                : ValueListenableBuilder<int>(
                    valueListenable: input,
                    builder: (context, pwm, child) {
                      final activeSlot = _flightModeSlot(pwm);
                      return ListView.separated(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 8,
                          vertical: 8,
                        ),
                        itemCount: 6,
                        separatorBuilder: (context, index) => Divider(
                          height: 1,
                          indent: 12,
                          endIndent: 12,
                          color: MacGlassTokens.ink.withValues(alpha: 0.055),
                        ),
                        itemBuilder: (context, index) {
                          final name = '$modePrefix${index + 1}';
                          final descriptor =
                              controller.descriptorFor(name) ??
                              controller.descriptorFor('${modePrefix}1');
                          final options = _distinctParameterOptions(descriptor);
                          final active = activeSlot == index;
                          return Container(
                            key: ValueKey('mg-remote-flight-mode-${index + 1}'),
                            constraints: const BoxConstraints(minHeight: 56),
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 8,
                            ),
                            decoration: BoxDecoration(
                              color: active
                                  ? MacGlassTokens.blue.withValues(alpha: 0.12)
                                  : Colors.transparent,
                              borderRadius: BorderRadius.circular(
                                MacGlassTokens.radiusControl,
                              ),
                            ),
                            child: Row(
                              children: [
                                SizedBox(
                                  width: 102,
                                  child: Column(
                                    mainAxisSize: MainAxisSize.min,
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      Row(
                                        children: [
                                          AnimatedContainer(
                                            duration: const Duration(
                                              milliseconds: 140,
                                            ),
                                            width: 6,
                                            height: 6,
                                            decoration: BoxDecoration(
                                              color: active
                                                  ? MacGlassTokens.blue
                                                  : Colors.transparent,
                                              shape: BoxShape.circle,
                                            ),
                                          ),
                                          SizedBox(width: active ? 6 : 0),
                                          Text(
                                            '模式${index + 1}',
                                            style: TextStyle(
                                              color: active
                                                  ? MacGlassTokens.blue
                                                  : MacGlassTokens.ink,
                                              fontSize: 13,
                                              fontWeight: FontWeight.w700,
                                            ),
                                          ),
                                        ],
                                      ),
                                      const SizedBox(height: 2),
                                      Text(
                                        _ranges[index],
                                        style: const TextStyle(
                                          color: MacGlassTokens.muted,
                                          fontSize: 10.5,
                                          fontFeatures: [
                                            FontFeature.tabularFigures(),
                                          ],
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                                const SizedBox(width: 8),
                                Expanded(
                                  child:
                                      parameters.containsKey(name) &&
                                          options.isNotEmpty
                                      ? _MacParameterPicker(
                                          key: ValueKey(
                                            'mg-remote-flight-mode-picker-${index + 1}',
                                          ),
                                          value: _value(name),
                                          options: options,
                                          enabled: controller.canEdit,
                                          label: '',
                                          compact: true,
                                          inset: true,
                                          showValue: false,
                                          onChanged: (value) => unawaited(
                                            onWrite(name, value.toDouble()),
                                          ),
                                        )
                                      : const _ServoUnavailableCell('无飞行模式元数据'),
                                ),
                              ],
                            ),
                          );
                        },
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class _RemoteAuxOptionsPanel extends StatelessWidget {
  const _RemoteAuxOptionsPanel({
    required this.controller,
    required this.parameters,
    required this.staged,
    required this.inputs,
    required this.onWrite,
  });

  final ParameterController controller;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final List<ValueListenable<int>> inputs;
  final Future<bool> Function(String name, double value) onWrite;

  String? _name(int channel, String suffix) {
    for (final name in ['RC${channel}_$suffix', 'CH${channel}_$suffix']) {
      if (parameters.containsKey(name)) return name;
    }
    return null;
  }

  double? _value(String? name) => name == null
      ? null
      : staged[name]?.stagedValue ?? parameters[name]?.value;

  @override
  Widget build(BuildContext context) {
    return _GlassPanel(
      radius: 8,
      child: Column(
        children: [
          const Padding(
            padding: EdgeInsets.fromLTRB(16, 13, 16, 12),
            child: Row(
              children: [
                Expanded(child: _RemotePanelTitle('其他通道开关选项')),
                Text(
                  'RC5-16',
                  style: TextStyle(
                    color: MacGlassTokens.muted,
                    fontSize: 11,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
          ),
          Divider(height: 1, color: MacGlassTokens.ink.withValues(alpha: 0.08)),
          Expanded(
            child: ListView.separated(
              key: const ValueKey('mg-remote-aux-channel-list'),
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
              itemCount: 12,
              separatorBuilder: (context, index) => Divider(
                height: 1,
                indent: 12,
                endIndent: 12,
                color: MacGlassTokens.ink.withValues(alpha: 0.055),
              ),
              itemBuilder: (context, index) {
                final channel = index + 5;
                final optionName = _name(channel, 'OPTION');
                final trimName = _name(channel, 'TRIM');
                final value = (_value(optionName) ?? 0).round();
                final options = _distinctParameterOptions(
                  optionName == null
                      ? null
                      : controller.descriptorFor(optionName),
                );
                final trim = (_value(trimName) ?? 1500).round();
                return ValueListenableBuilder<int>(
                  valueListenable: inputs[channel - 1],
                  builder: (context, pwm, child) {
                    final active = value != 0 && pwm > trim;
                    return Container(
                      key: ValueKey('mg-remote-aux-channel-$channel'),
                      constraints: const BoxConstraints(minHeight: 56),
                      padding: const EdgeInsets.symmetric(
                        horizontal: 12,
                        vertical: 8,
                      ),
                      decoration: BoxDecoration(
                        color: active
                            ? MacGlassTokens.blue.withValues(alpha: 0.12)
                            : Colors.transparent,
                        borderRadius: BorderRadius.circular(
                          MacGlassTokens.radiusControl,
                        ),
                      ),
                      child: Row(
                        children: [
                          SizedBox(
                            width: 82,
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Row(
                                  children: [
                                    AnimatedContainer(
                                      duration: const Duration(
                                        milliseconds: 140,
                                      ),
                                      width: 6,
                                      height: 6,
                                      decoration: BoxDecoration(
                                        color: active
                                            ? MacGlassTokens.blue
                                            : Colors.transparent,
                                        shape: BoxShape.circle,
                                      ),
                                    ),
                                    SizedBox(width: active ? 6 : 0),
                                    Text(
                                      '通道$channel',
                                      style: TextStyle(
                                        color: active
                                            ? MacGlassTokens.blue
                                            : MacGlassTokens.ink,
                                        fontSize: 13,
                                        fontWeight: FontWeight.w700,
                                      ),
                                    ),
                                  ],
                                ),
                                const SizedBox(height: 2),
                                Text(
                                  pwm > 0 ? '$pwm PWM' : '--',
                                  style: const TextStyle(
                                    color: MacGlassTokens.muted,
                                    fontSize: 10.5,
                                    fontFeatures: [
                                      FontFeature.tabularFigures(),
                                    ],
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                            child: optionName != null && options.isNotEmpty
                                ? _MacParameterPicker(
                                    key: ValueKey(
                                      'mg-remote-aux-channel-$channel-option',
                                    ),
                                    value: value,
                                    options: options,
                                    enabled: controller.canEdit,
                                    label: '',
                                    compact: true,
                                    inset: true,
                                    showValue: false,
                                    onChanged: (updated) => unawaited(
                                      onWrite(optionName, updated.toDouble()),
                                    ),
                                  )
                                : const _ServoUnavailableCell('无通道功能元数据'),
                          ),
                        ],
                      ),
                    );
                  },
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _RemotePanelTitle extends StatelessWidget {
  const _RemotePanelTitle(this.text);

  final String text;

  @override
  Widget build(BuildContext context) => Text(
    text,
    style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w700),
  );
}

class _RemoteLabeledSwitch extends StatelessWidget {
  const _RemoteLabeledSwitch({
    required this.switchKey,
    required this.label,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final Key switchKey;
  final String label;
  final bool value;
  final bool enabled;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => Row(
    mainAxisSize: MainAxisSize.min,
    children: [
      Text(
        label,
        style: TextStyle(
          color: enabled ? MacGlassTokens.ink : MacGlassTokens.muted,
          fontSize: 10.5,
          fontWeight: FontWeight.w600,
        ),
      ),
      const SizedBox(width: 5),
      SizedBox(
        width: 36,
        height: 22,
        child: FittedBox(
          fit: BoxFit.contain,
          child: cupertino.CupertinoSwitch(
            key: switchKey,
            value: value,
            onChanged: enabled ? onChanged : null,
            activeTrackColor: MacGlassTokens.blue,
            inactiveTrackColor: MacGlassTokens.ink.withValues(alpha: 0.12),
            thumbColor: Colors.white,
          ),
        ),
      ),
    ],
  );
}

class _RemoteModernElevonConfigBar extends StatelessWidget {
  const _RemoteModernElevonConfigBar({
    required this.parameters,
    required this.staged,
    required this.enabled,
    required this.leftFunction,
    required this.rightFunction,
    required this.onWrite,
  });

  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final bool enabled;
  final int leftFunction;
  final int rightFunction;
  final Future<bool> Function(String name, double value) onWrite;

  List<int> get _availableChannels {
    final channels = <int>[];
    for (var channel = 1; channel <= 16; channel++) {
      if (parameters.containsKey('SERVO${channel}_FUNCTION')) {
        channels.add(channel);
      }
    }
    return channels;
  }

  double _value(String name) =>
      staged[name]?.stagedValue ?? parameters[name]?.value ?? 0;

  int _assignedChannel(int function) {
    for (final channel in _availableChannels) {
      if (_value('SERVO${channel}_FUNCTION').round() == function) {
        return channel;
      }
    }
    return 0;
  }

  String? _reverseName(int channel) {
    final name = 'SERVO${channel}_REVERSED';
    return parameters.containsKey(name) ? name : null;
  }

  Future<void> _assignFunction(
    int function,
    int currentChannel,
    int nextChannel,
  ) async {
    if (currentChannel == nextChannel) return;
    if (nextChannel > 0 &&
        !await onWrite('SERVO${nextChannel}_FUNCTION', function.toDouble())) {
      return;
    }
    if (currentChannel > 0) {
      await onWrite('SERVO${currentChannel}_FUNCTION', 0);
    }
  }

  @override
  Widget build(BuildContext context) {
    final channels = _availableChannels;
    final options = <ParameterOption>[
      const ParameterOption(value: 0, label: '未分配'),
      for (final channel in channels)
        ParameterOption(value: channel, label: 'SERVO$channel'),
    ];
    return Container(
      key: const ValueKey('mg-remote-modern-elevon-settings'),
      padding: const EdgeInsets.fromLTRB(14, 11, 14, 12),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.62),
        border: Border.all(color: Colors.white.withValues(alpha: 0.9)),
        borderRadius: BorderRadius.circular(8),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          final rows = <Widget>[
            _RemoteModernElevonRow(
              key: const ValueKey('mg-remote-elevon-left'),
              label: '左副翼',
              channel: _assignedChannel(leftFunction),
              options: options,
              enabled: enabled,
              reverseName: _reverseName(_assignedChannel(leftFunction)),
              reversed:
                  _value(_reverseName(_assignedChannel(leftFunction)) ?? '') !=
                  0,
              onChannelChanged: (channel) => unawaited(
                _assignFunction(
                  leftFunction,
                  _assignedChannel(leftFunction),
                  channel,
                ),
              ),
              onReversedChanged: (value) {
                final name = _reverseName(_assignedChannel(leftFunction));
                if (name != null) unawaited(onWrite(name, value ? 1 : 0));
              },
            ),
            _RemoteModernElevonRow(
              key: const ValueKey('mg-remote-elevon-right'),
              label: '右副翼',
              channel: _assignedChannel(rightFunction),
              options: options,
              enabled: enabled,
              reverseName: _reverseName(_assignedChannel(rightFunction)),
              reversed:
                  _value(_reverseName(_assignedChannel(rightFunction)) ?? '') !=
                  0,
              onChannelChanged: (channel) => unawaited(
                _assignFunction(
                  rightFunction,
                  _assignedChannel(rightFunction),
                  channel,
                ),
              ),
              onReversedChanged: (value) {
                final name = _reverseName(_assignedChannel(rightFunction));
                if (name != null) unawaited(onWrite(name, value ? 1 : 0));
              },
            ),
          ];
          final controls = Wrap(spacing: 12, runSpacing: 8, children: rows);
          const title = Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                '副翼输出配置',
                style: TextStyle(fontSize: 13, fontWeight: FontWeight.w700),
              ),
              SizedBox(height: 2),
              Text(
                'Plane',
                style: TextStyle(
                  color: MacGlassTokens.muted,
                  fontSize: 10,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          );
          if (constraints.maxWidth < 720) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [title, const SizedBox(height: 10), controls],
            );
          }
          return Row(
            children: [
              title,
              const SizedBox(width: 24),
              Expanded(
                child: Align(alignment: Alignment.centerRight, child: controls),
              ),
            ],
          );
        },
      ),
    );
  }
}

class _RemoteModernElevonRow extends StatelessWidget {
  const _RemoteModernElevonRow({
    required this.label,
    required this.channel,
    required this.options,
    required this.enabled,
    required this.reverseName,
    required this.reversed,
    required this.onChannelChanged,
    required this.onReversedChanged,
    super.key,
  });

  final String label;
  final int channel;
  final List<ParameterOption> options;
  final bool enabled;
  final String? reverseName;
  final bool reversed;
  final ValueChanged<int> onChannelChanged;
  final ValueChanged<bool> onReversedChanged;

  @override
  Widget build(BuildContext context) => SizedBox(
    width: 244,
    child: Row(
      children: [
        SizedBox(
          width: 48,
          child: Text(
            label,
            style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700),
          ),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: _MacParameterPicker(
            key: ValueKey('mg-remote-modern-elevon-$label-output'),
            value: channel,
            options: options,
            enabled: enabled,
            label: '',
            compact: true,
            inset: true,
            showValue: false,
            onChanged: onChannelChanged,
          ),
        ),
        const SizedBox(width: 8),
        _RemoteLabeledSwitch(
          switchKey: ValueKey('mg-remote-modern-elevon-$label-reversed'),
          label: '反向',
          value: reversed,
          enabled: enabled && reverseName != null && channel != 0,
          onChanged: onReversedChanged,
        ),
      ],
    ),
  );
}

class _RemoteAxisCard extends StatelessWidget {
  const _RemoteAxisCard({
    required this.axis,
    required this.channel,
    required this.channelOptions,
    required this.mappingEnabled,
    required this.reverseEnabled,
    required this.onChannelChanged,
    required this.reversed,
    required this.onReversedChanged,
    required this.input,
    required this.minValue,
    required this.trimValue,
    required this.maxValue,
  });

  final _RemoteAxisDefinition axis;
  final int channel;
  final List<ParameterOption> channelOptions;
  final bool mappingEnabled;
  final bool reverseEnabled;
  final ValueChanged<int>? onChannelChanged;
  final bool? reversed;
  final ValueChanged<bool>? onReversedChanged;
  final ValueListenable<int> input;
  final int minValue;
  final int trimValue;
  final int maxValue;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.62),
        border: Border.all(color: Colors.white.withValues(alpha: 0.9)),
        borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 13),
        child: ValueListenableBuilder<int>(
          valueListenable: input,
          builder: (context, value, child) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Container(
                      width: 8,
                      height: 8,
                      decoration: BoxDecoration(
                        color: axis.color,
                        shape: BoxShape.circle,
                      ),
                    ),
                    const SizedBox(width: 8),
                    Column(
                      mainAxisSize: MainAxisSize.min,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          axis.label,
                          style: const TextStyle(
                            fontSize: 14,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        Text(
                          axis.surfaceLabel,
                          style: const TextStyle(
                            color: MacGlassTokens.muted,
                            fontSize: 10,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ],
                    ),
                    const Spacer(),
                    if (reversed != null && onReversedChanged != null) ...[
                      _RemoteLabeledSwitch(
                        switchKey: ValueKey(
                          'mg-remote-axis-${axis.mappingName.toLowerCase()}-reversed',
                        ),
                        label: '反向',
                        value: reversed!,
                        enabled: reverseEnabled,
                        onChanged: onReversedChanged!,
                      ),
                      const SizedBox(width: 8),
                    ],
                    SizedBox(
                      width: 100,
                      child: onChannelChanged == null
                          ? Text(
                              'RC$channel',
                              textAlign: TextAlign.end,
                              style: const TextStyle(
                                color: MacGlassTokens.muted,
                                fontSize: 11,
                                fontWeight: FontWeight.w700,
                              ),
                            )
                          : _MacParameterPicker(
                              key: ValueKey(
                                'mg-remote-axis-${axis.mappingName.toLowerCase()}-channel',
                              ),
                              value: channel,
                              options: channelOptions,
                              enabled: mappingEnabled,
                              onChanged: onChannelChanged!,
                              label: '',
                              compact: true,
                              inset: true,
                              centered: true,
                              showValue: false,
                            ),
                    ),
                  ],
                ),
                const SizedBox(height: 11),
                Center(
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Text(
                        value > 0 ? '$value' : '--',
                        style: TextStyle(
                          color: value > 0
                              ? MacGlassTokens.ink
                              : MacGlassTokens.muted,
                          fontSize: 28,
                          fontWeight: FontWeight.w700,
                          fontFeatures: const [FontFeature.tabularFigures()],
                        ),
                      ),
                      const SizedBox(width: 5),
                      const Text(
                        'PWM',
                        style: TextStyle(
                          color: MacGlassTokens.muted,
                          fontSize: 10.5,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 7),
                RepaintBoundary(
                  child: _RemotePwmScale(
                    value: value,
                    minValue: minValue,
                    trimValue: trimValue,
                    maxValue: maxValue,
                    reversed: reversed ?? false,
                    color: axis.color,
                  ),
                ),
                const SizedBox(height: 9),
                Row(
                  children: [
                    Expanded(child: _RemoteCalibrationValue('最小', minValue)),
                    Container(
                      width: 1,
                      height: 24,
                      color: MacGlassTokens.ink.withValues(alpha: 0.07),
                    ),
                    Expanded(child: _RemoteCalibrationValue('中位', trimValue)),
                    Container(
                      width: 1,
                      height: 24,
                      color: MacGlassTokens.ink.withValues(alpha: 0.07),
                    ),
                    Expanded(child: _RemoteCalibrationValue('最大', maxValue)),
                  ],
                ),
              ],
            );
          },
        ),
      ),
    );
  }
}

class _RemoteCalibrationValue extends StatelessWidget {
  const _RemoteCalibrationValue(this.label, this.value);

  final String label;
  final int value;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.center,
    children: [
      Text(
        label,
        style: const TextStyle(
          color: MacGlassTokens.muted,
          fontSize: 10,
          fontWeight: FontWeight.w600,
        ),
      ),
      const SizedBox(height: 2),
      Text(
        '$value',
        style: const TextStyle(
          fontSize: 12.5,
          fontWeight: FontWeight.w700,
          fontFeatures: [FontFeature.tabularFigures()],
        ),
      ),
    ],
  );
}

class _RemotePwmScale extends StatelessWidget {
  const _RemotePwmScale({
    required this.value,
    required this.minValue,
    required this.trimValue,
    required this.maxValue,
    required this.reversed,
    required this.color,
  });

  final int value;
  final int minValue;
  final int trimValue;
  final int maxValue;
  final bool reversed;
  final Color color;

  double _position(int pwm) {
    final normalized = ((pwm - 800) / 1400).clamp(0.0, 1.0);
    return reversed ? 1 - normalized : normalized;
  }

  @override
  Widget build(BuildContext context) {
    final valid = value > 0;
    final markers = <double>[
      _position(minValue),
      _position(trimValue),
      _position(maxValue),
    ];
    final target = valid ? _position(value) : 0.0;
    return SizedBox(
      height: 18,
      child: LayoutBuilder(
        builder: (context, constraints) {
          final trackWidth = constraints.maxWidth;
          return Stack(
            clipBehavior: Clip.none,
            alignment: Alignment.centerLeft,
            children: [
              Positioned(
                left: 0,
                right: 0,
                top: 7,
                child: Container(
                  height: 4,
                  decoration: BoxDecoration(
                    color: MacGlassTokens.ink.withValues(alpha: 0.075),
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
              ),
              for (final marker in markers)
                Positioned(
                  left: (trackWidth * marker - 0.5).clamp(0.0, trackWidth - 1),
                  top: 3,
                  child: Container(
                    width: 1,
                    height: 12,
                    color: MacGlassTokens.muted.withValues(alpha: 0.45),
                  ),
                ),
              TweenAnimationBuilder<double>(
                tween: Tween<double>(end: target),
                duration: const Duration(milliseconds: 75),
                curve: Curves.linear,
                builder: (context, progress, child) => Positioned(
                  left: (trackWidth * progress - 5).clamp(0.0, trackWidth - 10),
                  top: 4,
                  child: AnimatedOpacity(
                    duration: const Duration(milliseconds: 120),
                    opacity: valid ? 1 : 0.35,
                    child: Container(
                      width: 10,
                      height: 10,
                      decoration: BoxDecoration(
                        color: color,
                        shape: BoxShape.circle,
                        border: Border.all(
                          color: Colors.white.withValues(alpha: 0.95),
                          width: 2,
                        ),
                        boxShadow: [
                          BoxShadow(
                            color: color.withValues(alpha: 0.25),
                            blurRadius: 5,
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

class _RemoteCalibrationActionBar extends StatelessWidget {
  const _RemoteCalibrationActionBar({
    required this.capturing,
    required this.awaitingCenter,
    required this.saving,
    required this.onBegin,
    required this.onFinish,
    required this.onSave,
    required this.onCancel,
  });

  final bool capturing;
  final bool awaitingCenter;
  final bool saving;
  final VoidCallback? onBegin;
  final VoidCallback onFinish;
  final VoidCallback onSave;
  final VoidCallback onCancel;

  @override
  Widget build(BuildContext context) {
    final status = saving
        ? '正在写入校准参数'
        : awaitingCenter
        ? '摇杆回中，油门置低'
        : capturing
        ? '移动四个摇杆至全部端点'
        : '等待开始';
    final statusColor = saving
        ? MacGlassTokens.amber
        : awaitingCenter
        ? MacGlassTokens.amber
        : capturing
        ? MacGlassTokens.blue
        : MacGlassTokens.muted;
    final primaryLabel = capturing
        ? '完成端点采样'
        : awaitingCenter
        ? '保存中心与低位'
        : '开始校准';
    final primaryIcon = capturing
        ? Icons.stop_rounded
        : awaitingCenter
        ? Icons.save_rounded
        : Icons.play_arrow_rounded;
    final primaryAction = saving
        ? null
        : capturing
        ? onFinish
        : awaitingCenter
        ? onSave
        : onBegin;

    return Container(
      key: const ValueKey('mg-remote-calibration-actions'),
      constraints: const BoxConstraints(minHeight: 56),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.62),
        border: Border.all(color: Colors.white.withValues(alpha: 0.9)),
        borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          final statusView = Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (saving)
                const SizedBox(
                  width: 14,
                  height: 14,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              else
                AnimatedContainer(
                  duration: const Duration(milliseconds: 150),
                  width: 8,
                  height: 8,
                  decoration: BoxDecoration(
                    color: statusColor,
                    shape: BoxShape.circle,
                  ),
                ),
              const SizedBox(width: 9),
              Flexible(
                child: Text(
                  status,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ],
          );
          final actions = Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (capturing || awaitingCenter) ...[
                IconButton(
                  tooltip: '取消校准',
                  onPressed: saving ? null : onCancel,
                  icon: const Icon(Icons.close_rounded, size: 18),
                  visualDensity: VisualDensity.compact,
                ),
                const SizedBox(width: 4),
              ],
              FilledButton.icon(
                key: const ValueKey('mg-remote-calibration-primary'),
                onPressed: primaryAction,
                style: FilledButton.styleFrom(
                  minimumSize: const Size(0, 36),
                  padding: const EdgeInsets.symmetric(horizontal: 13),
                  backgroundColor: MacGlassTokens.blue,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(10),
                  ),
                  textStyle: const TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                icon: Icon(primaryIcon, size: 16),
                label: Text(primaryLabel),
              ),
            ],
          );
          if (constraints.maxWidth < 590) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                statusView,
                const SizedBox(height: 8),
                Align(alignment: Alignment.centerRight, child: actions),
              ],
            );
          }
          return Row(
            children: [
              Expanded(child: statusView),
              actions,
            ],
          );
        },
      ),
    );
  }
}

int _flightModeSlot(int pwm) {
  if (pwm <= 0) return -1;
  if (pwm <= 1230) return 0;
  if (pwm <= 1360) return 1;
  if (pwm <= 1490) return 2;
  if (pwm <= 1620) return 3;
  if (pwm <= 1749) return 4;
  return 5;
}

List<int> _serialPortNumbers(List<FlightControllerParameter> parameters) {
  final ports = <int>{};
  final pattern = RegExp(r'^SERIAL(\d+)_BAUD$');
  for (final parameter in parameters) {
    final match = pattern.firstMatch(parameter.name);
    final port = match == null ? null : int.tryParse(match.group(1)!);
    if (port != null) ports.add(port);
  }
  return ports.toList()..sort();
}
