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
    final backend = widget.controller.backend;
    if (backend is! SerialHardwareSessionBackend) {
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
      (backend as SerialHardwareSessionBackend)
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
    final showHardware = hardware.isNotEmpty;
    return _BackstageFeatureCard(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(18, 16, 18, 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Text('串口配置', style: MacGlassTypography.sectionTitle),
            const SizedBox(height: 14),
            LayoutBuilder(
              builder: (context, constraints) {
                final metrics = _SerialTableMetrics(showHardware: showHardware);
                return SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: SizedBox(
                    width: metrics.totalWidth,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        _SerialPortTableHeader(metrics: metrics),
                        const Divider(height: 18),
                        for (var index = 0; index < ports.length; index++) ...[
                          _SerialPortRow(
                            key: ValueKey('mg-serial-port-${ports[index]}'),
                            port: ports[index],
                            parameters: parameters,
                            staged: staged,
                            hardware: hardware[ports[index]],
                            metrics: metrics,
                            controller: controller,
                            onWrite: onWrite,
                            onProtocolChanged: onProtocolChanged,
                          ),
                          if (index < ports.length - 1)
                            const Divider(height: 14),
                        ],
                      ],
                    ),
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _SerialTableMetrics {
  _SerialTableMetrics({required this.showHardware})
    : port = 104,
      hardware = showHardware ? 150 : 0,
      protocol = 180,
      baud = 150,
      flow = 148,
      options = 196;

  final bool showHardware;
  final double port;
  final double hardware;
  final double protocol;
  final double baud;
  final double flow;
  final double options;

  double get totalWidth => port + hardware + protocol + baud + flow + options;
}

class _SerialPortTableHeader extends StatelessWidget {
  const _SerialPortTableHeader({required this.metrics});

  final _SerialTableMetrics metrics;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      SizedBox(width: metrics.port, child: const _SerialTableLabel('端口')),
      if (metrics.showHardware)
        SizedBox(
          width: metrics.hardware,
          child: const _SerialTableLabel('硬件接口'),
        ),
      SizedBox(width: metrics.protocol, child: const _SerialTableLabel('协议')),
      SizedBox(width: metrics.baud, child: const _SerialTableLabel('波特率')),
      SizedBox(
        width: metrics.flow,
        child: const _SerialTableLabel('CTS / RTS'),
      ),
      SizedBox(width: metrics.options, child: const _SerialTableLabel('线路选项')),
    ],
  );
}

class _SerialTableLabel extends StatelessWidget {
  const _SerialTableLabel(this.label);
  final String label;

  @override
  Widget build(BuildContext context) => Text(
    label,
    textAlign: TextAlign.center,
    style: MacGlassTypography.tableHeader,
  );
}

class _SerialPortRow extends StatelessWidget {
  const _SerialPortRow({
    required this.port,
    required this.parameters,
    required this.staged,
    required this.hardware,
    required this.metrics,
    required this.controller,
    required this.onWrite,
    required this.onProtocolChanged,
    super.key,
  });

  final int port;
  final Map<String, FlightControllerParameter> parameters;
  final Map<String, StagedParameterChange> staged;
  final SerialPortHardwareDescriptor? hardware;
  final _SerialTableMetrics metrics;
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

    return Row(
      children: [
        SizedBox(
          width: metrics.port,
          child: Text(
            'SERIAL$port',
            textAlign: TextAlign.center,
            style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w800),
          ),
        ),
        if (metrics.showHardware)
          SizedBox(
            width: metrics.hardware,
            child: _SerialHardwareLabel(hardware: hardware),
          ),
        SizedBox(
          width: metrics.protocol,
          child: Padding(
            padding: const EdgeInsets.only(right: 12),
            child: protocol != null && protocolOptions.isNotEmpty
                ? _MacParameterPicker(
                    key: ValueKey('mg-serial-$port-protocol'),
                    value: protocol,
                    options: protocolOptions,
                    enabled: enabled,
                    label: '',
                    compact: true,
                    inset: true,
                    showValue: false,
                    onChanged: (value) =>
                        unawaited(onProtocolChanged(port, value)),
                  )
                : const _ServoUnavailableCell('无协议元数据'),
          ),
        ),
        SizedBox(
          width: metrics.baud,
          child: Padding(
            padding: const EdgeInsets.only(right: 12),
            child: baud != null && baudOptions.isNotEmpty
                ? _MacParameterPicker(
                    key: ValueKey('mg-serial-$port-baud'),
                    value: baud,
                    options: baudOptions,
                    enabled: enabled,
                    label: '',
                    compact: true,
                    inset: true,
                    centered: true,
                    showValue: false,
                    onChanged: (value) =>
                        unawaited(onWrite(baudName, value.toDouble())),
                  )
                : const _ServoUnavailableCell('无波特率元数据'),
          ),
        ),
        SizedBox(
          width: metrics.flow,
          child: Padding(
            padding: const EdgeInsets.only(right: 12),
            child: _SerialFlowControlPicker(
              pickerKey: ValueKey('mg-serial-$port-flow-control'),
              value: flowControl,
              options: flowControlOptions,
              enabled: enabled,
              onChanged: (value) =>
                  unawaited(onWrite(flowControlName, value.toDouble())),
            ),
          ),
        ),
        SizedBox(
          width: metrics.options,
          child: Padding(
            padding: const EdgeInsets.only(right: 12),
            child: optionValue != null && options.isNotEmpty
                ? _SerialOptionsPicker(
                    key: ValueKey('mg-serial-$port-options'),
                    parameterName: optionsName,
                    value: optionValue,
                    options: options,
                    enabled: enabled,
                    onChanged: (value) =>
                        unawaited(onWrite(optionsName, value.toDouble())),
                  )
                : const _ServoUnavailableCell('此端口无线路选项'),
          ),
        ),
      ],
    );
  }
}

class _SerialHardwareLabel extends StatelessWidget {
  const _SerialHardwareLabel({required this.hardware});

  final SerialPortHardwareDescriptor? hardware;

  @override
  Widget build(BuildContext context) {
    if (hardware == null) return const SizedBox.shrink();
    return Text(
      hardware!.name,
      textAlign: TextAlign.center,
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700),
    );
  }
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
    final selected = options
        .where((option) => (value & option.value) != 0)
        .map((option) => option.label)
        .toList(growable: false);
    final label = selected.isEmpty ? '无附加选项' : selected.join(' / ');
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
