part of 'backstage_page.dart';

class _AirframePage extends StatefulWidget {
  const _AirframePage({
    required this.controller,
    required this.airframe,
    required this.firmwareType,
  });

  final ParameterController controller;
  final String? airframe;
  final String? firmwareType;

  @override
  State<_AirframePage> createState() => _AirframePageState();
}

class _AirframePageState extends State<_AirframePage> {
  final Set<String> _requestedDescriptors = <String>{};

  ParameterController get controller => widget.controller;

  @override
  void initState() {
    super.initState();
    _requestDescriptors();
  }

  @override
  void didUpdateWidget(covariant _AirframePage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      _requestedDescriptors.clear();
      _requestDescriptors();
    }
  }

  void _requestDescriptors() {
    for (final name in const [
      'FRAME_CLASS',
      'FRAME_TYPE',
      'Q_FRAME_CLASS',
      'Q_FRAME_TYPE',
      'FRAME',
    ]) {
      if (controller.parameters.any((parameter) => parameter.name == name) &&
          _requestedDescriptors.add(name)) {
        unawaited(controller.loadDescriptor(name));
      }
    }
  }

  FlightControllerParameter? _parameter(String name) => controller.parameters
      .where((parameter) => parameter.name == name)
      .firstOrNull;

  Future<void> _write(String name, double value) async {
    final result = await controller.writeValueImmediately(name, value);
    if (!result.success && mounted) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
    }
  }

  @override
  Widget build(BuildContext context) {
    _requestDescriptors();
    final frameClass = _parameter('FRAME_CLASS');
    final frameType = _parameter('FRAME_TYPE');
    final quadPlaneClass = _parameter('Q_FRAME_CLASS');
    final quadPlaneType = _parameter('Q_FRAME_TYPE');
    final legacyFrame = _parameter('FRAME');
    final detectedAirframe = widget.airframe;
    final copterDetected = detectedAirframe == 'Copter';
    final planeDetected =
        detectedAirframe == 'Plane' || detectedAirframe == 'QPlane';
    final airframeKnown =
        copterDetected ||
        detectedAirframe == 'Plane' ||
        detectedAirframe == 'QPlane';
    final hasCopterFrameParameters = frameClass != null && frameType != null;
    final hasQuadPlaneFrameParameters =
        quadPlaneClass != null && quadPlaneType != null;

    if ((copterDetected || (!airframeKnown && hasCopterFrameParameters)) &&
        hasCopterFrameParameters) {
      return _buildFrameEditor(
        title: 'Copter 机架',
        subtitle: 'FRAME_CLASS / FRAME_TYPE · 修改后立即写入飞控，重启后生效',
        classParameter: frameClass,
        typeParameter: frameType,
      );
    }
    if ((detectedAirframe == 'QPlane' ||
            (detectedAirframe == 'Plane' && hasQuadPlaneFrameParameters) ||
            (!airframeKnown &&
                !hasCopterFrameParameters &&
                hasQuadPlaneFrameParameters)) &&
        hasQuadPlaneFrameParameters) {
      return _buildFrameEditor(
        title: 'Plane QuadPlane 机架',
        subtitle: 'Q_FRAME_CLASS / Q_FRAME_TYPE · 仅适用于带 VTOL 电机的 Plane',
        classParameter: quadPlaneClass,
        typeParameter: quadPlaneType,
      );
    }
    if (!planeDetected && legacyFrame != null) {
      return _buildLegacyEditor(legacyFrame);
    }

    final detected = widget.firmwareType == 'ArduPilot'
        ? (widget.airframe ?? 'ArduPilot')
        : (widget.airframe ?? '当前飞控');
    return _AirframeNotice(
      title: '$detected 没有可编辑的电机机架参数',
      message: widget.airframe == 'Plane'
          ? '普通固定翼由舵面、发动机和空速相关参数定义，不使用 Copter 的 FRAME_CLASS/FRAME_TYPE。若这是 QuadPlane，请先确认参数列表中存在 Q_FRAME_CLASS 和 Q_FRAME_TYPE。'
          : '当前飞控尚未提供 FRAME_CLASS/FRAME_TYPE 或 Q_FRAME_CLASS/Q_FRAME_TYPE。请先完成参数同步，或在参数配置页面确认参数是否存在。',
    );
  }

  Widget _buildFrameEditor({
    required String title,
    required String subtitle,
    required FlightControllerParameter classParameter,
    required FlightControllerParameter typeParameter,
  }) {
    final classOptions = _optionsForClass(classParameter.name);
    final classValue = classParameter.value.round();
    final typeValue = typeParameter.value.round();
    final typeOptions = _optionsForType(
      typeParameter.name,
      classValue,
      classParameter.name.startsWith('Q_'),
      typeValue,
    );
    final classLabel = _labelFor(classParameter.name, classValue);
    final typeLabel = _labelFor(typeParameter.name, typeValue);
    final enabled = controller.canEdit;

    return _AirframeEditorFrame(
      title: title,
      subtitle: subtitle,
      className: classParameter.name,
      classLabel: classLabel,
      typeName: typeParameter.name,
      typeLabel: typeLabel,
      classOptions: classOptions,
      typeOptions: typeOptions,
      classValue: classValue,
      typeValue: typeValue,
      enabled: enabled,
      onClassChanged: (value) => _write(classParameter.name, value.toDouble()),
      onTypeChanged: (value) => _write(typeParameter.name, value.toDouble()),
    );
  }

  Widget _buildLegacyEditor(FlightControllerParameter parameter) {
    final value = parameter.value.round();
    final options = const [
      ParameterOption(value: 0, label: 'Plus'),
      ParameterOption(value: 1, label: 'X'),
      ParameterOption(value: 2, label: 'V'),
      ParameterOption(value: 3, label: 'H'),
      ParameterOption(value: 4, label: 'V-Tail'),
      ParameterOption(value: 5, label: 'Y6'),
    ];
    return _AirframeEditorFrame(
      title: '旧版 Copter 机架',
      subtitle: 'FRAME · 兼容旧版固件，修改后立即写入飞控，重启后生效',
      className: 'FRAME',
      classLabel: 'Frame Type',
      typeName: '',
      typeLabel: _labelFromOptions(options, value),
      classOptions: options,
      typeOptions: const [],
      classValue: value,
      typeValue: null,
      enabled: controller.canEdit,
      onClassChanged: (next) => _write(parameter.name, next.toDouble()),
      onTypeChanged: null,
    );
  }

  List<ParameterOption> _optionsForClass(String name) {
    final fromMetadata = controller.descriptorFor(name)?.options ?? const [];
    if (fromMetadata.isNotEmpty) return fromMetadata;
    return const [
      ParameterOption(value: 0, label: 'Undefined'),
      ParameterOption(value: 1, label: 'Quad'),
      ParameterOption(value: 2, label: 'Hexa'),
      ParameterOption(value: 3, label: 'Octa'),
      ParameterOption(value: 4, label: 'OctaQuad'),
      ParameterOption(value: 5, label: 'Y6'),
      ParameterOption(value: 6, label: 'Heli'),
      ParameterOption(value: 7, label: 'Tri'),
      ParameterOption(value: 8, label: 'SingleCopter'),
      ParameterOption(value: 9, label: 'CoaxCopter'),
      ParameterOption(value: 10, label: 'BiCopter'),
      ParameterOption(value: 11, label: 'Heli_Dual'),
      ParameterOption(value: 12, label: 'DodecaHexa'),
      ParameterOption(value: 13, label: 'HeliQuad'),
      ParameterOption(value: 14, label: 'Deca'),
      ParameterOption(value: 15, label: 'Scripting Matrix'),
      ParameterOption(value: 16, label: '6DoF Scripting'),
      ParameterOption(value: 17, label: 'Dynamic Scripting Matrix'),
    ];
  }

  List<ParameterOption> _optionsForType(
    String name,
    int classValue,
    bool quadPlane,
    int typeValue,
  ) {
    final fromMetadata = controller.descriptorFor(name)?.options ?? const [];
    final options = fromMetadata.isNotEmpty ? fromMetadata : _fallbackTypes;
    final validTypes = _validTypesForClass(classValue, quadPlane);
    final filtered = options
        .where(
          (option) => validTypes == null || validTypes.contains(option.value),
        )
        .toList(growable: false);
    if (filtered.any((option) => option.value == typeValue)) {
      return filtered;
    }
    return [
      ...filtered,
      ParameterOption(
        value: typeValue,
        label: _labelFromOptions(options, typeValue),
      ),
    ];
  }

  Set<int>? _validTypesForClass(int value, bool quadPlane) {
    if (value == 1) return const {0, 1, 2, 3, 4, 5};
    if (value == 2) return const {0, 1};
    if (value == 3 || value == 4) return const {0, 1, 2, 3};
    if (value == 5) return const {1, 10};
    if (value == 12) return const {0, 1};
    if (quadPlane && value == 10) return null;
    return <int>{};
  }

  String _labelFor(String name, int value) {
    final descriptor = controller.descriptorFor(name);
    return _labelFromOptions(descriptor?.options ?? const [], value);
  }

  String _labelFromOptions(List<ParameterOption> options, int value) {
    return options
            .where((option) => option.value == value)
            .firstOrNull
            ?.label ??
        '值 $value';
  }
}

