part of 'backstage_page.dart';

class _SerialPortConfigPage extends StatefulWidget {
  const _SerialPortConfigPage({required this.controller});

  final ParameterController controller;

  @override
  State<_SerialPortConfigPage> createState() => _SerialPortConfigPageState();
}

class _SerialPortConfigPageState extends State<_SerialPortConfigPage> {
  final Map<int, SerialPortHardwareDescriptor> _hardware = {};
  bool _hardwareRequested = false;
  bool _hardwareLoading = false;
  bool _hardwareScheduled = false;
  String? _hardwareError;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_scheduleHardwareMapping);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _scheduleHardwareMapping();
    });
  }

  @override
  void didUpdateWidget(covariant _SerialPortConfigPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller == widget.controller) return;
    oldWidget.controller.removeListener(_scheduleHardwareMapping);
    widget.controller.addListener(_scheduleHardwareMapping);
    _hardware.clear();
    _hardwareRequested = false;
    _hardwareError = null;
    _scheduleHardwareMapping();
  }

  @override
  void dispose() {
    widget.controller.removeListener(_scheduleHardwareMapping);
    super.dispose();
  }

  void _scheduleHardwareMapping() {
    if (!mounted ||
        _hardwareRequested ||
        _hardwareLoading ||
        _hardwareScheduled ||
        widget.controller.parameters.isEmpty ||
        !widget.controller.canEdit) {
      return;
    }
    _hardwareScheduled = true;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _hardwareScheduled = false;
      if (mounted) _loadHardwareMapping();
    });
  }

  void _loadHardwareMapping() {
    if (!mounted ||
        _hardwareRequested ||
        _hardwareLoading ||
        widget.controller.parameters.isEmpty ||
        !widget.controller.canEdit) {
      return;
    }
    final backend = widget.controller.capabilities.serialHardware;
    if (backend == null) {
      _hardwareRequested = true;
      setState(() => _hardwareError = '当前连接后端不支持硬件串口映射');
      return;
    }
    _hardwareRequested = true;
    setState(() {
      _hardwareLoading = true;
      _hardwareError = null;
    });
    unawaited(
      backend
          .loadSerialPortHardware()
          .then((items) {
            if (!mounted) return;
            setState(() {
              _hardware
                ..clear()
                ..addEntries(
                  items.map((item) => MapEntry(item.serialIndex, item)),
                );
              _hardwareLoading = false;
              _hardwareError = items.isEmpty
                  ? '@SYS/uarts.txt 中没有可用的串口硬件映射'
                  : null;
            });
          })
          .catchError((Object error) {
            if (!mounted) return;
            final message = error.toString();
            setState(() {
              _hardwareLoading = false;
              _hardwareError = _friendlyHardwareError(message);
            });
          }),
    );
  }

  void _retryHardwareMapping() {
    if (_hardwareLoading) return;
    setState(() {
      _hardwareRequested = false;
      _hardwareError = null;
      _hardware.clear();
    });
    _scheduleHardwareMapping();
  }

  String _friendlyHardwareError(String message) {
    final normalized = message.startsWith('Bad state: ')
        ? message.substring('Bad state: '.length)
        : message;
    if (normalized.contains('FTP 错误码 10')) {
      return '飞控未提供 @SYS/uarts.txt（固件未启用 UART 统计，MAVLink FTP 错误码 10）';
    }
    if (normalized.contains('open timeout')) {
      return 'MAVLink FTP 打开超时（open timeout），请确认链路稳定并重试';
    }
    if (normalized.contains('read timeout')) {
      return 'MAVLink FTP 读取超时（read timeout），请确认链路稳定并重试';
    }
    return normalized;
  }

  Future<bool> _writeImmediately(String name, double value) async {
    final result = await widget.controller.writeValueImmediately(name, value);
    if (!mounted || result.success) return result.success;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
    return false;
  }

  Future<void> _writeProtocol(int port, int protocol) async {
    final prefix = 'SERIAL$port';
    if (!await _writeImmediately('${prefix}_PROTOCOL', protocol.toDouble())) {
      return;
    }

    // Mission Planner SerialOptionRules.json presets for MAVLink 1 and 2.
    final parameterNames = widget.controller.parameters
        .map((parameter) => parameter.name)
        .toSet();
    if (protocol == 1 && parameterNames.contains('${prefix}_BAUD')) {
      if (!await _writeImmediately('${prefix}_BAUD', 115)) return;
    }
    if ((protocol == 1 || protocol == 2) &&
        parameterNames.contains('${prefix}_OPTIONS')) {
      await _writeImmediately('${prefix}_OPTIONS', 0);
    }
  }

  @override
  Widget build(BuildContext context) {
    _scheduleHardwareMapping();
    final parameters = {
      for (final parameter in widget.controller.parameters)
        parameter.name: parameter,
    };
    final staged = {
      for (final change in widget.controller.stagedChanges)
        change.parameter.name: change,
    };
    final ports = _serialPortNumbers(widget.controller.parameters);
    return Column(
      children: [
        if (_hardwareLoading ||
            _hardwareError != null ||
            _hardware.isNotEmpty) ...[
          _BackstageFeatureCard(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 9, 10, 9),
              child: Row(
                children: [
                  const Icon(
                    Icons.memory_rounded,
                    size: 17,
                    color: MacGlassTokens.blue,
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      _serialHardwareStatus(
                        loading: _hardwareLoading,
                        error: _hardwareError,
                        count: _hardware.length,
                      ),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                        color: MacGlassTokens.muted,
                        fontSize: 11.5,
                      ),
                    ),
                  ),
                  IconButton(
                    key: const ValueKey('mg-serial-refresh-hardware'),
                    tooltip: '重新读取硬件接口',
                    onPressed: _hardwareLoading ? null : _retryHardwareMapping,
                    icon: const Icon(Icons.refresh_rounded, size: 17),
                    visualDensity: VisualDensity.compact,
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 12),
        ],
        Expanded(
          child: ports.isEmpty
              ? const Center(
                  child: Text(
                    '飞控未提供 SERIALn_BAUD 参数',
                    style: TextStyle(color: MacGlassTokens.muted),
                  ),
                )
              : ListView(
                  key: const ValueKey('mg-serial-port-list'),
                  padding: const EdgeInsets.only(bottom: 12),
                  children: [
                    _SerialPortConfigurationPanel(
                      ports: ports,
                      parameters: parameters,
                      staged: staged,
                      hardware: _hardware,
                      controller: widget.controller,
                      onWrite: _writeImmediately,
                      onProtocolChanged: _writeProtocol,
                    ),
                  ],
                ),
        ),
      ],
    );
  }

  String _serialHardwareStatus({
    required bool loading,
    required String? error,
    required int count,
  }) {
    if (loading) return '正在从飞控读取 @SYS/uarts.txt…';
    if (error != null && error.isNotEmpty) return '硬件接口读取失败：$error';
    if (count > 0) return '已读取 $count 个硬件接口 · 修改后立即写入飞控，重启后生效';
    return '硬件接口来自飞控 @SYS/uarts.txt；尚未读取到 UART/USART 映射';
  }
}

