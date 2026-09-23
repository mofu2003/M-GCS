part of 'backstage_page.dart';

class _PidTuningPage extends StatefulWidget {
  const _PidTuningPage({required this.controller, required this.airframe});

  final ParameterController controller;
  final String? airframe;

  @override
  State<_PidTuningPage> createState() => _PidTuningPageState();
}

class _PidTuningPageState extends State<_PidTuningPage> {
  int _selectedTab = 0;

  Future<bool> _write(String name, double value) async {
    final result = await widget.controller.writeValueImmediately(name, value);
    if (!result.success && mounted) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('$name 写入失败：${result.message}')));
    }
    return result.success;
  }

  @override
  Widget build(BuildContext context) {
    final profiles = _pidProfilesFor(
      airframe: widget.airframe,
      parameters: widget.controller.parameters,
    );
    final selectedTab = _selectedTab.clamp(0, profiles.length - 1);
    final profile = profiles[selectedTab];
    return _GlassPanel(
      child: Column(
        children: [
          if (profiles.length > 1) ...[
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 10),
              child: Align(
                alignment: Alignment.center,
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 460),
                  child: SizedBox(
                    width: double.infinity,
                    height: 34,
                    child: cupertino.CupertinoSlidingSegmentedControl<int>(
                      groupValue: selectedTab,
                      onValueChanged: (tab) {
                        if (tab != null && tab != selectedTab) {
                          setState(() => _selectedTab = tab);
                        }
                      },
                      padding: const EdgeInsets.all(2),
                      backgroundColor: MacGlassTokens.ink.withValues(
                        alpha: 0.055,
                      ),
                      thumbColor: Colors.white.withValues(alpha: 0.94),
                      children: {
                        for (var index = 0; index < profiles.length; index++)
                          index: _PidTabLabel(label: profiles[index].tabLabel),
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
          ],
          Expanded(
            child: _PidParameterPane(
              pageKey: ValueKey('mg-pid-${profile.key}'),
              controller: widget.controller,
              profile: profile,
              onWrite: _write,
            ),
          ),
        ],
      ),
    );
  }
}

class _PidTabLabel extends StatelessWidget {
  const _PidTabLabel({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => Center(
    child: Text(
      label,
      style: const TextStyle(
        color: MacGlassTokens.ink,
        fontSize: 11.5,
        fontWeight: FontWeight.w600,
      ),
    ),
  );
}

class _PidParameterPane extends StatefulWidget {
  const _PidParameterPane({
    required this.pageKey,
    required this.controller,
    required this.profile,
    required this.onWrite,
  });

  final Key pageKey;
  final ParameterController controller;
  final _PidProfileDefinition profile;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  State<_PidParameterPane> createState() => _PidParameterPaneState();
}

class _PidParameterPaneState extends State<_PidParameterPane> {
  bool _lockRollPitch = false;

  @override
  Widget build(BuildContext context) {
    final parameters =
        widget.controller.parameters
            .where(
              (parameter) =>
                  widget.profile.prefixes.any(parameter.name.startsWith),
            )
            .where(
              (parameter) =>
                  _isPidRelatedParameter(parameter, widget.profile.kind),
            )
            .toList()
          ..sort((a, b) => a.name.compareTo(b.name));
    if (widget.profile.kind == _PidProfile.copter) {
      return _CalibrationPane(
        pageKey: widget.pageKey,
        children: [
          const _CopterPidIntro(),
          const SizedBox(height: 14),
          if (parameters.isEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 56),
              child: Center(
                child: Text(
                  widget.profile.emptyMessage,
                  style: const TextStyle(color: MacGlassTokens.muted),
                ),
              ),
            )
          else
            _CopterPidTuningView(
              parameters: parameters,
              controller: widget.controller,
              onWrite: widget.onWrite,
            ),
        ],
      );
    }
    final groups = _pidGroupsFor(widget.profile.kind);
    final remaining = List<FlightControllerParameter>.of(parameters);
    final categorized = <_PidCategorizedGroup>[];
    for (final group in groups) {
      final items = remaining.where(group.matches).toList();
      remaining.removeWhere(group.matches);
      if (items.isNotEmpty) {
        categorized.add(_PidCategorizedGroup(group: group, parameters: items));
      }
    }
    final categorizedGroups = [
      ...categorized,
      if (remaining.isNotEmpty)
        _PidCategorizedGroup(
          group: const _PidGroup(
            title: '其他 PID 相关参数',
            subtitle: '当前固件提供、但未归入标准控制链路的高级参数。',
            matches: _matchesEverything,
          ),
          parameters: remaining,
        ),
    ];
    final matrix = _pidMatrixFor(categorizedGroups);
    return _CalibrationPane(
      pageKey: widget.pageKey,
      children: [
        _CalibrationSectionHeader(
          title: widget.profile.title,
          subtitle: widget.profile.subtitle,
        ),
        const SizedBox(height: 12),
        if (parameters.isEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 56),
            child: Center(
              child: Text(
                widget.profile.emptyMessage,
                style: const TextStyle(color: MacGlassTokens.muted),
              ),
            ),
          )
        else ...[
          _PidMatrixView(
            matrix: matrix,
            controller: widget.controller,
            lockRollPitch: _lockRollPitch,
            onLockRollPitchChanged: (value) =>
                setState(() => _lockRollPitch = value),
            onWrite: widget.onWrite,
          ),
          if (matrix.advancedGroups.isNotEmpty) ...[
            const SizedBox(height: 18),
            _PidAdvancedGroups(
              groups: matrix.advancedGroups,
              controller: widget.controller,
              onWrite: widget.onWrite,
            ),
          ],
        ],
      ],
    );
  }

  static bool _matchesEverything(FlightControllerParameter _) => true;

  bool _isPidRelatedParameter(
    FlightControllerParameter parameter,
    _PidProfile kind,
  ) {
    final name = parameter.name;
    if (kind == _PidProfile.plane) {
      return name.startsWith('RLL_') ||
          name.startsWith('PTCH_') ||
          name.startsWith('YAW_') ||
          name.startsWith('RLL2SRV_') ||
          name.startsWith('PTCH2SRV_') ||
          name.startsWith('YAW2SRV_') ||
          name.startsWith('STEER2SRV_') ||
          name.startsWith('TECS_') ||
          name.startsWith('NAVL1_') ||
          name.startsWith('LAND_');
    }
    return RegExp(
          r'_(P|I|D|FF|IMAX|ILMI|FLTD|FLTE|FLTT|SMAX|PDMX|NEF|NTF)$',
        ).hasMatch(name) ||
        name.contains('_ACC_') ||
        name.contains('_RATE_') ||
        name.contains('ANGLE_MAX') ||
        name.contains('INPUT_TC') ||
        name.contains('JERK') ||
        name.contains('THR_MIX') ||
        name.contains('LAND_');
  }
}

class _CopterPidIntro extends StatelessWidget {
  const _CopterPidIntro();

  @override
  Widget build(BuildContext context) => Column(
    key: const ValueKey('mg-pid-copter-page'),
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Text('多旋翼姿态控制', style: MacGlassTypography.sectionTitle),
      const SizedBox(height: 3),
      Text('横滚、俯仰、偏航统一放在同一页，先调姿态环，再调角速度环。', style: MacGlassTypography.caption),
    ],
  );
}

