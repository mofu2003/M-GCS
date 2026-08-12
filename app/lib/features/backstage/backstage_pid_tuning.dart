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
  String? _selectedDomain;
  _PidTuningSection? _selectedSection;

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
    final domains = [
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
    final selectedDomain =
        domains
            .where((domain) => domain.group.title == _selectedDomain)
            .firstOrNull ??
        domains.firstOrNull;
    final domainParameters = selectedDomain?.parameters ?? const [];
    final sections = _pidSectionsFor(domainParameters);
    final selectedSection = sections.contains(_selectedSection)
        ? _selectedSection!
        : sections.firstOrNull;
    final sectionParameters = selectedSection == null
        ? const <FlightControllerParameter>[]
        : domainParameters
              .where(
                (parameter) => _pidSectionFor(parameter) == selectedSection,
              )
              .toList(growable: false);
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
          _PidDomainPicker(
            domains: domains,
            selectedTitle: selectedDomain?.group.title,
            onSelected: (domain) => setState(() {
              _selectedDomain = domain.group.title;
              _selectedSection = null;
            }),
          ),
          const SizedBox(height: 14),
          if (selectedDomain != null) ...[
            _CalibrationSectionHeader(
              title: selectedDomain.group.title,
              subtitle: selectedDomain.group.subtitle,
            ),
            const SizedBox(height: 12),
            _PidSectionPicker(
              sections: sections,
              selected: selectedSection,
              onSelected: (section) =>
                  setState(() => _selectedSection = section),
            ),
            const SizedBox(height: 16),
            if (selectedSection != null)
              _PidTuningSectionView(
                group: selectedDomain.group,
                section: selectedSection,
                controller: widget.controller,
                parameters: sectionParameters,
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

class _PidCategorizedGroup {
  const _PidCategorizedGroup({required this.group, required this.parameters});

  final _PidGroup group;
  final List<FlightControllerParameter> parameters;
}

enum _PidTuningSection {
  gains,
  integrator,
  filters,
  protection,
  limits,
  advanced,
}

List<_PidTuningSection> _pidSectionsFor(
  List<FlightControllerParameter> parameters,
) {
  final sections = parameters
      .map(_pidSectionFor)
      .toSet()
      .toList(growable: false);
  sections.sort(
    (left, right) => _pidSectionOrder(left).compareTo(_pidSectionOrder(right)),
  );
  return sections;
}

_PidTuningSection _pidSectionFor(FlightControllerParameter parameter) {
  final name = parameter.name;
  if (name.endsWith('_IMAX') || name.endsWith('_ILMI')) {
    return _PidTuningSection.integrator;
  }
  if (name.endsWith('_FLTD') ||
      name.endsWith('_FLTE') ||
      name.endsWith('_FLTT') ||
      name.endsWith('_NEF') ||
      name.endsWith('_NTF')) {
    return _PidTuningSection.filters;
  }
  if (name.endsWith('_SMAX') || name.endsWith('_PDMX')) {
    return _PidTuningSection.protection;
  }
  if (RegExp(r'_(P|I|D|FF)$').hasMatch(name)) {
    return _PidTuningSection.gains;
  }
  if (name.contains('ANGLE_MAX') ||
      name.contains('RATE_MAX') ||
      name.contains('_ACC_') ||
      name.contains('JERK') ||
      name.contains('INPUT_TC') ||
      name.contains('THR_MIX') ||
      name.contains('LAND_')) {
    return _PidTuningSection.limits;
  }
  return _PidTuningSection.advanced;
}

int _pidSectionOrder(_PidTuningSection section) => switch (section) {
  _PidTuningSection.gains => 0,
  _PidTuningSection.integrator => 1,
  _PidTuningSection.filters => 2,
  _PidTuningSection.protection => 3,
  _PidTuningSection.limits => 4,
  _PidTuningSection.advanced => 5,
};

String _pidSectionLabel(_PidTuningSection section) => switch (section) {
  _PidTuningSection.gains => 'PID 增益',
  _PidTuningSection.integrator => '积分管理',
  _PidTuningSection.filters => '滤波',
  _PidTuningSection.protection => '输出保护',
  _PidTuningSection.limits => '限制与响应',
  _PidTuningSection.advanced => '高级参数',
};

String _pidSectionSubtitle(_PidTuningSection section) => switch (section) {
  _PidTuningSection.gains => '比例、积分、微分与前馈增益。',
  _PidTuningSection.integrator => '限制积分累积，避免持续误差导致输出饱和。',
  _PidTuningSection.filters => '分别处理目标、误差、D 项和陷波频率。',
  _PidTuningSection.protection => '限制瞬时输出变化和 P/D 合成输出。',
  _PidTuningSection.limits => '速度、加速度、角度、输入响应与工作边界。',
  _PidTuningSection.advanced => '固件报告的扩展控制参数。',
};

class _PidDomainPicker extends StatelessWidget {
  const _PidDomainPicker({
    required this.domains,
    required this.selectedTitle,
    required this.onSelected,
  });

  final List<_PidCategorizedGroup> domains;
  final String? selectedTitle;
  final ValueChanged<_PidCategorizedGroup> onSelected;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: const Color(0xfff2f2f7),
      borderRadius: BorderRadius.circular(8),
    ),
    child: Padding(
      padding: const EdgeInsets.all(4),
      child: Wrap(
        spacing: 4,
        runSpacing: 4,
        children: [
          for (final domain in domains)
            _PidDomainTile(
              domain: domain,
              selected: selectedTitle == domain.group.title,
              onPressed: () => onSelected(domain),
            ),
        ],
      ),
    ),
  );
}

class _PidDomainTile extends StatelessWidget {
  const _PidDomainTile({
    required this.domain,
    required this.selected,
    required this.onPressed,
  });

  final _PidCategorizedGroup domain;
  final bool selected;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => cupertino.CupertinoButton(
    padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 8),
    onPressed: onPressed,
    borderRadius: BorderRadius.circular(6),
    color: selected ? Colors.white : Colors.transparent,
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(
          _pidDomainIcon(domain.group.title),
          color: selected ? MacGlassTokens.blue : MacGlassTokens.muted,
          size: 16,
        ),
        const SizedBox(width: 7),
        Text(
          domain.group.title,
          style: MacGlassTypography.control.copyWith(
            color: selected ? MacGlassTokens.blue : MacGlassTokens.ink,
          ),
        ),
      ],
    ),
  );
}