class _SerialPortConfigurationPanel extends StatelessWidget {
  const _SerialPortConfigurationPanel({
    required this.ports,
    required this.parameters,
    required this.staged,
    required this.hardware,
    required this.controller,
    required this.onWrite,
    required this.onProtocolChanged,
  });

  final List<int> ports;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final Map<int, SerialPortHardwareDescriptor> hardware;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;
  final Future<void> Function(int port, int protocol) onProtocolChanged;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth >= 760 ? 2 : 1;
        final spacing = columns == 2 ? 14.0 : 10.0;
        return GridView.builder(
          key: const ValueKey('mg-serial-port-grid'),
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          padding: const EdgeInsets.only(bottom: 12),
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: columns,
            crossAxisSpacing: spacing,
            mainAxisSpacing: spacing,
            // The protocol and baud-rate controls share one row. Keep the
            // card compact so the old second-row height is not left as a
            // blank spacer above the divider.
            mainAxisExtent: 176,
          ),
          itemCount: ports.length,
          itemBuilder: (context, index) {
            final port = ports[index];
            return _SerialPortRow(
              key: ValueKey('mg-serial-port-$port'),
              port: port,
              parameters: parameters,
              staged: staged,
              hardware: hardware[port],
              controller: controller,
              onWrite: onWrite,
              onProtocolChanged: onProtocolChanged,
            );
          },
        );
      },
    );
  }
}