class _CopterPidTuningView extends StatelessWidget {
  const _CopterPidTuningView({
    required this.parameters,
    required this.controller,
    required this.onWrite,
  });

  final List<FlightControllerParameter> parameters;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final axes = <_CopterAxisDefinition>[
      const _CopterAxisDefinition(
        token: 'RLL',
        shortLabel: 'Roll',
        label: '横滚',
      ),
      const _CopterAxisDefinition(
        token: 'PIT',
        shortLabel: 'Pitch',
        label: '俯仰',
      ),
      const _CopterAxisDefinition(token: 'YAW', shortLabel: 'Yaw', label: '偏航'),
    ];
    final used = <String>{};
    final rateCards = [
      for (final axis in axes)
        _CopterAxisPanel(
          axis: axis,
          parameters: _takeCopterRateParameters(parameters, axis, used),
          controller: controller,
          onWrite: onWrite,
        ),
    ];
    final attitude = _takeCopterControlParameters(
      parameters,
      used,
      title: '姿态',
      matcher: (name) =>
          name.startsWith('ATC_ANG_') ||
          name == 'ATC_ANGLE_RLL_MAX' ||
          name == 'ATC_ANGLE_PIT_MAX' ||
          name == 'ATC_ACCEL_R_MAX' ||
          name == 'ATC_ACCEL_P_MAX' ||
          name == 'ATC_ACCEL_Y_MAX',
    );
    final altitude = _takeCopterControlParameters(
      parameters,
      used,
      title: '高度',
      matcher: (name) =>
          name.startsWith('PSC_POSZ_') ||
          name.startsWith('PSC_VELZ_') ||
          name.startsWith('PSC_ACCZ_'),
    );
    final position = _takeCopterControlParameters(
      parameters,
      used,
      title: '位置',
      matcher: (name) =>
          name.startsWith('PSC_POSXY_') ||
          name.startsWith('PSC_VELXY_') ||
          name.startsWith('PSC_ACCXY_'),
    );
    final remaining = parameters
        .where((parameter) => !used.contains(parameter.name))
        .toList();

    return Column(
      key: const ValueKey('mg-pid-copter-view'),
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _CopterPidSectionHeading(
          title: '角速率控制',
          subtitle: '横滚、俯仰、偏航使用相同的速率环布局。',
        ),
        const SizedBox(height: 8),
        _CopterResponsiveCardGrid(children: rateCards),
        const SizedBox(height: 18),
        _CopterPidSectionHeading(
          title: '姿态与位置控制',
          subtitle: '外环、位置和高度参数按控制链路分组。',
        ),
        const SizedBox(height: 8),
        _CopterResponsiveCardGrid(
          children: [
            _CopterControlCard(
              key: const ValueKey('mg-pid-copter-attitude'),
              title: '姿态',
              subtitle: '姿态角响应与最大倾角/加速度限制',
              parameters: attitude,
              controller: controller,
              onWrite: onWrite,
            ),
            _CopterControlCard(
              key: const ValueKey('mg-pid-copter-altitude'),
              title: '高度',
              subtitle: '高度位置、速度和加速度控制',
              parameters: altitude,
              controller: controller,
              onWrite: onWrite,
            ),
            _CopterControlCard(
              key: const ValueKey('mg-pid-copter-position'),
              title: '位置',
              subtitle: '水平位置、速度和加速度控制',
              parameters: position,
              controller: controller,
              onWrite: onWrite,
            ),
          ],
        ),
        if (remaining.isNotEmpty) ...[
          const SizedBox(height: 14),
          _CopterAdditionalParameters(
            parameters: remaining,
            controller: controller,
            onWrite: onWrite,
          ),
        ],
      ],
    );
  }
}

class _CopterPidSectionHeading extends StatelessWidget {
  const _CopterPidSectionHeading({required this.title, required this.subtitle});

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Text(title, style: MacGlassTypography.cardTitle),
      const SizedBox(height: 2),
      Text(subtitle, style: MacGlassTypography.caption),
    ],
  );
}

class _CopterResponsiveCardGrid extends StatelessWidget {
  const _CopterResponsiveCardGrid({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 980
          ? 3
          : constraints.maxWidth >= 650
          ? 2
          : 1;
      const gap = 12.0;
      final width = (constraints.maxWidth - gap * (columns - 1)) / columns;
      return Wrap(
        spacing: gap,
        runSpacing: gap,
        children: [
          for (final child in children) SizedBox(width: width, child: child),
        ],
      );
    },
  );
}

class _CopterAxisDefinition {
  const _CopterAxisDefinition({
    required this.token,
    required this.shortLabel,
    required this.label,
  });

  final String token;
  final String shortLabel;
  final String label;
}

List<FlightControllerParameter> _takeCopterRateParameters(
  List<FlightControllerParameter> parameters,
  _CopterAxisDefinition axis,
  Set<String> used,
) {
  final result =
      parameters
          .where(
            (parameter) =>
                parameter.name.startsWith('ATC_RAT_${axis.token}_') &&
                const {
                  'P',
                  'I',
                  'D',
                  'FF',
                  'IMAX',
                  'ILMI',
                  'FLTE',
                  'FLTD',
                  'FLTT',
                  'NEF',
                  'NTF',
                  'SMAX',
                  'PDMX',
                }.any((suffix) => parameter.name.endsWith('_$suffix')),
          )
          .toList()
        ..sort(_comparePidCardParameters);
  used.addAll(result.map((parameter) => parameter.name));
  return result;
}

List<FlightControllerParameter> _takeCopterControlParameters(
  List<FlightControllerParameter> parameters,
  Set<String> used, {
  required String title,
  required bool Function(String name) matcher,
}) {
  final result =
      parameters.where((parameter) => matcher(parameter.name)).toList()
        ..sort((a, b) {
          final aLabel = _copterControlLabel(a.name, title);
          final bLabel = _copterControlLabel(b.name, title);
          return aLabel.compareTo(bLabel);
        });
  used.addAll(result.map((parameter) => parameter.name));
  return result;
}

