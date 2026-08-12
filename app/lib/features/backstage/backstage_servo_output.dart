part of 'backstage_page.dart';

class _ServoOutputPage extends StatefulWidget {
  const _ServoOutputPage({required this.controller});

  final ParameterController controller;

  @override
  State<_ServoOutputPage> createState() => _ServoOutputPageState();
}

class _ServoOutputPageState extends State<_ServoOutputPage> {
  final _scrollController = ScrollController();
  late final List<ValueNotifier<int>> _outputValues = List.generate(
    16,
    (_) => ValueNotifier<int>(0),
  );
  late final PollingTask _poller;

  @override
  void initState() {
    super.initState();
    _poller = PollingTask(
      interval: const Duration(milliseconds: 50),
      onTick: _pollOutputs,
    );
    unawaited(_pollOutputs());
    _poller.start();
  }

  @override
  void dispose() {
    _poller.dispose();
    for (final output in _outputValues) {
      output.dispose();
    }
    _scrollController.dispose();
    super.dispose();
  }

  void _publishOutputs(ServoOutputSnapshot outputs) {
    for (var index = 0; index < _outputValues.length; index++) {
      final value = outputs.valid ? outputs.values[index] : 0;
      if (_outputValues[index].value != value) {
        _outputValues[index].value = value;
      }
    }
  }

  Future<void> _pollOutputs() async {
    final backend = widget.controller.backend;
    if (backend is! ServoOutputSessionBackend) return;
    final servoBackend = backend as ServoOutputSessionBackend;
    try {
      final outputs = await servoBackend.loadServoOutputs();
      if (mounted) _publishOutputs(outputs);
    } on Object {
      if (mounted) {
        _publishOutputs(
          ServoOutputSnapshot(values: List<int>.filled(16, 0), valid: false),
        );
      }
    }
  }

  Future<void> _writeImmediately(String name, double value) async {
    final result = await widget.controller.writeValueImmediately(name, value);
    if (!mounted || result.success) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
  }