IconData _pidDomainIcon(String title) => switch (title) {
  '姿态外环' => Icons.threed_rotation_rounded,
  '角速度内环' || '固定翼轴速率' => Icons.speed_rounded,
  '角速度滤波与保护' => Icons.filter_alt_outlined,
  '水平位置与速度' => Icons.navigation_rounded,
  '高度与爬升速度' => Icons.height_rounded,
  '限制与响应' => Icons.tune_rounded,
  _ => Icons.settings_input_component_rounded,
};

class _PidSectionPicker extends StatelessWidget {
  const _PidSectionPicker({
    required this.sections,
    required this.selected,
    required this.onSelected,
  });

  final List<_PidTuningSection> sections;
  final _PidTuningSection? selected;
  final ValueChanged<_PidTuningSection> onSelected;

  @override
  Widget build(BuildContext context) {
    if (sections.length < 2) return const SizedBox.shrink();
    return SizedBox(
      height: 34,
      child: cupertino.CupertinoSlidingSegmentedControl<_PidTuningSection>(
        groupValue: selected,
        padding: const EdgeInsets.all(2),
        backgroundColor: const Color(0xffe9e9ee),
        thumbColor: Colors.white,
        children: {
          for (final section in sections)
            section: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 5),
              child: Text(
                _pidSectionLabel(section),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
        },
        onValueChanged: (value) {
          if (value != null) onSelected(value);
        },
      ),
    );
  }
}

class _PidTuningSectionView extends StatelessWidget {
  const _PidTuningSectionView({
    required this.group,
    required this.section,
    required this.controller,
    required this.parameters,
    required this.onWrite,
  });

  final _PidGroup group;
  final _PidTuningSection section;
  final ParameterController controller;
  final List<FlightControllerParameter> parameters;
  final Future<bool> Function(String name, double value) onWrite;