String _copterControlLabel(String name, String section) {
  if (section == '姿态') {
    if (name == 'ATC_ANG_RLL_P') return '横滚控制力度';
    if (name == 'ATC_ANG_PIT_P') return '俯仰控制力度';
    if (name == 'ATC_ANG_YAW_P') return '偏航控制力度';
    if (name == 'ATC_ANGLE_RLL_MAX') return 'R_MAX';
    if (name == 'ATC_ANGLE_PIT_MAX') return 'P_MAX';
    if (name == 'ATC_ACCEL_Y_MAX') return 'Y_MAX';
  }
  if (section == '高度') {
    if (name.startsWith('PSC_POSZ_')) return '高度P';
    if (name.startsWith('PSC_VELZ_')) {
      return '速度${name.substring('PSC_VELZ_'.length)}';
    }
    if (name.startsWith('PSC_ACCZ_')) {
      return '加速度${name.substring('PSC_ACCZ_'.length)}';
    }
  }
  if (section == '位置') {
    if (name.startsWith('PSC_POSXY_')) return '位置P';
    if (name.startsWith('PSC_VELXY_')) {
      return '速度${name.substring('PSC_VELXY_'.length)}';
    }
    if (name.startsWith('PSC_ACCXY_')) {
      return '加速度${name.substring('PSC_ACCXY_'.length)}';
    }
  }
  return _pidFieldShortLabel(name);
}

class _CopterAxisPanel extends StatelessWidget {
  const _CopterAxisPanel({
    required this.axis,
    required this.parameters,
    required this.controller,
    required this.onWrite,
  });

  final _CopterAxisDefinition axis;
  final List<FlightControllerParameter> parameters;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => _CopterControlCard(
    key: ValueKey(
      'mg-pid-copter-rate-${switch (axis.token) {
        'RLL' => 'roll',
        'PIT' => 'pitch',
        _ => 'yaw',
      }}',
    ),
    title: '${axis.label}角速率',
    subtitle: '${axis.shortLabel} rate controller',
    parameters: parameters,
    controller: controller,
    onWrite: onWrite,
  );
}

class _CopterControlCard extends StatefulWidget {
  const _CopterControlCard({
    super.key,
    required this.title,
    required this.subtitle,
    required this.parameters,
    required this.controller,
    required this.onWrite,
  });

  final String title;
  final String subtitle;
  final List<FlightControllerParameter> parameters;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  State<_CopterControlCard> createState() => _CopterControlCardState();
}

class _CopterControlCardState extends State<_CopterControlCard> {
  bool _expanded = true;

  @override
  Widget build(BuildContext context) => _PidGlassSurface(
    child: Padding(
      padding: const EdgeInsets.fromLTRB(13, 12, 13, 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(widget.title, style: MacGlassTypography.cardTitle),
              ),
              Text(
                '${widget.parameters.length} 项',
                style: MacGlassTypography.caption.copyWith(
                  color: MacGlassTokens.blue,
                ),
              ),
            ],
          ),
          const SizedBox(height: 2),
          Text(widget.subtitle, style: MacGlassTypography.caption),
          AnimatedCrossFade(
            firstChild: Padding(
              padding: const EdgeInsets.only(top: 11),
              child: _CopterPidFields(
                parameters: widget.parameters,
                controller: widget.controller,
                onWrite: widget.onWrite,
                sectionTitle: widget.title,
              ),
            ),
            secondChild: const SizedBox.shrink(),
            crossFadeState: _expanded
                ? CrossFadeState.showFirst
                : CrossFadeState.showSecond,
            duration: const Duration(milliseconds: 160),
          ),
          const SizedBox(height: 4),
          InkWell(
            key: ValueKey('mg-pid-copter-toggle-${widget.title}'),
            borderRadius: BorderRadius.circular(8),
            onTap: () => setState(() => _expanded = !_expanded),
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 5),
              child: Icon(
                _expanded
                    ? Icons.keyboard_arrow_up_rounded
                    : Icons.keyboard_arrow_down_rounded,
                size: 20,
                color: MacGlassTokens.muted,
              ),
            ),
          ),
        ],
      ),
    ),
  );
}

class _CopterPidFields extends StatelessWidget {
  const _CopterPidFields({
    required this.parameters,
    required this.controller,
    required this.onWrite,
    this.sectionTitle = '',
  });

  final List<FlightControllerParameter> parameters;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;
  final String sectionTitle;

  @override
  Widget build(BuildContext context) {
    if (parameters.isEmpty) {
      return const Text('固件未报告该参数', style: MacGlassTypography.caption);
    }
    return Column(
      children: [
        for (var index = 0; index < parameters.length; index++) ...[
          if (index > 0) const SizedBox(height: 6),
          _PidValueField(
            parameter: parameters[index],
            descriptor: controller.descriptorFor(parameters[index].name),
            enabled: controller.canEdit,
            compactLabel: _copterControlLabel(
              parameters[index].name,
              sectionTitle,
            ),
            onWrite: onWrite,
          ),
        ],
      ],
    );
  }
}

class _CopterAdditionalParameters extends StatelessWidget {
  const _CopterAdditionalParameters({
    required this.parameters,
    required this.controller,
    required this.onWrite,
  });

  final List<FlightControllerParameter> parameters;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => _CopterControlCard(
    key: const ValueKey('mg-pid-copter-additional'),
    title: '其他控制参数',
    subtitle: '当前固件提供、但未归入标准控制链路的参数',
    parameters: parameters,
    controller: controller,
    onWrite: onWrite,
  );
}

class _PidGlassSurface extends StatelessWidget {
  const _PidGlassSurface({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.30),
      border: Border.all(color: Colors.white.withValues(alpha: 0.72)),
      borderRadius: BorderRadius.circular(14),
      boxShadow: [MacGlassTokens.floatingShadow()],
    ),
    child: ClipRRect(
      borderRadius: BorderRadius.circular(14),
      child: Material(type: MaterialType.transparency, child: child),
    ),
  );
}

class _PidMatrixLayout {
  const _PidMatrixLayout({
    required this.attitudeCards,
    required this.rateCards,
    required this.advancedGroups,
  });

  final List<_PidControllerCardModel> attitudeCards;
  final List<_PidControllerCardModel> rateCards;
  final List<_PidCategorizedGroup> advancedGroups;
}

_PidMatrixLayout _pidMatrixFor(List<_PidCategorizedGroup> groups) {
  final attitudeTitles = <String>{'姿态外环', '横滚控制器', '俯仰控制器', '偏航与转向'};
  final rateTitles = <String>{'角速度内环', '角速度滤波与保护', '固定翼轴速率'};
  final attitudeGroups = groups
      .where((group) => attitudeTitles.contains(group.group.title))
      .toList(growable: false);
  final rateGroups = groups
      .where((group) => rateTitles.contains(group.group.title))
      .toList(growable: false);
  final used = <String>{
    for (final group in [...attitudeGroups, ...rateGroups])
      for (final parameter in group.parameters) parameter.name,
  };
  final advanced = groups
      .where(
        (group) =>
            !attitudeTitles.contains(group.group.title) &&
            !rateTitles.contains(group.group.title),
      )
      .toList(growable: false);
  final cardGroup = _PidGroup(
    title: attitudeGroups.isNotEmpty ? '姿态外环' : '角速度内环',
    subtitle: attitudeGroups.isNotEmpty
        ? '横滚、俯仰和偏航的姿态目标响应。'
        : '以 P / I / D / FF 追踪目标角速度。',
    matches: _PidParameterPaneState._matchesEverything,
  );
  final rateGroup = _PidGroup(
    title: '角速度内环',
    subtitle: '速率增益、滤波与输出保护。',
    matches: _PidParameterPaneState._matchesEverything,
  );
  return _PidMatrixLayout(
    attitudeCards: _mergePidCards(cardGroup, attitudeGroups),
    rateCards: _mergePidCards(rateGroup, rateGroups),
    advancedGroups: [
      for (final group in advanced)
        _PidCategorizedGroup(
          group: group.group,
          parameters: group.parameters
              .where((parameter) => !used.contains(parameter.name))
              .toList(growable: false),
        ),
    ]..removeWhere((group) => group.parameters.isEmpty),
  );
}

