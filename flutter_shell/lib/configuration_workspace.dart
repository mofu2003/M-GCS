import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'widgets/aligned_field_row.dart';
import 'widgets/adaptive_popup_button.dart';
import 'widgets/mac_panel.dart';
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
  });

  final List<String> labels;
  final List<Widget> children;

  @override
  State<_MacTabSurface> createState() => _MacTabSurfaceState();
}

class _MacTabSurfaceState extends State<_MacTabSurface> {
  late final MacosTabController _tabs = MacosTabController(
    initialIndex: 0,
    length: widget.labels.length,
  );

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
    _tabs.dispose();
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
      return MacosWindow(
        sidebar: Sidebar(
          minWidth: 190,
          maxWidth: 270,
          startWidth: 220,
          top: const Padding(
            padding: EdgeInsets.fromLTRB(12, 8, 12, 8),
            child: MacosSearchField(placeholder: '搜索配置'),
          ),
          builder: (context, scrollController) => SidebarItems(
            scrollController: scrollController,
            currentIndex: selectedIndex,
            onChanged: (index) =>
                setState(() => _selectedPageId = pages[index].id),
            items: [
              for (final page in pages)
                SidebarItem(
                  leading: MacosIcon(page.icon),
                  label: Text(
                    page.label,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    // 固定行高：中英混排时字体回退会改变段落高度，进而在
                    // 仅重绘（不改布局）的帧上触发 Flutter 的 TextPainter 断言。
                    strutStyle: const StrutStyle(
                      fontSize: 11,
                      height: 1.3,
                      forceStrutHeight: true,
                    ),
                  ),
                ),
            ],
          ),
        ),
        child: MacosScaffold(
          toolBar: ToolBar(
            title: Text(
              pages[selectedIndex].label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
            actions: [
              ToolBarIconButton(
                label: '返回前台地图',
                icon: const MacosIcon(CupertinoIcons.map, size: 17),
                showLabel: true,
                onPressed: widget.onReturnToMap,
              ),
            ],
          ),
          children: [
            ContentArea(
              builder: (context, scrollController) => _buildSelectedPage(
                context,
                scrollController,
                pageId: selectedPageId,
              ),
            ),
          ],
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
    'arming' => const _ArmingChecksPage(),
    'allParameters' => _AllParametersPage(
      scrollController: scrollController,
      controller: widget.connectionController,
    ),
    'system' => _SystemStatusPage(controller: widget.connectionController),
    'calibration' => const _CalibrationPage(),
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
  const _CalibrationPage();

  @override
  State<_CalibrationPage> createState() => _CalibrationPageState();
}

class _CalibrationPageState extends State<_CalibrationPage> {
  late final MacosTabController _controller = MacosTabController(
    initialIndex: 0,
    length: 4,
  );

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => const _MacTabSurface(
    labels: ['加速度校准', '磁力计校准', '空速计校准', '遥控器校准'],
    children: [
      SizedBox.expand(),
      SizedBox.expand(),
      SizedBox.expand(),
      SizedBox.expand(),
    ],
  );
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

  @override
  void initState() {
    super.initState();
    _loadVisibleParameters();
  }

  @override
  void dispose() {
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

  void _loadVisibleParameters() {
    final names = switch ((widget.profile, widget.vtol)) {
      (VehicleControlProfile.copter, false) => [
        for (final axis in const ['RLL', 'PIT', 'YAW']) ...[
          for (final term in const ['P', 'I', 'D', 'FF'])
            'ATC_RAT_${axis}_$term',
          'ATC_ANG_${axis}_P',
        ],
        for (final prefix in const ['PSC_VELXY', 'PSC_VELZ'])
          for (final term in const ['P', 'I', 'D', 'FF']) '${prefix}_$term',
        'PSC_POSXY_P',
        'PSC_ACCZ_P',
      ],
      (VehicleControlProfile.copter, true) ||
      (VehicleControlProfile.planeVtol, true) => [
        for (final axis in const ['RLL', 'PIT', 'YAW']) ...[
          for (final term in const ['P', 'I', 'D', 'FF'])
            'Q_A_RAT_${axis}_$term',
          'Q_A_ANG_${axis}_P',
        ],
        for (final prefix in const ['Q_P_VELXY', 'Q_P_VELZ'])
          for (final term in const ['P', 'I', 'D', 'FF']) '${prefix}_$term',
        'Q_P_POSXY_P',
      ],
      (VehicleControlProfile.fixedWing, _) ||
      (VehicleControlProfile.planeVtol, false) => [
        for (final axis in const ['RLL_RATE', 'PTCH_RATE', 'YAW_RATE'])
          for (final term in const ['P', 'I', 'D', 'FF']) '${axis}_$term',
        'TECS_TIME_CONST',
        'TECS_SPDWEIGHT',
        'TECS_INTEG_GAIN',
        'TECS_THR_DAMP',
        'TECS_PTCH_DAMP',
      ],
      _ => const <String>[],
    };
    if (names.isEmpty) return;
    widget.controller.loadParameterMetadata(names);
  }

  @override
  Widget build(BuildContext context) => SizedBox.expand(
    // 面板必须撑满标签视图，否则 Stack 会垂直居中面板，在标签卡下方留出空档。
    child: AnimatedBuilder(
      animation: widget.controller.parameterChanges,
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
        return _buildPidContent(context, controller, profile);
      },
    ),
  );

  Widget _buildPidContent(
    BuildContext context,
    ConnectionController controller,
    VehicleControlProfile profile,
  ) {
    final isCopter = profile == VehicleControlProfile.copter;
    final isVtolPage = widget.vtol;
    final planeFixedWing =
        profile == VehicleControlProfile.fixedWing ||
        (profile == VehicleControlProfile.planeVtol && !isVtolPage);
    final axisPrefix = isVtolPage ? 'Q_A_RAT' : 'ATC_RAT';
    final axes = isCopter || isVtolPage
        ? [
            ('横滚', '${axisPrefix}_RLL'),
            ('俯仰', '${axisPrefix}_PIT'),
            ('偏航', '${axisPrefix}_YAW'),
          ]
        : [('横滚', 'RLL_RATE'), ('俯仰', 'PTCH_RATE'), ('偏航', 'YAW_RATE')];
    final heading = isVtolPage
        ? 'VTOL 姿态速率'
        : planeFixedWing
        ? '固定翼姿态速率'
        : '多旋翼姿态速率';

    return SingleChildScrollView(
      controller: _scrollController,
      // 外层 MacosTabView 已提供 12pt，这里补足到统一标准的 24pt 左右边距。
      padding: const EdgeInsets.fromLTRB(_tabBodyInset, 8, _tabBodyInset, 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _PidSectionHeader(title: heading),
          const SizedBox(height: _headerGap),
          _PidAxisGrid(axes: axes, controller: controller),
          if (!planeFixedWing) ...[
            const SizedBox(height: _sectionGap),
            _PidSectionHeader(title: isVtolPage ? 'VTOL 角度响应' : '姿态角响应'),
            const SizedBox(height: _headerGap),
            _PidGainGrid(
              controller: controller,
              fields: [
                _PidGainField(
                  '横滚 P',
                  isVtolPage ? 'Q_A_ANG_RLL_P' : 'ATC_ANG_RLL_P',
                ),
                _PidGainField(
                  '俯仰 P',
                  isVtolPage ? 'Q_A_ANG_PIT_P' : 'ATC_ANG_PIT_P',
                ),
                _PidGainField(
                  '偏航 P',
                  isVtolPage ? 'Q_A_ANG_YAW_P' : 'ATC_ANG_YAW_P',
                ),
              ],
            ),
          ],
          if (planeFixedWing) ...[
            const SizedBox(height: _sectionGap),
            _PidSectionHeader(title: 'TECS 能量控制'),
            const SizedBox(height: _headerGap),
            _PidGainGrid(
              controller: controller,
              fields: const [
                _PidGainField('时间常数', 'TECS_TIME_CONST'),
                _PidGainField('速度权重', 'TECS_SPDWEIGHT'),
                _PidGainField('积分增益', 'TECS_INTEG_GAIN'),
                _PidGainField('油门阻尼', 'TECS_THR_DAMP'),
                _PidGainField('俯仰阻尼', 'TECS_PTCH_DAMP'),
              ],
            ),
          ] else ...[
            const SizedBox(height: _sectionGap),
            _PidSectionHeader(title: '位置与速度'),
            const SizedBox(height: _headerGap),
            _PidGainGrid(
              controller: controller,
              fields: [
                _PidGainField(
                  '水平位置 P',
                  isVtolPage ? 'Q_P_POSXY_P' : 'PSC_POSXY_P',
                ),
                _PidGainField(
                  '水平速度 P',
                  isVtolPage ? 'Q_P_VELXY_P' : 'PSC_VELXY_P',
                ),
                _PidGainField(
                  '水平速度 I',
                  isVtolPage ? 'Q_P_VELXY_I' : 'PSC_VELXY_I',
                ),
                _PidGainField(
                  '垂直速度 P',
                  isVtolPage ? 'Q_P_VELZ_P' : 'PSC_VELZ_P',
                ),
                _PidGainField(
                  '垂直速度 I',
                  isVtolPage ? 'Q_P_VELZ_I' : 'PSC_VELZ_I',
                ),
                if (isCopter) const _PidGainField('垂直加速度 P', 'PSC_ACCZ_P'),
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _PidGainField {
  const _PidGainField(this.label, this.parameterName);

  final String label;
  final String parameterName;
}

class _PidSectionHeader extends StatelessWidget {
  const _PidSectionHeader({required this.title});

  final String title;

  @override
  Widget build(BuildContext context) =>
      Text(title, style: MacosTheme.of(context).typography.title3);
}

class _PidAxisGrid extends StatelessWidget {
  const _PidAxisGrid({required this.axes, required this.controller});

  final List<(String, String)> axes;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 1050
          ? 3
          : constraints.maxWidth >= 680
          ? 2
          : 1;
      final width = (constraints.maxWidth - (columns - 1) * 12) / columns;
      return Wrap(
        spacing: _cardGap,
        runSpacing: _cardGap,
        children: [
          for (final (axis, prefix) in axes)
            SizedBox(
              width: width,
              child: _PidAxisCard(
                axis: axis,
                prefix: prefix,
                controller: controller,
              ),
            ),
        ],
      );
    },
  );
}

class _PidAxisCard extends StatelessWidget {
  const _PidAxisCard({
    required this.axis,
    required this.prefix,
    required this.controller,
  });

  final String axis;
  final String prefix;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final brightness = MacosTheme.brightnessOf(context);
    const terms = ['P', 'I', 'D', 'FF'];
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: brightness.resolve(
          const Color(0xFFFFFFFF),
          const Color(0xFF26272B),
        ),
        border: Border.all(color: theme.dividerColor),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(axis, style: theme.typography.headline),
          const SizedBox(height: 10),
          Row(
            children: [
              for (final term in terms)
                Expanded(
                  child: Padding(
                    padding: EdgeInsets.only(right: term == 'FF' ? 0 : 6),
                    child: Text(
                      term,
                      textAlign: TextAlign.center,
                      style: theme.typography.caption1.copyWith(
                        color: MacosColors.secondaryLabelColor,
                      ),
                    ),
                  ),
                ),
            ],
          ),
          const SizedBox(height: 4),
          Row(
            children: [
              for (final term in terms)
                Expanded(
                  child: Padding(
                    padding: EdgeInsets.only(right: term == 'FF' ? 0 : 6),
                    child: _ParameterValueControl(
                      parameter: controller.parameters['${prefix}_$term'],
                      controller: controller,
                      center: true,
                    ),
                  ),
                ),
            ],
          ),
        ],
      ),
    );
  }
}

class _PidGainGrid extends StatelessWidget {
  const _PidGainGrid({required this.controller, required this.fields});

  final ConnectionController controller;
  final List<_PidGainField> fields;

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final columns = constraints.maxWidth >= 1000
          ? 4
          : constraints.maxWidth >= 640
          ? 3
          : 2;
      final width = (constraints.maxWidth - (columns - 1) * 10) / columns;
      return Wrap(
        spacing: _cardGap,
        runSpacing: _cardGap,
        children: [
          for (final field in fields)
            SizedBox(
              width: width,
              child: _PidGainTile(field: field, controller: controller),
            ),
        ],
      );
    },
  );
}

class _PidGainTile extends StatelessWidget {
  const _PidGainTile({required this.field, required this.controller});

  final _PidGainField field;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) {
    final theme = MacosTheme.of(context);
    final brightness = MacosTheme.brightnessOf(context);
    final parameter = controller.parameters[field.parameterName];
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        color: brightness.resolve(
          const Color(0xFFFFFFFF),
          const Color(0xFF26272B),
        ),
        border: Border.all(color: theme.dividerColor),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            field.label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.typography.caption1,
          ),
          const SizedBox(height: 5),
          _ParameterValueControl(
            parameter: parameter,
            controller: controller,
            center: true,
          ),
        ],
      ),
    );
  }
}