class _SerialPortRow extends StatelessWidget {
  const _SerialPortRow({
    required this.port,
    required this.parameters,
    required this.staged,
    required this.hardware,
    required this.controller,
    required this.onWrite,
    required this.onProtocolChanged,
    super.key,
  });

  final int port;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final SerialPortHardwareDescriptor? hardware;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;
  final Future<void> Function(int port, int protocol) onProtocolChanged;

  String _name(String suffix) => 'SERIAL${port}_$suffix';

  double? _value(String suffix) {
    final name = _name(suffix);
    return staged[name]?.stagedValue ?? parameters[name]?.value;
  }

  @override
  Widget build(BuildContext context) {
    final baudName = _name('BAUD');
    final protocolName = _name('PROTOCOL');
    final optionsName = _name('OPTIONS');
    final baudOptions = _distinctParameterOptions(
      controller.descriptorFor(baudName),
    );
    final protocolOptions = _distinctParameterOptions(
      controller.descriptorFor(protocolName),
    );
    final options = _distinctParameterOptions(
      controller.descriptorFor(optionsName),
    ).where((option) => option.value != 0).toList(growable: false);
    final enabled = controller.canEdit;
    final baud = _value('BAUD')?.round();
    final protocol = _value('PROTOCOL')?.round();
    final optionValue = _value('OPTIONS')?.round();
    final flowControlName = 'BRD_SER${port}_RTSCTS';
    final flowControl =
        staged[flowControlName]?.stagedValue.round() ??
        parameters[flowControlName]?.value.round();
    final flowControlOptions = parameters.containsKey(flowControlName)
        ? const [
            ParameterOption(value: 0, label: 'CTS/RTS 关闭'),
            ParameterOption(value: 1, label: '启用'),
            ParameterOption(value: 2, label: '自动'),
          ]
        : const <ParameterOption>[];

    return _SerialPortCard(
      port: port,
      hardware: hardware,
      protocol: protocol,
      protocolOptions: protocolOptions,
      baud: baud,
      baudOptions: baudOptions,
      optionValue: optionValue,
      options: options,
      optionsName: optionsName,
      flowControl: flowControl,
      flowControlOptions: flowControlOptions,
      enabled: enabled,
      onProtocolChanged: protocol == null || protocolOptions.isEmpty
          ? null
          : (value) => unawaited(onProtocolChanged(port, value)),
      onBaudChanged: baud == null || baudOptions.isEmpty
          ? null
          : (value) => unawaited(onWrite(baudName, value.toDouble())),
      onOptionsChanged: optionValue == null || options.isEmpty
          ? null
          : (value) => unawaited(onWrite(optionsName, value.toDouble())),
      onFlowControlChanged: flowControl == null || flowControlOptions.isEmpty
          ? null
          : (value) => unawaited(onWrite(flowControlName, value.toDouble())),
    );
  }
}

class _SerialPortCard extends StatelessWidget {
  const _SerialPortCard({
    required this.port,
    required this.hardware,
    required this.protocol,
    required this.protocolOptions,
    required this.baud,
    required this.baudOptions,
    required this.optionValue,
    required this.options,
    required this.optionsName,
    required this.flowControl,
    required this.flowControlOptions,
    required this.enabled,
    required this.onProtocolChanged,
    required this.onBaudChanged,
    required this.onOptionsChanged,
    required this.onFlowControlChanged,
  });

  final int port;
  final SerialPortHardwareDescriptor? hardware;
  final int? protocol;
  final List<ParameterOption> protocolOptions;
  final int? baud;
  final List<ParameterOption> baudOptions;
  final int? optionValue;
  final List<ParameterOption> options;
  final String optionsName;
  final int? flowControl;
  final List<ParameterOption> flowControlOptions;
  final bool enabled;
  final ValueChanged<int>? onProtocolChanged;
  final ValueChanged<int>? onBaudChanged;
  final ValueChanged<int>? onOptionsChanged;
  final ValueChanged<int>? onFlowControlChanged;