List<_PidControllerCardModel> _mergePidCards(
  _PidGroup cardGroup,
  List<_PidCategorizedGroup> groups,
) {
  if (groups.isEmpty) return const [];
  final parameters = <FlightControllerParameter>[
    for (final group in groups) ...group.parameters,
  ];
  return _pidCardsFor(cardGroup, parameters);
}

class _PidMatrixView extends StatelessWidget {
  const _PidMatrixView({
    required this.matrix,
    required this.controller,
    required this.lockRollPitch,
    required this.onLockRollPitchChanged,
    required this.onWrite,
  });

  final _PidMatrixLayout matrix;
  final ParameterController controller;
  final bool lockRollPitch;
  final ValueChanged<bool> onLockRollPitchChanged;
  final Future<bool> Function(String name, double value) onWrite;

  Future<bool> _writeWithLock(String name, double value) async {
    final primary = await onWrite(name, value);
    if (!primary || !lockRollPitch) return primary;
    final counterpart = _pidRollPitchCounterpart(name);
    if (counterpart == null ||
        !controller.parameters.any(
          (parameter) => parameter.name == counterpart,
        )) {
      return primary;
    }
    final linked = await onWrite(counterpart, value);
    return primary && linked;
  }

  @override
  Widget build(BuildContext context) => Column(
    key: const ValueKey('mg-pid-matrix'),
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      if (matrix.attitudeCards.isNotEmpty) ...[
        const _PidMatrixSectionHeading(
          title: '姿态角控制',
          subtitle: '横向比较外环增益，保持 Roll / Pitch / Yaw 的调参节奏一致。',
        ),
        const SizedBox(height: 10),
        _PidMatrixCardGrid(
          rowKey: 'attitude',
          cards: matrix.attitudeCards,
          controller: controller,
          onWrite: _writeWithLock,
        ),
      ],
      if (matrix.attitudeCards.isNotEmpty && matrix.rateCards.isNotEmpty) ...[
        const SizedBox(height: 12),
        _PidLockControl(
          value: lockRollPitch,
          onChanged: onLockRollPitchChanged,
        ),
        const SizedBox(height: 14),
      ],
      if (matrix.rateCards.isNotEmpty) ...[
        const _PidMatrixSectionHeading(
          title: '角速度控制',
          subtitle: '内环 PID、滤波和输出保护集中在同一行，便于快速比较。',
        ),
        const SizedBox(height: 10),
        _PidMatrixCardGrid(
          rowKey: 'rate',
          cards: matrix.rateCards,
          controller: controller,
          onWrite: _writeWithLock,
        ),
      ],
      if (matrix.attitudeCards.isEmpty && matrix.rateCards.isNotEmpty) ...[
        const SizedBox(height: 12),
        _PidLockControl(
          value: lockRollPitch,
          onChanged: onLockRollPitchChanged,
        ),
      ],
    ],
  );
}

String? _pidRollPitchCounterpart(String name) {
  if (name.contains('_RLL_')) return name.replaceFirst('_RLL_', '_PIT_');
  if (name.contains('_PIT_')) return name.replaceFirst('_PIT_', '_RLL_');
  if (name.startsWith('RLL2SRV_')) {
    return name.replaceFirst('RLL2SRV_', 'PTCH2SRV_');
  }
  if (name.startsWith('PTCH2SRV_')) {
    return name.replaceFirst('PTCH2SRV_', 'RLL2SRV_');
  }
  if (name.startsWith('RLL_')) return name.replaceFirst('RLL_', 'PTCH_');
  if (name.startsWith('PTCH_')) return name.replaceFirst('PTCH_', 'RLL_');
  return null;
}

class _PidMatrixSectionHeading extends StatelessWidget {
  const _PidMatrixSectionHeading({required this.title, required this.subtitle});

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Text(title, style: MacGlassTypography.sectionTitle),
      const SizedBox(height: 2),
      Text(subtitle, style: MacGlassTypography.caption),
    ],
  );
}

class _PidMatrixCardGrid extends StatelessWidget {
  const _PidMatrixCardGrid({
    required this.rowKey,
    required this.cards,
    required this.controller,
    required this.onWrite,
  });

  final String rowKey;
  final List<_PidControllerCardModel> cards;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 980
          ? 4
          : constraints.maxWidth >= 620
          ? 2
          : 1;
      const gap = 10.0;
      final width = (constraints.maxWidth - gap * (columns - 1)) / columns;
      return Wrap(
        spacing: gap,
        runSpacing: gap,
        children: [
          for (final card in cards)
            SizedBox(
              width: width,
              child: KeyedSubtree(
                key: ValueKey('mg-pid-card-$rowKey-${card.key}'),
                child: _PidControllerCard(
                  card: card,
                  controller: controller,
                  enabled: controller.canEdit,
                  onWrite: onWrite,
                ),
              ),
            ),
        ],
      );
    },
  );
}

class _PidLockControl extends StatelessWidget {
  const _PidLockControl({required this.value, required this.onChanged});

  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.30),
      border: Border.all(color: Colors.white.withValues(alpha: 0.70)),
      borderRadius: BorderRadius.circular(12),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 8),
      child: Row(
        children: [
          const Icon(Icons.link_rounded, size: 16, color: MacGlassTokens.blue),
          const SizedBox(width: 8),
          const Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('锁定 Pitch 和 Roll 的值', style: MacGlassTypography.control),
                SizedBox(height: 1),
                Text(
                  '开启后用于联动调参提示，实际写入仍由每个参数单独确认。',
                  style: MacGlassTypography.caption,
                ),
              ],
            ),
          ),
          cupertino.CupertinoSwitch(
            key: ValueKey('mg-pid-lock-roll-pitch'),
            value: value,
            onChanged: onChanged,
            activeTrackColor: MacGlassTokens.blue,
          ),
        ],
      ),
    ),
  );
}

class _PidAdvancedGroups extends StatelessWidget {
  const _PidAdvancedGroups({
    required this.groups,
    required this.controller,
    required this.onWrite,
  });