class _BatteryConfigurationPage extends StatelessWidget {
  const _BatteryConfigurationPage();

  @override
  Widget build(BuildContext context) => const _BlankConfigurationPage();
}

class _ArmingChecksPage extends StatelessWidget {
  const _ArmingChecksPage();

  @override
  Widget build(BuildContext context) => const _BlankConfigurationPage();
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
      final theme = MacosTheme.of(context);
      final brightness = MacosTheme.brightnessOf(context);
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
            if (widget.controller.hasParameterWritesInProgress ||
                widget.controller.parameterWriteError != null) ...[
              _ParameterWriteStatus(controller: widget.controller),
              const SizedBox(height: 10),
            ],
            DecoratedBox(
              decoration: BoxDecoration(
                color: brightness.resolve(
                  const Color(0xFFFFFFFF),
                  const Color(0xFF252629),
                ),
                border: Border.all(color: theme.dividerColor),
                borderRadius: BorderRadius.circular(10),
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(10),
                child: SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: ConstrainedBox(
                    constraints: const BoxConstraints(minWidth: 920),
                    child: Table(
                      columnWidths: const {
                        0: FixedColumnWidth(76),
                        1: FixedColumnWidth(148),
                        2: FixedColumnWidth(90),
                        3: FlexColumnWidth(1.5),
                        4: FixedColumnWidth(132),
                        5: FixedColumnWidth(132),
                        6: FixedColumnWidth(132),
                      },
                      children: [
                        TableRow(
                          decoration: BoxDecoration(
                            color: brightness.resolve(
                              const Color(0xFFF0F6FC),
                              const Color(0xFF2A3542),
                            ),
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
                                  color: theme.dividerColor,
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
    height: 48,
    child: Center(
      child: Container(
        width: 24,
        height: 24,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: MacosColors.systemBlueColor.withValues(alpha: 0.12),
          shape: BoxShape.circle,
          border: Border.all(
            color: MacosColors.systemBlueColor.withValues(alpha: 0.35),
          ),
        ),
        child: Text(
          '$number',
          style: const TextStyle(
            color: MacosColors.systemBlueColor,
            fontSize: 11,
            fontWeight: FontWeight.w600,
          ),
        ),
      ),
    ),
  );
}

class _LiveOutputCell extends StatelessWidget {
  const _LiveOutputCell({required this.value});

  final int? value;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: 48,
    child: Center(
      child: _ValueWell(
        text: value?.toString() ?? '—',
        enabled: false,
        center: true,
      ),
    ),
  );
}

class _ReverseCell extends StatelessWidget {
  const _ReverseCell({required this.parameter, required this.controller});

  final VehicleParameter? parameter;
  final ConnectionController controller;

  @override
  Widget build(BuildContext context) => SizedBox(
    height: 48,
    child: Center(
      child: parameter == null
          ? Text('—', style: MacosTheme.of(context).typography.body)
          : MacosCheckbox(
              value: parameter!.value != 0,
              onChanged: parameter!.readOnly || !controller.isConnected
                  ? null
                  : (value) => unawaited(
                      controller.setParameter(parameter!.name, value ? 1 : 0),
                    ),
              size: 18,
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
    height: 48,
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
    final brightness = MacosTheme.brightnessOf(context);
    final textColor = brightness.resolve(
      const Color(0xFF1D1D1F),
      const Color(0xFFF5F5F7),
    );
    return MacosTextField(
      controller: _textController,
      focusNode: _focusNode,
      enabled: widget.enabled,
      readOnly: !widget.enabled,
      placeholder: widget.text,
      placeholderStyle: TextStyle(color: textColor),
      style: TextStyle(color: textColor, fontWeight: FontWeight.w600),
      textAlign: widget.center ? TextAlign.center : TextAlign.start,
      keyboardType: const TextInputType.numberWithOptions(
        decimal: true,
        signed: true,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color:
            widget.fillColor ??
            brightness.resolve(
              const Color(0xFFF5F5F7),
              const Color(0xFF303136),
            ),
        borderRadius: BorderRadius.circular(7),
        border: Border.all(color: MacosTheme.of(context).dividerColor),
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
    final toneColor = switch (tone) {
      _LimitTone.min => MacosColors.systemRedColor,
      _LimitTone.mid => MacosColors.systemBlueColor,
      _LimitTone.max => MacosColors.systemGreenColor,
    };
    final brightness = MacosTheme.brightnessOf(context);
    return SizedBox(
      height: 48,
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
            fillColor: toneColor.withValues(
              alpha: brightness.isDark ? 0.28 : 0.12,
            ),
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
    final error = controller.parameterWriteError;
    final color = error == null
        ? MacosColors.systemBlueColor
        : MacosColors.systemRedColor;
    final message =
        error ?? '正在等待飞控确认 ${controller.pendingParameterWriteCount} 项参数变更…';
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2, vertical: 3),
      child: Row(
        children: [
          MacosIcon(
            error == null
                ? CupertinoIcons.arrow_2_circlepath
                : CupertinoIcons.exclamationmark_circle,
            size: 14,
            color: color,
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Text(
              message,
              style: MacosTheme.of(context).typography.caption1.copyWith(
                color: error == null ? MacosColors.secondaryLabelColor : color,
              ),
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
      final theme = MacosTheme.of(context);
      final brightness = MacosTheme.brightnessOf(context);
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
                  brightness: brightness,
                  dividerColor: theme.dividerColor,
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
    final brightness = MacosTheme.brightnessOf(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: brightness.resolve(
          const Color(0xFFF7F7F9),
          const Color(0xFF26272B),
        ),
        border: Border.all(color: theme.dividerColor),
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
    required this.brightness,
    required this.dividerColor,
  });

  final int channel;
  final VehicleParameter? parameter;
  final ConnectionController controller;
  final Brightness brightness;
  final Color dividerColor;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: brightness.resolve(
        const Color(0xFFF7F7F9),
        const Color(0xFF26272B),
      ),
      border: Border.all(color: dividerColor),
      borderRadius: BorderRadius.circular(10),
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
      style: const TextStyle(
        color: MacosColors.systemBlueColor,
        fontWeight: FontWeight.w700,
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
                      color: MacosColors.systemRedColor,
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
                      color: MacosColors.secondaryLabelColor,
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
          ).typography.callout.copyWith(color: MacosColors.secondaryLabelColor),
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
    final brightness = MacosTheme.brightnessOf(context);
    return Container(
      margin: const EdgeInsets.only(bottom: 6),
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: brightness.resolve(
          const Color(0xFFFFFFFF),
          const Color(0xFF252629),
        ),
        border: Border.all(color: theme.dividerColor),
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
                  style: theme.typography.caption1.copyWith(
                    color: MacosColors.secondaryLabelColor,
                    fontFamily: 'Menlo',
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
            if (controller.hasParameterWritesInProgress ||
                controller.parameterWriteError != null) ...[
              _ParameterWriteStatus(controller: controller),
              const SizedBox(height: 10),
            ],
            if (controller.serialHardwareMappingLoading)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '正在读取飞控硬件串口映射…',
                  style: theme.typography.caption1.copyWith(
                    color: MacosColors.secondaryLabelColor,
                  ),
                ),
              )
            else if (controller.serialHardwareMappingError case final error?)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  '飞控未提供硬件串口映射：$error',
                  style: theme.typography.caption1.copyWith(
                    color: MacosColors.secondaryLabelColor,
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
                          dividerColor: theme.dividerColor,
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
    final brightness = MacosTheme.brightnessOf(context);
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
        color: brightness.resolve(
          const Color(0xFFF7F7F9),
          const Color(0xFF26272B),
        ),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: widget.dividerColor),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.06),
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
                  style: theme.typography.title3.copyWith(
                    fontFamily: 'Menlo',
                    fontWeight: FontWeight.w700,
                  ),
                ),
                if (widget.hardwareName case final hardwareName?) ...[
                  const SizedBox(width: 10),
                  Flexible(
                    child: Text(
                      hardwareName,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                        color: MacosColors.systemBlueColor,
                        fontWeight: FontWeight.w600,
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
                  color: MacosColors.secondaryLabelColor,
                ),
              ),
            if (extraParameters.isNotEmpty) ...[
              const SizedBox(height: 8),
              Divider(height: 1, color: theme.dividerColor),
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
                style: TextStyle(color: _adaptiveLabelColor(context)),
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
                  color: Colors.transparent,
                  child: InkWell(
                    onTap: toggleGroup,
                    borderRadius: BorderRadius.circular(7),
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
                                  color: MacosColors.secondaryLabelColor,
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
      final theme = MacosTheme.of(context);
      final brightness = MacosTheme.brightnessOf(context);
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
              background: brightness.resolve(
                const Color(0xFFF7F7F9),
                const Color(0xFF26272B),
              ),
              borderColor: theme.dividerColor,
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
                borderColor: theme.dividerColor,
                background: brightness.resolve(
                  const Color(0xFFFFFFFF),
                  const Color(0xFF252629),
                ),
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
                            borderColor: theme.dividerColor,
                            background: brightness.resolve(
                              const Color(0xFFFFFFFF),
                              const Color(0xFF252629),
                            ),
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
                        borderColor: theme.dividerColor,
                        background: brightness.resolve(
                          const Color(0xFFFFFFFF),
                          const Color(0xFF252629),
                        ),
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
                        borderColor: theme.dividerColor,
                        background: brightness.resolve(
                          const Color(0xFFFFFFFF),
                          const Color(0xFF252629),
                        ),
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
                        borderColor: theme.dividerColor,
                        background: brightness.resolve(
                          const Color(0xFFFFFFFF),
                          const Color(0xFF252629),
                        ),
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
                    ? MacosColors.systemGreenColor
                    : MacosColors.secondaryLabelColor,
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
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(top: 2),
          child: MacosIcon(icon, size: 17, color: MacosColors.systemBlueColor),
        ),
        const SizedBox(width: 9),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                label,
                style: typography.caption1.copyWith(
                  color: MacosColors.secondaryLabelColor,
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
    final faults = summary!['fault_count'] as int? ?? 0;
    final healthy = summary!['healthy_count'] as int? ?? 0;
    final label = faults > 0 ? '异常 $faults' : '正常 $healthy';
    final color = faults > 0
        ? MacosColors.systemRedColor
        : MacosColors.systemGreenColor;
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
      borderRadius: BorderRadius.circular(10),
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
    final (label, color) = switch (status) {
      'healthy' => ('正常', MacosColors.systemGreenColor),
      'fault' => ('异常', MacosColors.systemRedColor),
      'disabled' => ('未启用', MacosColors.systemOrangeColor),
      'missing' => ('未检测到', MacosColors.secondaryLabelColor),
      _ => ('未知', MacosColors.secondaryLabelColor),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: background,
        border: Border.all(color: borderColor),
        borderRadius: BorderRadius.circular(10),
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
      borderRadius: BorderRadius.circular(10),
    ),
    child: Row(
      children: [
        MacosIcon(icon, size: 21, color: MacosColors.systemBlueColor),
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
                      : MacosColors.secondaryLabelColor,
                  fontWeight: FontWeight.w600,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                detail,
                style: MacosTheme.of(context).typography.caption1.copyWith(
                  color: MacosColors.secondaryLabelColor,
                ),
              ),
            ],
          ),
        ),
      ],
    ),
  );
}

Color _adaptiveLabelColor(BuildContext context) => MacosTheme.brightnessOf(
  context,
).resolve(const Color(0xFF1D1D1F), const Color(0xFFF5F5F7));