  @override
  Widget build(BuildContext context) {
    final cards = _pidCardsFor(group, parameters);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _CalibrationSectionHeader(
          title: _pidSectionLabel(section),
          subtitle: _pidSectionSubtitle(section),
        ),
        const SizedBox(height: 10),
        LayoutBuilder(
          builder: (context, constraints) {
            final rows = constraints.maxWidth < 710
                ? [
                    for (final card in cards)
                      [_PidCardLayout(card: card, span: 3)],
                  ]
                : _pidCardRows(cards);
            return Column(
              children: [
                for (var rowIndex = 0; rowIndex < rows.length; rowIndex++) ...[
                  if (rowIndex > 0) const SizedBox(height: 12),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      for (
                        var cardIndex = 0;
                        cardIndex < rows[rowIndex].length;
                        cardIndex++
                      ) ...[
                        if (cardIndex > 0) const SizedBox(width: 12),
                        Expanded(
                          flex: rows[rowIndex][cardIndex].span,
                          child: _PidControllerCard(
                            card: rows[rowIndex][cardIndex].card,
                            controller: controller,
                            enabled: controller.canEdit,
                            onWrite: onWrite,
                          ),
                        ),
                      ],
                    ],
                  ),
                ],
              ],
            );
          },
        ),
      ],
    );
  }
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

class _PidCardLayout {
  const _PidCardLayout({required this.card, required this.span});

  final _PidControllerCardModel card;
  final int span;
}

List<List<_PidCardLayout>> _pidCardRows(List<_PidControllerCardModel> cards) {
  if (cards.isEmpty) return const [];
  final rows = <List<_PidCardLayout>>[];
  var current = <_PidCardLayout>[];
  var used = 0;
  for (var index = 0; index < cards.length; index++) {
    final card = cards[index];
    final span = _pidCardSpan(card, index, cards.length);
    if (span == 3) {
      if (current.isNotEmpty) rows.add(current);
      rows.add([_PidCardLayout(card: card, span: 3)]);
      current = <_PidCardLayout>[];
      used = 0;
      continue;
    }
    if (used + span > 3) {
      rows.add(current);
      current = <_PidCardLayout>[];
      used = 0;
    }
    current.add(_PidCardLayout(card: card, span: span));
    used += span;
    if (used == 3) {
      rows.add(current);
      current = <_PidCardLayout>[];
      used = 0;
    }
  }
  if (current.isNotEmpty) rows.add(current);
  return rows;
}

int _pidCardSpan(_PidControllerCardModel card, int index, int count) {
  if (count == 1) return 3;
  if (count == 2) return index == 0 ? 2 : 1;
  if (card.parameters.length >= 9) return 3;
  if (card.parameters.length >= 5) return 2;
  return 1;
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
      color: const Color(0xfff7f7f9),
      border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.10)),
      borderRadius: BorderRadius.circular(8),
    ),
    child: Padding(
      padding: const EdgeInsets.fromLTRB(16, 13, 16, 16),
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
              Icon(card.icon, size: 16, color: MacGlassTokens.muted),
            ],
          ),
          const SizedBox(height: 10),
          Divider(height: 1, color: MacGlassTokens.ink.withValues(alpha: 0.08)),
          const SizedBox(height: 12),
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
  });

  final FlightControllerParameter parameter;
  final ParameterDescriptor? descriptor;
  final bool enabled;
  final Future<bool> Function(String name, double value) onWrite;

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
    _writing = true;
    try {
      await widget.onWrite(widget.parameter.name, value);
    } finally {
      _writing = false;
    }
  }

  @override
  Widget build(BuildContext context) {
    final label = _pidFieldLabel(widget.parameter.name);
    final unit = widget.descriptor?.unit ?? '';
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
        SizedBox(
          height: 38,
          child: cupertino.CupertinoTextField(
            key: ValueKey('mg-pid-${widget.parameter.name}'),
            controller: _controller,
            focusNode: _focusNode,
            enabled: widget.enabled,
            textAlign: TextAlign.center,
            keyboardType: const TextInputType.numberWithOptions(
              decimal: true,
              signed: true,
            ),
            style: MacGlassTypography.value.copyWith(fontFamily: 'monospace'),
            suffix: unit.isEmpty
                ? null
                : Padding(
                    padding: const EdgeInsets.only(right: 7),
                    child: Text(
                      unit,
                      style: const TextStyle(
                        color: MacGlassTokens.muted,
                        fontSize: 10,
                      ),
                    ),
                  ),
            padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 9),
            decoration: BoxDecoration(
              color: const Color(0xfff2f2f7),
              border: Border.all(color: const Color(0xffc7c7cc)),
              borderRadius: BorderRadius.circular(7),
            ),
            onSubmitted: (_) => unawaited(_commit()),
          ),
        ),
      ],
    );
  }
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