  @override
  Widget build(BuildContext context) {
    final hardwareName = hardware?.name.trim();
    final hardwareLabel = hardwareName == null || hardwareName.isEmpty
        ? '未映射'
        : hardwareName;
    return ClipRRect(
      borderRadius: BorderRadius.circular(18),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.48),
            border: Border.all(color: Colors.white.withValues(alpha: 0.78)),
            borderRadius: BorderRadius.circular(18),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Padding(
            padding: const EdgeInsets.fromLTRB(18, 16, 18, 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Text(
                      'SERIAL$port',
                      style: const TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.w800,
                        letterSpacing: 0.2,
                      ),
                    ),
                    const SizedBox(width: 10),
                    DecoratedBox(
                      decoration: BoxDecoration(
                        color: MacGlassTokens.blue.withValues(alpha: 0.13),
                        borderRadius: BorderRadius.circular(14),
                      ),
                      child: Padding(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 10,
                          vertical: 5,
                        ),
                        child: Text(
                          hardwareLabel,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: TextStyle(
                            color: hardwareName == null || hardwareName.isEmpty
                                ? MacGlassTokens.muted
                                : MacGlassTokens.blue,
                            fontSize: 13,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    Expanded(
                      child: Row(
                        children: [
                          const _SerialFieldLabel('协议'),
                          const SizedBox(width: 10),
                          Expanded(
                            child:
                                protocol != null && protocolOptions.isNotEmpty
                                ? _MacParameterPicker(
                                    key: ValueKey('mg-serial-$port-protocol'),
                                    value: protocol!,
                                    options: protocolOptions,
                                    enabled: enabled,
                                    label: '',
                                    compact: true,
                                    inset: true,
                                    showValue: false,
                                    onChanged: onProtocolChanged ?? (_) {},
                                  )
                                : const _ServoUnavailableCell('无协议元数据'),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 20),
                    Expanded(
                      child: Row(
                        children: [
                          const _SerialFieldLabel('波特率'),
                          const SizedBox(width: 10),
                          Expanded(
                            child: baud != null && baudOptions.isNotEmpty
                                ? _MacParameterPicker(
                                    key: ValueKey('mg-serial-$port-baud'),
                                    value: baud!,
                                    options: baudOptions,
                                    enabled: enabled,
                                    label: '',
                                    compact: true,
                                    inset: true,
                                    centered: true,
                                    showValue: false,
                                    onChanged: onBaudChanged ?? (_) {},
                                  )
                                : const _ServoUnavailableCell('无波特率元数据'),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                Divider(
                  height: 18,
                  color: MacGlassTokens.ink.withValues(alpha: 0.08),
                ),
                Row(
                  children: [
                    if (optionValue != null && options.isNotEmpty)
                      SizedBox(
                        width: 126,
                        child: _SerialOptionsPicker(
                          key: ValueKey('mg-serial-$port-options'),
                          parameterName: optionsName,
                          value: optionValue!,
                          options: options,
                          enabled: enabled,
                          onChanged: onOptionsChanged ?? (_) {},
                        ),
                      )
                    else
                      const _SerialOptionsButton(
                        label: 'Options 0',
                        enabled: false,
                      ),
                    if (flowControl != null &&
                        flowControlOptions.isNotEmpty) ...[
                      const Spacer(),
                      const Text('CTS/RTS', style: MacGlassTypography.caption),
                      const SizedBox(width: 7),
                      SizedBox(
                        width: 92,
                        child: _SerialFlowControlPicker(
                          pickerKey: ValueKey('mg-serial-$port-flow-control'),
                          value: flowControl,
                          options: flowControlOptions,
                          enabled: enabled,
                          onChanged: onFlowControlChanged ?? (_) {},
                        ),
                      ),
                    ],
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _SerialFieldLabel extends StatelessWidget {
  const _SerialFieldLabel(this.label);

  final String label;

  @override
  Widget build(BuildContext context) => SizedBox(
    width: 44,
    child: Text(
      label,
      style: const TextStyle(fontSize: 12.5, fontWeight: FontWeight.w700),
    ),
  );
}

class _SerialOptionsButton extends StatelessWidget {
  const _SerialOptionsButton({required this.label, required this.enabled});

  final String label;
  final bool enabled;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: MacGlassTokens.ink.withValues(alpha: enabled ? 0.045 : 0.025),
      border: Border.all(
        color: MacGlassTokens.ink.withValues(alpha: enabled ? 0.075 : 0.04),
      ),
      borderRadius: BorderRadius.circular(14),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 8),
      child: Text(
        label,
        style: TextStyle(
          color: enabled ? MacGlassTokens.ink : MacGlassTokens.muted,
          fontSize: 12.5,
          fontWeight: FontWeight.w700,
        ),
      ),
    ),
  );
}

class _SerialFlowControlPicker extends StatelessWidget {
  const _SerialFlowControlPicker({
    required this.pickerKey,
    required this.value,
    required this.options,
    required this.enabled,
    required this.onChanged,
  });

  final Key pickerKey;
  final int? value;
  final List<ParameterOption> options;
  final bool enabled;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    if (value == null || options.isEmpty) return const SizedBox.shrink();
    return _MacParameterPicker(
      key: pickerKey,
      value: value!,
      options: options,
      enabled: enabled,
      label: '',
      compact: true,
      inset: true,
      centered: true,
      showValue: false,
      onChanged: onChanged,
    );
  }
}

class _SerialOptionsPicker extends StatelessWidget {
  const _SerialOptionsPicker({
    required this.parameterName,
    required this.value,
    required this.options,
    required this.enabled,
    required this.onChanged,
    super.key,
  });

  final String parameterName;
  final int value;
  final List<ParameterOption> options;
  final bool enabled;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    final label = 'Options $value';
    final menuHeight = (options.length * 42.0).clamp(42.0, 360.0);
    return MenuAnchor(
      style: MacGlassMenu.style(minimumWidth: 268),
      alignmentOffset: const Offset(0, 4),
      menuChildren: [
        SizedBox(
          width: 268,
          height: menuHeight,
          child: ListView(
            primary: false,
            padding: EdgeInsets.zero,
            children: [
              for (final option in options)
                SizedBox(
                  height: 42,
                  child: MenuItemButton(
                    key: ValueKey(
                      'mg-serial-option-$parameterName-${option.value}',
                    ),
                    closeOnActivate: false,
                    onPressed: enabled
                        ? () {
                            final checked = (value & option.value) != 0;
                            onChanged(
                              checked
                                  ? value & ~option.value
                                  : value | option.value,
                            );
                          }
                        : null,
                    style: MacGlassMenu.itemStyle(
                      selected: (value & option.value) != 0,
                    ),
                    child: Row(
                      children: [
                        Icon(
                          (value & option.value) != 0
                              ? Icons.check_box_rounded
                              : Icons.check_box_outline_blank_rounded,
                          size: 17,
                          color: (value & option.value) != 0
                              ? MacGlassTokens.green
                              : MacGlassTokens.muted,
                        ),
                        const SizedBox(width: 9),
                        Expanded(
                          child: Text(
                            option.label,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                              fontSize: 12.5,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
            ],
          ),
        ),
      ],
      builder: (context, menuController, child) => DecoratedBox(
        decoration: BoxDecoration(
          color: MacGlassTokens.ink.withValues(alpha: enabled ? 0.045 : 0.025),
          border: Border.all(
            color: MacGlassTokens.ink.withValues(alpha: enabled ? 0.075 : 0.04),
          ),
          borderRadius: BorderRadius.circular(10),
        ),
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            borderRadius: BorderRadius.circular(10),
            onTap: !enabled
                ? null
                : () => menuController.isOpen
                      ? menuController.close()
                      : menuController.open(),
            child: SizedBox(
              height: 38,
              child: Padding(
                padding: const EdgeInsets.fromLTRB(10, 3, 5, 3),
                child: Row(
                  children: [
                    Expanded(
                      child: Text(
                        label,
                        textAlign: TextAlign.center,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          color: enabled
                              ? MacGlassTokens.ink
                              : MacGlassTokens.muted,
                          fontSize: 13,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                    const SizedBox.square(
                      dimension: 22,
                      child: Icon(Icons.checklist_rounded, size: 17),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