  @override
  Widget build(BuildContext context) {
    final parameters = {
      for (final parameter in widget.controller.parameters)
        parameter.name: parameter,
    };
    final staged = {
      for (final change in widget.controller.stagedChanges)
        change.parameter.name: change,
    };
    return _BackstageFeatureCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Padding(
            padding: EdgeInsets.fromLTRB(18, 16, 18, 10),
            child: _ServoOutputTableHeader(),
          ),
          const Divider(height: 1),
          Expanded(
            child: Scrollbar(
              controller: _scrollController,
              thumbVisibility: true,
              child: ListView.separated(
                key: const ValueKey('mg-servo-output-list'),
                controller: _scrollController,
                padding: const EdgeInsets.fromLTRB(18, 10, 18, 12),
                itemCount: _outputValues.length,
                itemBuilder: (context, index) => _ServoChannelRow(
                  channel: index + 1,
                  output: _outputValues[index],
                  parameters: parameters,
                  staged: staged,
                  controller: widget.controller,
                  onWrite: _writeImmediately,
                ),
                separatorBuilder: (_, _) => const Divider(height: 14),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ServoOutputTableHeader extends StatelessWidget {
  const _ServoOutputTableHeader();

  @override
  Widget build(BuildContext context) => const Row(
    children: [
      SizedBox(width: 74, child: _ServoTableLabel('通道')),
      Expanded(flex: 3, child: _ServoTableLabel('功能')),
      SizedBox(width: 80, child: _ServoTableLabel('反向')),
      Expanded(child: _ServoTableLabel('实时 PWM')),
      SizedBox(width: 94, child: _ServoTableLabel('最小')),
      SizedBox(width: 94, child: _ServoTableLabel('中位')),
      SizedBox(width: 94, child: _ServoTableLabel('最大')),
    ],
  );
}

class _ServoTableLabel extends StatelessWidget {
  const _ServoTableLabel(this.label);
  final String label;

  @override
  Widget build(BuildContext context) => Text(
    label,
    textAlign: TextAlign.center,
    style: MacGlassTypography.tableHeader,
  );
}

class _ServoChannelRow extends StatelessWidget {
  const _ServoChannelRow({
    required this.channel,
    required this.output,
    required this.parameters,
    required this.staged,
    required this.controller,
    required this.onWrite,
  });

  final int channel;
  final ValueListenable<int> output;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final ParameterController controller;
  final Future<void> Function(String name, double value) onWrite;

  String _name(String suffix) => 'SERVO${channel}_$suffix';

  double? _value(String suffix) {
    final name = _name(suffix);
    return staged[name]?.stagedValue ?? parameters[name]?.value;
  }

  bool get _hasPending => const [
    'FUNCTION',
    'REVERSED',
    'MIN',
    'TRIM',
    'MAX',
  ].map(_name).any(staged.containsKey);

  @override
  Widget build(BuildContext context) {
    final functionName = _name('FUNCTION');
    final function = parameters[functionName];
    final functionDescriptor = controller.descriptorFor(functionName);
    final functionOptions = _distinctParameterOptions(functionDescriptor);
    final reversedName = _name('REVERSED');
    final reversed = parameters[reversedName];
    final min = _value('MIN') ?? 800;
    final max = _value('MAX') ?? 2200;
    final available =
        function != null ||
        parameters.keys.any((name) => name.startsWith('SERVO${channel}_'));
    final enabled = controller.canEdit && available;

    return Opacity(
      key: ValueKey('mg-servo-channel-$channel'),
      opacity: available ? 1 : 0.48,
      child: Row(
        children: [
          SizedBox(
            width: 74,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  'S$channel',
                  style: const TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.w800,
                  ),
                ),
                if (_hasPending) ...[
                  const SizedBox(width: 5),
                  const Icon(
                    Icons.circle,
                    size: 7,
                    color: MacGlassTokens.amber,
                  ),
                ],
              ],
            ),
          ),
          Expanded(
            flex: 3,
            child: function != null && functionOptions.length > 1
                ? _MacParameterPicker(
                    key: ValueKey('mg-servo-$channel-function'),
                    value: (_value('FUNCTION') ?? 0).round(),
                    options: functionOptions,
                    enabled:
                        enabled && !(functionDescriptor?.readOnly ?? false),
                    label: '',
                    compact: true,
                    inset: true,
                    showValue: true,
                    onChanged: (value) =>
                        unawaited(onWrite(functionName, value.toDouble())),
                  )
                : const _ServoUnavailableCell('无功能元数据'),
          ),
          SizedBox(
            width: 80,
            child: Center(
              child: Transform.scale(
                scale: 0.76,
                child: cupertino.CupertinoSwitch(
                  key: ValueKey('mg-servo-$channel-reversed'),
                  value: (_value('REVERSED') ?? 0) != 0,
                  onChanged: enabled && reversed != null
                      ? (value) =>
                            unawaited(onWrite(reversedName, value ? 1 : 0))
                      : null,
                ),
              ),
            ),
          ),
          Expanded(
            child: RepaintBoundary(
              child: ValueListenableBuilder<int>(
                valueListenable: output,
                builder: (context, value, child) => _ServoOutputIndicator(
                  animationKey: ValueKey('mg-servo-$channel-output-animation'),
                  value: value,
                  minValue: min,
                  maxValue: max,
                ),
              ),
            ),
          ),
          SizedBox(
            width: 94,
            child: Center(
              child: _ServoNumberField(
                fieldKey: ValueKey('mg-servo-$channel-min'),
                value: _value('MIN'),
                descriptor: controller.descriptorFor(_name('MIN')),
                enabled: enabled && parameters.containsKey(_name('MIN')),
                onChanged: (value) => unawaited(onWrite(_name('MIN'), value)),
              ),
            ),
          ),
          SizedBox(
            width: 94,
            child: Center(
              child: _ServoNumberField(
                fieldKey: ValueKey('mg-servo-$channel-trim'),
                value: _value('TRIM'),
                descriptor: controller.descriptorFor(_name('TRIM')),
                enabled: enabled && parameters.containsKey(_name('TRIM')),
                onChanged: (value) => unawaited(onWrite(_name('TRIM'), value)),
              ),
            ),
          ),
          SizedBox(
            width: 94,
            child: Center(
              child: _ServoNumberField(
                fieldKey: ValueKey('mg-servo-$channel-max'),
                value: _value('MAX'),
                descriptor: controller.descriptorFor(_name('MAX')),
                enabled: enabled && parameters.containsKey(_name('MAX')),
                onChanged: (value) => unawaited(onWrite(_name('MAX'), value)),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ServoOutputIndicator extends StatelessWidget {
  const _ServoOutputIndicator({
    required this.animationKey,
    required this.value,
    required this.minValue,
    required this.maxValue,
  });
  final Key animationKey;
  final int value;
  final double minValue;
  final double maxValue;

  @override
  Widget build(BuildContext context) {
    final valid = value > 0;
    final span = maxValue - minValue;
    final progress = !valid || span <= 0
        ? 0.0
        : ((value - minValue) / span).clamp(0.0, 1.0);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 10),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          Text(
            valid ? '$value PWM' : '--',
            style: TextStyle(
              color: valid ? MacGlassTokens.ink : MacGlassTokens.muted,
              fontSize: 11,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 5),
          TweenAnimationBuilder<double>(
            key: animationKey,
            tween: Tween<double>(begin: 0, end: progress),
            duration: const Duration(milliseconds: 70),
            curve: Curves.linear,
            builder: (context, animatedProgress, child) => ClipRRect(
              borderRadius: BorderRadius.circular(2),
              child: LinearProgressIndicator(
                minHeight: 4,
                value: animatedProgress,
                color: MacGlassTokens.green,
                backgroundColor: MacGlassTokens.ink.withValues(alpha: 0.07),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ServoUnavailableCell extends StatelessWidget {
  const _ServoUnavailableCell(this.label);
  final String label;

  @override
  Widget build(BuildContext context) => Text(
    label,
    maxLines: 1,
    overflow: TextOverflow.ellipsis,
    style: const TextStyle(color: MacGlassTokens.muted, fontSize: 11),
  );
}

class _ServoNumberField extends StatefulWidget {
  const _ServoNumberField({
    required this.fieldKey,
    required this.value,
    required this.descriptor,
    required this.enabled,
    required this.onChanged,
  });
  final Key fieldKey;
  final double? value;
  final ParameterDescriptor? descriptor;
  final bool enabled;
  final ValueChanged<double> onChanged;

  @override
  State<_ServoNumberField> createState() => _ServoNumberFieldState();
}

class _ServoNumberFieldState extends State<_ServoNumberField> {
  late final TextEditingController _controller;
  late final FocusNode _focusNode;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(text: _textFor(widget.value));
    _focusNode = FocusNode()..addListener(_handleFocusChanged);
  }

  @override
  void didUpdateWidget(covariant _ServoNumberField oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!_focusNode.hasFocus && oldWidget.value != widget.value) {
      _controller.text = _textFor(widget.value);
    }
  }

  @override
  void dispose() {
    _focusNode.removeListener(_handleFocusChanged);
    _focusNode.dispose();
    _controller.dispose();
    super.dispose();
  }

  String _textFor(double? value) =>
      value == null ? '--' : _formatParameterNumber(value);

  void _handleFocusChanged() {
    if (!_focusNode.hasFocus) _commit();
  }

  void _commit() {
    if (!widget.enabled) return;
    final parsed = double.tryParse(_controller.text);
    if (parsed == null || !parsed.isFinite) {
      _controller.text = _textFor(widget.value);
      return;
    }
    final min = widget.descriptor?.minValue;
    final max = widget.descriptor?.maxValue;
    final updated = parsed.clamp(min ?? parsed, max ?? parsed).toDouble();
    _controller.text = _formatParameterNumber(updated);
    widget.onChanged(updated);
  }

  void _step(double direction) {
    if (!widget.enabled) return;
    final current = double.tryParse(_controller.text) ?? widget.value ?? 0;
    final increment = widget.descriptor?.increment ?? 1;
    _controller.text = _formatParameterNumber(current + direction * increment);
    _commit();
  }

  @override
  Widget build(BuildContext context) => SizedBox(
    width: 88,
    height: 44,
    child: DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: widget.enabled ? 0.62 : 0.28),
        border: Border.all(color: Colors.white.withValues(alpha: 0.9)),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          Expanded(
            child: cupertino.CupertinoTextField(
              key: widget.fieldKey,
              controller: _controller,
              focusNode: _focusNode,
              enabled: widget.enabled,
              textAlign: TextAlign.center,
              style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700),
              keyboardType: const TextInputType.numberWithOptions(),
              padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 11),
              decoration: const BoxDecoration(
                color: Color(0xfff2f2f7),
                borderRadius: BorderRadius.all(Radius.circular(7)),
              ),
              onSubmitted: (_) => _commit(),
            ),
          ),
          SizedBox(
            width: 22,
            child: Column(
              children: [
                Expanded(
                  child: InkWell(
                    onTap: widget.enabled ? () => _step(1) : null,
                    child: const Center(
                      child: Icon(Icons.keyboard_arrow_up_rounded, size: 14),
                    ),
                  ),
                ),
                Divider(
                  height: 1,
                  color: MacGlassTokens.ink.withValues(alpha: 0.08),
                ),
                Expanded(
                  child: InkWell(
                    onTap: widget.enabled ? () => _step(-1) : null,
                    child: const Center(
                      child: Icon(Icons.keyboard_arrow_down_rounded, size: 14),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    ),
  );
}