const _fallbackTypes = [
  ParameterOption(value: 0, label: 'Plus'),
  ParameterOption(value: 1, label: 'X'),
  ParameterOption(value: 2, label: 'V'),
  ParameterOption(value: 3, label: 'H'),
  ParameterOption(value: 4, label: 'V-Tail'),
  ParameterOption(value: 5, label: 'A-Tail'),
  ParameterOption(value: 10, label: 'Y6B'),
  ParameterOption(value: 11, label: 'Y6F'),
  ParameterOption(value: 12, label: 'BetaFlightX'),
  ParameterOption(value: 13, label: 'DJIX'),
  ParameterOption(value: 14, label: 'ClockwiseX'),
  ParameterOption(value: 15, label: 'I'),
  ParameterOption(value: 16, label: 'NYT Plus'),
  ParameterOption(value: 17, label: 'NYT X'),
  ParameterOption(value: 18, label: 'BetaFlightXReversed'),
  ParameterOption(value: 19, label: 'Y4'),
];

class _AirframeEditorFrame extends StatelessWidget {
  const _AirframeEditorFrame({
    required this.title,
    required this.subtitle,
    required this.className,
    required this.classLabel,
    required this.typeName,
    required this.typeLabel,
    required this.classOptions,
    required this.typeOptions,
    required this.classValue,
    required this.typeValue,
    required this.enabled,
    required this.onClassChanged,
    required this.onTypeChanged,
  });