  final List<_PidCategorizedGroup> groups;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    key: const ValueKey('mg-pid-advanced'),
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.24),
      border: Border.all(color: Colors.white.withValues(alpha: 0.68)),
      borderRadius: BorderRadius.circular(14),
    ),
    child: Theme(
      data: Theme.of(context).copyWith(dividerColor: Colors.transparent),
      child: ExpansionTile(
        tilePadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 2),
        childrenPadding: const EdgeInsets.fromLTRB(14, 0, 14, 14),
        leading: const Icon(
          Icons.tune_rounded,
          size: 18,
          color: MacGlassTokens.muted,
        ),
        title: const Text('高级控制参数', style: MacGlassTypography.cardTitle),
        subtitle: Text(
          '${groups.length} 个控制域 · 固件提供的扩展项',
          style: MacGlassTypography.caption,
        ),
        children: [
          for (var index = 0; index < groups.length; index++) ...[
            if (index > 0) const SizedBox(height: 12),
            _PidAdvancedGroup(
              group: groups[index],
              controller: controller,
              onWrite: onWrite,
            ),
          ],
        ],
      ),
    ),
  );
}

class _PidAdvancedGroup extends StatelessWidget {
  const _PidAdvancedGroup({
    required this.group,
    required this.controller,
    required this.onWrite,
  });

  final _PidCategorizedGroup group;
  final ParameterController controller;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final cards = _pidCardsFor(group.group, group.parameters);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(group.group.title, style: MacGlassTypography.control),
        const SizedBox(height: 2),
        Text(group.group.subtitle, style: MacGlassTypography.caption),
        const SizedBox(height: 8),
        _PidMatrixCardGrid(
          rowKey: 'advanced-${group.group.title}',
          cards: cards,
          controller: controller,
          onWrite: onWrite,
        ),
      ],
    );
  }
}

class _PidCategorizedGroup {
  const _PidCategorizedGroup({required this.group, required this.parameters});

  final _PidGroup group;
  final List<FlightControllerParameter> parameters;
}

enum _PidProfile { copter, plane, vtol, common, generic }

class _PidProfileDefinition {
  const _PidProfileDefinition({
    required this.kind,
    required this.key,
    required this.tabLabel,
    required this.title,
    required this.subtitle,
    required this.emptyMessage,
    required this.prefixes,
  });

  final _PidProfile kind;
  final String key;
  final String tabLabel;
  final String title;
  final String subtitle;
  final String emptyMessage;
  final List<String> prefixes;
}

List<_PidProfileDefinition> _pidProfilesFor({
  required String? airframe,
  required List<FlightControllerParameter> parameters,
}) {
  final names = parameters.map((parameter) => parameter.name).toList();
  final hasVtol = names.any(
    (name) => name.startsWith('Q_A_') || name.startsWith('Q_P_'),
  );
  final hasPlane = names.any(
    (name) =>
        name.startsWith('RLL_') ||
        name.startsWith('PTCH_') ||
        name.startsWith('TECS_') ||
        name.startsWith('NAVL1_'),
  );
  final hasCopter = names.any(
    (name) => name.startsWith('ATC_') || name.startsWith('PSC_'),
  );
  final vehicle = airframe?.trim().toUpperCase();

  // HEARTBEAT identity is preferred. Parameter signatures keep the page useful
  // while identity is still being fetched or on firmware with unusual MAV_TYPE.
  if (vehicle == 'QPLANE' || vehicle == 'VTOL' || hasVtol) {
    final profiles = <_PidProfileDefinition>[
      if (hasPlane) _planePidProfile,
      if (hasVtol) _vtolPidProfile,
      if (hasCopter) _commonPidProfile,
    ];
    return profiles.isEmpty ? const [_genericPidProfile] : profiles;
  }
  if (vehicle == 'COPTER') return const [_copterPidProfile];
  if (vehicle == 'PLANE') {
    return [_planePidProfile, if (hasCopter) _commonPidProfile];
  }
  if (hasCopter) return const [_copterPidProfile];
  if (hasPlane) return const [_planePidProfile];
  return const [_genericPidProfile];
}

const _copterPidProfile = _PidProfileDefinition(
  kind: _PidProfile.copter,
  key: 'copter',
  tabLabel: '多旋翼 PID',
  title: '多旋翼控制器',
  subtitle: '按姿态、角速度、位置与高度分组；每次修改立即写入飞控。',
  emptyMessage: '当前 Copter 固件没有报告可调的 PID 参数。',
  prefixes: ['ATC_', 'PSC_'],
);

const _planePidProfile = _PidProfileDefinition(
  kind: _PidProfile.plane,
  key: 'plane',
  tabLabel: '固定翼 PID',
  title: '固定翼控制器',
  subtitle: '按横滚、俯仰、偏航、能量与导航控制器分组；每次修改立即写入飞控。',
  emptyMessage: '当前 Plane 固件没有报告可调的 PID 参数。',
  prefixes: [
    'RLL_',
    'PTCH_',
    'YAW_',
    'RLL2SRV_',
    'PTCH2SRV_',
    'YAW2SRV_',
    'STEER2SRV_',
    'TECS_',
    'NAVL1_',
    'LAND_',
  ],
);

const _vtolPidProfile = _PidProfileDefinition(
  kind: _PidProfile.vtol,
  key: 'vtol',
  tabLabel: 'VTOL PID',
  title: 'VTOL 控制器',
  subtitle: '仅显示 QuadPlane 实际报告的垂起姿态、速率与位置控制参数。',
  emptyMessage: '当前 QuadPlane 固件没有报告 VTOL PID 参数。',
  prefixes: ['Q_A_', 'Q_P_'],
);

const _commonPidProfile = _PidProfileDefinition(
  kind: _PidProfile.common,
  key: 'common',
  tabLabel: '通用 PID',
  title: '通用控制器',
  subtitle: '显示当前固件同时提供给多个机型的共用控制参数。',
  emptyMessage: '当前固件没有报告共用 PID 参数。',
  prefixes: ['ATC_', 'PSC_'],
);

const _genericPidProfile = _PidProfileDefinition(
  kind: _PidProfile.generic,
  key: 'generic',
  tabLabel: 'PID 调参',
  title: 'PID 控制器',
  subtitle: '尚未识别机型；仅显示飞控实际报告的通用 PID 参数。',
  emptyMessage: '请完成参数下载后再查看该飞控可用的 PID 参数。',
  prefixes: ['ATC_', 'PSC_', 'RLL_', 'PTCH_', 'YAW_', 'Q_A_', 'Q_P_'],
);

class _PidGroup {
  const _PidGroup({
    required this.title,
    required this.subtitle,
    required this.matches,
  });

  final String title;
  final String subtitle;
  final bool Function(FlightControllerParameter parameter) matches;
}

List<_PidGroup> _pidGroupsFor(_PidProfile profile) {
  if (profile == _PidProfile.plane) return _planePidGroups();
  final attitudePrefix = profile == _PidProfile.vtol ? 'Q_A_' : 'ATC_';
  final positionPrefix = profile == _PidProfile.vtol ? 'Q_P_' : 'PSC_';
  return [
    _PidGroup(
      title: '姿态外环',
      subtitle: '将目标横滚、俯仰或偏航角转换为目标角速度。',
      matches: (parameter) =>
          parameter.name.startsWith(attitudePrefix) &&
          parameter.name.contains('_ANG_'),
    ),
    _PidGroup(
      title: '角速度内环',
      subtitle: '以 P / I / D / FF 追踪目标角速度，是姿态稳定的核心环路。',
      matches: (parameter) =>
          parameter.name.startsWith(attitudePrefix) &&
          parameter.name.contains('_RAT_') &&
          RegExp(r'_(P|I|D|FF|IMAX|ILMI)$').hasMatch(parameter.name),
    ),
    _PidGroup(
      title: '角速度滤波与保护',
      subtitle: '控制目标、误差和 D 项滤波，并限制瞬时输出与 PD 总和。',
      matches: (parameter) =>
          parameter.name.startsWith(attitudePrefix) &&
          parameter.name.contains('_RAT_'),
    ),
    _PidGroup(
      title: '水平位置与速度',
      subtitle: '将水平位置误差转化为速度，并由速度控制器产生加速度指令。',
      matches: (parameter) =>
          parameter.name.startsWith(positionPrefix) &&
          (parameter.name.contains('_NE_') ||
              parameter.name.contains('POSXY') ||
              parameter.name.contains('VELXY') ||
              parameter.name.contains('ACCXY')),
    ),
    _PidGroup(
      title: '高度与爬升速度',
      subtitle: '将高度误差转化为爬升速度，再由垂直速度控制器驱动油门。',
      matches: (parameter) =>
          parameter.name.startsWith(positionPrefix) &&
          (parameter.name.contains('_D_') ||
              parameter.name.contains('POSZ') ||
              parameter.name.contains('VELZ') ||
              parameter.name.contains('ACCZ')) &&
          !parameter.name.contains('_NE_'),
    ),
    _PidGroup(
      title: '限制与响应',
      subtitle: '设置最大角度、角速度、加速度、输入响应、推力混合与着陆增益。',
      matches: (parameter) =>
          parameter.name.startsWith(attitudePrefix) ||
          parameter.name.startsWith(positionPrefix) ||
          parameter.name.startsWith('LAND_'),
    ),
  ];
}

List<_PidGroup> _planePidGroups() => const [
  _PidGroup(
    title: '横滚控制器',
    subtitle: '横滚速率与舵面输出控制器。',
    matches: _matchesPlaneRoll,
  ),
  _PidGroup(
    title: '俯仰控制器',
    subtitle: '俯仰速率与舵面输出控制器。',
    matches: _matchesPlanePitch,
  ),
  _PidGroup(
    title: '偏航与转向',
    subtitle: '方向舵偏航控制与地面转向控制器。',
    matches: _matchesPlaneYaw,
  ),
  _PidGroup(
    title: '高度与空速',
    subtitle: 'TECS 能量控制器，协调油门、俯仰、高度和空速。',
    matches: _matchesPlaneTecs,
  ),
  _PidGroup(
    title: '导航与航迹',
    subtitle: 'L1 导航器的航迹跟随、阻尼和交叉航迹积分参数。',
    matches: _matchesPlaneNavigation,
  ),
  _PidGroup(
    title: '着陆响应',
    subtitle: '进近与着陆阶段的高度、俯仰和油门响应。',
    matches: _matchesPlaneLanding,
  ),
];

bool _matchesPlaneRoll(FlightControllerParameter parameter) =>
    parameter.name.startsWith('RLL_') || parameter.name.startsWith('RLL2SRV_');

bool _matchesPlanePitch(FlightControllerParameter parameter) =>
    parameter.name.startsWith('PTCH_') ||
    parameter.name.startsWith('PTCH2SRV_');

bool _matchesPlaneYaw(FlightControllerParameter parameter) =>
    parameter.name.startsWith('YAW_') ||
    parameter.name.startsWith('YAW2SRV_') ||
    parameter.name.startsWith('STEER2SRV_');

bool _matchesPlaneTecs(FlightControllerParameter parameter) =>
    parameter.name.startsWith('TECS_');

bool _matchesPlaneNavigation(FlightControllerParameter parameter) =>
    parameter.name.startsWith('NAVL1_');

bool _matchesPlaneLanding(FlightControllerParameter parameter) =>
    parameter.name.startsWith('LAND_');

class _PidControllerCardModel {
  const _PidControllerCardModel({
    required this.key,
    required this.title,
    required this.subtitle,
    required this.icon,
    required this.parameters,
  });

  final String key;
  final String title;
  final String subtitle;
  final IconData icon;
  final List<FlightControllerParameter> parameters;
}

List<_PidControllerCardModel> _pidCardsFor(
  _PidGroup group,
  List<FlightControllerParameter> parameters,
) {
  final byKey = <String, List<FlightControllerParameter>>{};
  for (final parameter in parameters) {
    (byKey[_pidCardKey(group, parameter)] ??= []).add(parameter);
  }
  final cards = byKey.entries
      .map((entry) {
        final values = entry.value..sort(_comparePidCardParameters);
        return _PidControllerCardModel(
          key: entry.key,
          title: _pidCardTitle(group, entry.key),
          subtitle: _pidCardSubtitle(group, entry.key),
          icon: _pidCardIcon(entry.key),
          parameters: values,
        );
      })
      .toList(growable: false);
  cards.sort(
    (left, right) =>
        _pidCardOrder(left.key).compareTo(_pidCardOrder(right.key)),
  );
  return cards;
}

int _pidCardOrder(String key) => switch (key) {
  'roll' => 0,
  'pitch' => 1,
  'yaw' => 2,
  'position-xy' => 3,
  'velocity-xy' => 4,
  'acceleration-xy' => 5,
  'position-z' => 6,
  'velocity-z' => 7,
  'acceleration-z' => 8,
  'angle-limit' => 9,
  'rate-limit' => 10,
  'input-response' => 11,
  'throttle-mix' => 12,
  'landing' => 13,
  _ => 14,
};

String _pidCardKey(_PidGroup group, FlightControllerParameter parameter) {
  final name = parameter.name;
  if (name.startsWith('RLL_') || name.contains('_RLL_')) return 'roll';
  if (name.startsWith('PTCH_') ||
      name.contains('_PIT_') ||
      name.contains('_PTCH_')) {
    return 'pitch';
  }
  if (name.startsWith('YAW_') || name.contains('_YAW_')) return 'yaw';
  if (name.contains('POSXY') || name.contains('_NE_POS_')) return 'position-xy';
  if (name.contains('VELXY') || name.contains('_NE_VEL_')) return 'velocity-xy';
  if (name.contains('ACCXY') || name.contains('_NE_ACC_')) {
    return 'acceleration-xy';
  }
  if (name.contains('POSZ') || name.contains('_D_POS_')) return 'position-z';
  if (name.contains('VELZ') || name.contains('_D_VEL_')) return 'velocity-z';
  if (name.contains('ACCZ') || name.contains('_D_ACC_')) {
    return 'acceleration-z';
  }
  if (name.contains('ANGLE_MAX') || name.contains('ANG_LIM')) {
    return 'angle-limit';
  }
  if (name.contains('RATE_MAX') ||
      name.contains('_ACC_') ||
      name.contains('JERK')) {
    return 'rate-limit';
  }
  if (name.contains('INPUT_TC')) return 'input-response';
  if (name.contains('THR_MIX')) return 'throttle-mix';
  if (name.contains('LAND_')) return 'landing';
  if (group.title == '其他 PID 相关参数') return 'advanced';
  return 'general';
}