  final String title;
  final String subtitle;
  final String className;
  final String classLabel;
  final String typeName;
  final String typeLabel;
  final List<ParameterOption> classOptions;
  final List<ParameterOption> typeOptions;
  final int classValue;
  final int? typeValue;
  final bool enabled;
  final ValueChanged<int> onClassChanged;
  final ValueChanged<int>? onTypeChanged;

  @override
  Widget build(BuildContext context) => _GlassPanel(
    child: ListView(
      padding: const EdgeInsets.all(18),
      children: [
        Text(
          title,
          style: const TextStyle(fontSize: 17, fontWeight: FontWeight.w700),
        ),
        const SizedBox(height: 5),
        Text(
          subtitle,
          style: const TextStyle(color: MacGlassTokens.muted, fontSize: 11.5),
        ),
        const SizedBox(height: 18),
        _AirframeDropdownRow(
          title: '机架大类',
          parameterName: className,
          currentLabel: classLabel,
          options: classOptions,
          value: classValue,
          enabled: enabled,
          onChanged: onClassChanged,
        ),
        if (typeOptions.isNotEmpty) ...[
          const SizedBox(height: 12),
          _AirframeDropdownRow(
            title: '机架类型',
            parameterName: typeName,
            currentLabel: typeLabel,
            options: typeOptions,
            value: typeValue,
            enabled: enabled,
            onChanged: onTypeChanged,
          ),
        ],
        const SizedBox(height: 18),
        Text(
          typeOptions.isEmpty
              ? '当前参数只有一个机架类型值。'
              : '机架参数由飞控元数据提供。更换机架后通常需要重启飞控，并在拆除桨叶后检查电机输出顺序。',
          style: const TextStyle(color: MacGlassTokens.muted, fontSize: 11.5),
        ),
      ],
    ),
  );
}

class _AirframeDropdownRow extends StatelessWidget {
  const _AirframeDropdownRow({
    required this.title,
    required this.parameterName,
    required this.currentLabel,
    required this.options,
    required this.value,
    required this.enabled,
    required this.onChanged,
  });

  final String title;
  final String parameterName;
  final String currentLabel;
  final List<ParameterOption> options;
  final int? value;
  final bool enabled;
  final ValueChanged<int>? onChanged;

  @override
  Widget build(BuildContext context) => Row(
    crossAxisAlignment: CrossAxisAlignment.center,
    children: [
      Expanded(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: const TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(height: 3),
            Text(
              parameterName,
              style: const TextStyle(color: MacGlassTokens.muted, fontSize: 11),
            ),
          ],
        ),
      ),
      const SizedBox(width: 12),
      SizedBox(
        width: 250,
        child: DropdownButtonFormField<int>(
          key: ValueKey('$parameterName-$value'),
          initialValue: options.any((option) => option.value == value)
              ? value
              : null,
          isExpanded: true,
          decoration: InputDecoration(
            isDense: true,
            filled: true,
            fillColor: MacGlassTokens.ink.withValues(alpha: 0.045),
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(7),
              borderSide: BorderSide.none,
            ),
          ),
          hint: Text(currentLabel, overflow: TextOverflow.ellipsis),
          items: [
            for (final option in options)
              DropdownMenuItem<int>(
                value: option.value,
                child: Text(option.label, overflow: TextOverflow.ellipsis),
              ),
          ],
          onChanged: enabled
              ? (next) {
                  if (next != null) onChanged?.call(next);
                }
              : null,
        ),
      ),
    ],
  );
}

class _AirframeNotice extends StatelessWidget {
  const _AirframeNotice({required this.title, required this.message});

  final String title;
  final String message;

  @override
  Widget build(BuildContext context) => _GlassPanel(
    child: Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 620),
        child: Padding(
          padding: const EdgeInsets.all(28),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                Icons.airplanemode_active_rounded,
                size: 38,
                color: MacGlassTokens.muted,
              ),
              const SizedBox(height: 14),
              Text(
                title,
                textAlign: TextAlign.center,
                style: const TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.w700,
                ),
              ),
              const SizedBox(height: 8),
              Text(
                message,
                textAlign: TextAlign.center,
                style: const TextStyle(
                  color: MacGlassTokens.muted,
                  fontSize: 12,
                  height: 1.45,
                ),
              ),
            ],
          ),
        ),
      ),
    ),
  );
}