String _pidCardTitle(_PidGroup group, String key) {
  final axisTitle = switch (key) {
    'roll' => '横滚',
    'pitch' => '俯仰',
    'yaw' => '偏航',
    _ => '',
  };
  if (axisTitle.isNotEmpty) {
    final control = switch (group.title) {
      '姿态外环' => '姿态角',
      '角速度内环' => '速率 PID',
      '角速度滤波与保护' => '速率滤波',
      '固定翼轴速率' => '固定翼速率',
      _ => group.title,
    };
    return '$axisTitle $control';
  }
  return switch (key) {
    'position-xy' => '水平位置',
    'velocity-xy' => '水平速度',
    'acceleration-xy' => '水平加速度',
    'position-z' => '高度位置',
    'velocity-z' => '爬升速度',
    'acceleration-z' => '垂直加速度',
    'angle-limit' => '姿态限制',
    'rate-limit' => '速率与加速度限制',
    'input-response' => '操控响应',
    'throttle-mix' => '推力混合',
    'landing' => '着陆增益',
    'advanced' => '高级控制项',
    _ => group.title,
  };
}

String _pidCardSubtitle(_PidGroup group, String key) => switch (key) {
  'roll' || 'pitch' || 'yaw' => group.subtitle,
  'position-xy' => '位置误差到目标速度的外环。',
  'velocity-xy' => '目标速度到加速度指令的内环。',
  'acceleration-xy' => '限制水平机动加速度与平滑响应。',
  'position-z' => '高度误差到目标爬升速度的外环。',
  'velocity-z' => '目标爬升速度到油门输出的控制环。',
  'acceleration-z' => '限制垂直加减速与目标平滑度。',
  'angle-limit' => '飞行器姿态与倾角边界。',
  'rate-limit' => '最大旋转速度、加速度与轨迹平滑度。',
  'input-response' => '操纵输入的过渡速度与手感。',
  'throttle-mix' => '姿态保持与油门之间的优先级。',
  'landing' => '着陆阶段使用的响应倍率。',
  _ => group.subtitle,
};

IconData _pidCardIcon(String key) => switch (key) {
  'roll' => Icons.rotate_right_rounded,
  'pitch' => Icons.flight_rounded,
  'yaw' => Icons.explore_rounded,
  'position-xy' ||
  'velocity-xy' ||
  'acceleration-xy' => Icons.navigation_rounded,
  'position-z' || 'velocity-z' || 'acceleration-z' => Icons.height_rounded,
  'angle-limit' || 'rate-limit' => Icons.speed_rounded,
  'input-response' => Icons.tune_rounded,
  'throttle-mix' => Icons.swap_vert_rounded,
  'landing' => Icons.flight_land_rounded,
  _ => Icons.settings_input_component_rounded,
};

int _comparePidCardParameters(
  FlightControllerParameter left,
  FlightControllerParameter right,
) => _pidParameterRank(left.name).compareTo(_pidParameterRank(right.name));

int _pidParameterRank(String name) {
  const suffixes = <String>[
    '_P',
    '_I',
    '_D',
    '_FF',
    '_IMAX',
    '_ILMI',
    '_FLTE',
    '_FLTD',
    '_FLTT',
    '_NEF',
    '_NTF',
    '_SMAX',
    '_PDMX',
  ];
  for (var index = 0; index < suffixes.length; index++) {
    if (name.endsWith(suffixes[index])) return index;
  }
  return suffixes.length + name.codeUnits.fold(0, (sum, item) => sum + item);
}

class _PidControllerCard extends StatelessWidget {
  const _PidControllerCard({
    required this.card,
    required this.controller,
    required this.enabled,
    required this.onWrite,
  });

  final _PidControllerCardModel card;
  final ParameterController controller;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: Colors.white.withValues(alpha: 0.30),
      border: Border.all(color: Colors.white.withValues(alpha: 0.72)),
      borderRadius: BorderRadius.circular(12),
      boxShadow: [MacGlassTokens.floatingShadow()],
    ),
    child: Padding(
      padding: const EdgeInsets.fromLTRB(12, 11, 12, 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  card.title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: MacGlassTypography.cardTitle,
                ),
              ),
              Icon(card.icon, size: 16, color: MacGlassTokens.blue),
            ],
          ),
          const SizedBox(height: 3),
          Text(
            card.subtitle,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: MacGlassTypography.caption,
          ),
          const SizedBox(height: 9),
          Divider(height: 1, color: Colors.white.withValues(alpha: 0.60)),
          const SizedBox(height: 10),
          LayoutBuilder(
            builder: (context, constraints) {
              final width = (constraints.maxWidth - 10) / 2;
              return Wrap(
                spacing: 10,
                runSpacing: 12,
                children: [
                  for (final parameter in card.parameters)
                    SizedBox(
                      width: width,
                      child: _PidValueField(
                        parameter: parameter,
                        descriptor: controller.descriptorFor(parameter.name),
                        enabled: enabled,
                        onWrite: onWrite,
                      ),
                    ),
                ],
              );
            },
          ),
        ],
      ),
    ),
  );
}

class _PidValueField extends StatefulWidget {
  const _PidValueField({
    required this.parameter,
    required this.descriptor,
    required this.enabled,
    required this.onWrite,
    this.compactLabel,
  });

  final FlightControllerParameter parameter;
  final ParameterDescriptor? descriptor;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;
  final String? compactLabel;

  @override
  State<_PidValueField> createState() => _PidValueFieldState();
}

class _PidValueFieldState extends State<_PidValueField> {
  late final TextEditingController _controller;
  late final FocusNode _focusNode;
  bool _writing = false;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(
      text: _formatParameterNumber(widget.parameter.value),
    );
    _focusNode = FocusNode()..addListener(_onFocusChanged);
  }

  @override
  void didUpdateWidget(covariant _PidValueField oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!_focusNode.hasFocus &&
        oldWidget.parameter.value != widget.parameter.value) {
      _controller.text = _formatParameterNumber(widget.parameter.value);
    }
  }

  @override
  void dispose() {
    _focusNode.removeListener(_onFocusChanged);
    _focusNode.dispose();
    _controller.dispose();
    super.dispose();
  }

  void _onFocusChanged() {
    if (!_focusNode.hasFocus) unawaited(_commit());
  }

  Future<void> _commit() async {
    if (!widget.enabled || _writing) return;
    final rawValue = double.tryParse(_controller.text);
    if (rawValue == null || !rawValue.isFinite) {
      _controller.text = _formatParameterNumber(widget.parameter.value);
      return;
    }
    final value = rawValue
        .clamp(
          widget.descriptor?.minValue ?? rawValue,
          widget.descriptor?.maxValue ?? rawValue,
        )
        .toDouble();
    _controller.text = _formatParameterNumber(value);
    if (value == widget.parameter.value) return;
    await _writeValue(value);
  }

  double _step() {
    final increment = widget.descriptor?.increment;
    if (increment != null && increment > 0) return increment;
    final value = widget.parameter.value.abs();
    if (value >= 100) return 1;
    if (value >= 10) return 0.1;
    if (value >= 1) return 0.01;
    if (value >= 0.1) return 0.01;
    if (value >= 0.01) return 0.001;
    return 0.0001;
  }

  Future<void> _stepValue(double direction) async {
    if (!widget.enabled || _writing) return;
    final value = (widget.parameter.value + direction * _step())
        .clamp(
          widget.descriptor?.minValue ?? double.negativeInfinity,
          widget.descriptor?.maxValue ?? double.infinity,
        )
        .toDouble();
    if (value == widget.parameter.value) return;
    _controller.text = _formatParameterNumber(value);
    await _writeValue(value);
  }

  Future<void> _writeValue(double value) async {
    _writing = true;
    try {
      await widget.onWrite(widget.parameter.name, value);
    } finally {
      _writing = false;
    }
  }

  @override
  Widget build(BuildContext context) {
    final label = widget.compactLabel ?? _pidFieldLabel(widget.parameter.name);
    final unit = widget.descriptor?.unit ?? '';
    if (widget.compactLabel != null) {
      return Row(
        key: ValueKey('mg-pid-row-${widget.parameter.name}'),
        children: [
          Expanded(
            child: Tooltip(
              message: widget.parameter.name,
              child: Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MacGlassTypography.tableHeader.copyWith(
                  color: MacGlassTokens.ink,
                ),
              ),
            ),
          ),
          const SizedBox(width: 6),
          _PidStepButton(
            key: ValueKey('mg-pid-stepper-${widget.parameter.name}-decrement'),
            icon: Icons.remove_rounded,
            enabled: widget.enabled && !_writing,
            onTap: () => unawaited(_stepValue(-1)),
          ),
          const SizedBox(width: 4),
          SizedBox(width: 86, height: 34, child: _valueEditor(unit)),
          const SizedBox(width: 4),
          _PidStepButton(
            key: ValueKey('mg-pid-stepper-${widget.parameter.name}-increment'),
            icon: Icons.add_rounded,
            enabled: widget.enabled && !_writing,
            onTap: () => unawaited(_stepValue(1)),
          ),
        ],
      );
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: MacGlassTypography.tableHeader.copyWith(
                  color: MacGlassTokens.ink,
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 2),
        Text(
          widget.parameter.name,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: MacGlassTypography.code.copyWith(
            color: MacGlassTokens.muted,
            fontSize: 9.5,
          ),
        ),
        const SizedBox(height: 5),
        SizedBox(height: 38, child: _valueEditor(unit)),
      ],
    );
  }

  Widget _valueEditor(String unit) => cupertino.CupertinoTextField(
    key: ValueKey('mg-pid-${widget.parameter.name}'),
    controller: _controller,
    focusNode: _focusNode,
    enabled: widget.enabled,
    textAlign: TextAlign.center,
    keyboardType: const TextInputType.numberWithOptions(
      decimal: true,
      signed: true,
    ),
    style: MacGlassTypography.value.copyWith(
      color: MacGlassTokens.ink,
      fontFamily: 'monospace',
    ),
    suffix: unit.isEmpty
        ? null
        : Padding(
            padding: const EdgeInsets.only(right: 7),
            child: Text(
              unit,
              style: const TextStyle(color: MacGlassTokens.muted, fontSize: 10),
            ),
          ),
    padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 7),
    decoration: BoxDecoration(
      color: MacGlassTokens.controlFill,
      border: Border.all(color: MacGlassTokens.controlBorder),
      borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
    ),
    onSubmitted: (_) => unawaited(_commit()),
  );
}

class _PidStepButton extends StatelessWidget {
  const _PidStepButton({
    super.key,
    required this.icon,
    required this.enabled,
    required this.onTap,
  });

  final IconData icon;
  final bool enabled;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => Material(
    color: Colors.white.withValues(alpha: enabled ? 0.58 : 0.28),
    borderRadius: BorderRadius.circular(7),
    child: InkWell(
      onTap: enabled ? onTap : null,
      borderRadius: BorderRadius.circular(7),
      child: SizedBox(
        width: 30,
        height: 34,
        child: Icon(icon, size: 16, color: MacGlassTokens.ink),
      ),
    ),
  );
}

String _pidFieldShortLabel(String parameterName) {
  const labels = <String, String>{
    '_P': 'P',
    '_I': 'I',
    '_D': 'D',
    '_FF': 'FF',
    '_IMAX': 'IMAX',
    '_ILMI': 'ILMI',
    '_FLTD': 'FLTD',
    '_FLTE': 'FLTE',
    '_FLTT': 'FLTT',
    '_SMAX': 'SMAX',
    '_PDMX': 'PDMX',
    '_NEF': 'NEF',
    '_NTF': 'NTF',
  };
  return labels.entries
          .where((entry) => parameterName.endsWith(entry.key))
          .firstOrNull
          ?.value ??
      parameterName.replaceAll('_', ' ');
}

String _pidFieldLabel(String parameterName) {
  const labels = <String, String>{
    '_P': '比例 P',
    '_I': '积分 I',
    '_D': '微分 D',
    '_FF': '前馈 FF',
    '_IMAX': '积分上限 IMAX',
    '_ILMI': '积分最小 ILMI',
    '_FLTD': 'D 项滤波 FLTD',
    '_FLTE': '误差滤波 FLTE',
    '_FLTT': '目标滤波 FLTT',
    '_SMAX': '输出变化上限 SMAX',
    '_PDMX': 'PD 输出上限 PDMX',
    '_NEF': '误差陷波 NEF',
    '_NTF': '目标陷波 NTF',
  };
  return labels.entries
          .where((entry) => parameterName.endsWith(entry.key))
          .firstOrNull
          ?.value ??
      _pidControlLabel(parameterName);
}

String _pidControlLabel(String parameterName) {
  if (parameterName.contains('_ANG_')) return '姿态角响应';
  if (parameterName.contains('_RAT_') || parameterName.contains('_RATE_')) {
    return '角速度响应';
  }
  if (parameterName.contains('POS')) return '位置控制';
  if (parameterName.contains('VEL')) return '速度控制';
  if (parameterName.contains('ACC')) return '加速度限制';
  if (parameterName.contains('ANGLE_MAX')) return '最大倾角';
  if (parameterName.contains('RATE_MAX')) return '最大角速度';
  if (parameterName.contains('INPUT_TC')) return '输入响应时间';
  if (parameterName.contains('THR_MIX')) return '推力混合';
  if (parameterName.contains('LAND_')) return '着陆响应';
  return parameterName.replaceAll('_', ' ');
}
