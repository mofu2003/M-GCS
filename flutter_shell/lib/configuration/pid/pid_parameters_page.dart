import 'package:flutter/material.dart';

import 'package:macos_ui/macos_ui.dart';

import 'package:mgcs_frontend/connection/connection_controller.dart';
import 'package:mgcs_frontend/theme/mg_typography.dart';

import '../widgets/page_data_notice.dart';
import 'models/pid_models.dart';
import 'widgets/pid_card_grid.dart';
import 'widgets/pid_section_header.dart';

/// PID 参数页，管理基础、扩展层级与参数元数据加载。
class PidParametersPage extends StatefulWidget {
  const PidParametersPage({
    super.key,
    required this.controller,
    required this.profile,
    this.vtol = false,
  });

  final ConnectionController controller;
  final VehicleControlProfile profile;
  final bool vtol;

  @override
  State<PidParametersPage> createState() => _PidParametersPageState();
}

class _PidParametersPageState extends State<PidParametersPage> {
  final ScrollController _scrollController = ScrollController();
  late final MacosTabController _levels = MacosTabController(
    initialIndex: 0,
    length: 2,
  );
  int _levelIndex = 0;

  PidLevel get _level => _levelIndex == 0 ? PidLevel.basic : PidLevel.extended;

  final Set<String> _registeredNames = <String>{};

  @override
  void initState() {
    super.initState();
    _levels.addListener(_handleLevelChanged);
    widget.controller.ensureParamRoles();
    widget.controller.parameterChanges.addListener(_syncVisibleParameters);
    widget.controller.profileChanges.addListener(_syncVisibleParameters);
    _syncVisibleParameters();
  }

  void _handleLevelChanged() {
    final index = _levels.index;
    if (!mounted || index == _levelIndex) return;
    setState(() => _levelIndex = index);
  }

  @override
  void dispose() {
    widget.controller.parameterChanges.removeListener(_syncVisibleParameters);
    widget.controller.profileChanges.removeListener(_syncVisibleParameters);
    _levels.removeListener(_handleLevelChanged);
    _levels.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(covariant PidParametersPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.parameterChanges.removeListener(_syncVisibleParameters);
      oldWidget.controller.profileChanges.removeListener(_syncVisibleParameters);
      widget.controller.parameterChanges.addListener(_syncVisibleParameters);
      widget.controller.profileChanges.addListener(_syncVisibleParameters);
      _registeredNames.clear();
      widget.controller.ensureParamRoles();
    }
    if (oldWidget.profile != widget.profile || oldWidget.vtol != widget.vtol) {
      _syncVisibleParameters();
    }
  }

  /// 两个层级一次全部登记，切换档位时不再往返后端补元数据。
  ///
  /// 参数名要先经角色表解析：角色表异步到位，所以既在初始化时登记一次，
  /// 也在角色表/机型变化时把新出现的名字补登记。
  void _syncVisibleParameters() {
    final resolved = <String>{};
    for (final level in PidLevel.values) {
      for (final section in pidSections(widget.profile, widget.vtol, level)) {
        for (final card in section.cards) {
          for (final field in card.fields) {
            final name = widget.controller.roleParam(field.role);
            if (name != null) resolved.add(name);
          }
        }
      }
    }
    final added = resolved.difference(_registeredNames);
    if (added.isEmpty) return;
    _registeredNames.addAll(added);
    widget.controller.loadParameterMetadata(added);
  }

  @override
  Widget build(BuildContext context) => SizedBox.expand(
    // 面板必须撑满标签视图，否则 Stack 会垂直居中面板，在标签卡下方留出空档。
    child: AnimatedBuilder(
      animation: Listenable.merge([
        widget.controller.parameterChanges,
        widget.controller.profileChanges,
      ]),
      builder: (context, _) {
        final controller = widget.controller;
        final profile = controller.controlProfile;
        // 机型未识别时先按多旋翼渲染骨架，参数仍然全部可编辑，避免整页空白。
        final resolved = profile == VehicleControlProfile.unknown
            ? VehicleControlProfile.copter
            : profile;
        final sections = pidSections(resolved, widget.vtol, _level);
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            if (!controller.isConnected)
              Padding(
                padding: const EdgeInsets.fromLTRB(
                  MgMetrics.configTabBodyInset,
                  8,
                  MgMetrics.configTabBodyInset,
                  0,
                ),
                child: PageDataNotice(
                  controller: controller,
                  message: '连接飞控后读取 PID 参数',
                ),
              )
            else if (!controller.parametersLoaded)
              Padding(
                padding: const EdgeInsets.fromLTRB(
                  MgMetrics.configTabBodyInset,
                  8,
                  MgMetrics.configTabBodyInset,
                  0,
                ),
                child: PageDataNotice(
                  controller: controller,
                  message: '正在读取飞控参数…',
                  showProgress: true,
                ),
              )
            else if (profile == VehicleControlProfile.unknown)
              Padding(
                padding: const EdgeInsets.fromLTRB(
                  MgMetrics.configTabBodyInset,
                  8,
                  MgMetrics.configTabBodyInset,
                  0,
                ),
                child: PageDataNotice(
                  controller: controller,
                  message: '暂未识别机型，先按多旋翼显示',
                ),
              ),
            Padding(
              padding: const EdgeInsets.fromLTRB(
                MgMetrics.configTabBodyInset,
                8,
                MgMetrics.configTabBodyInset,
                0,
              ),
              child: Align(
                alignment: Alignment.centerLeft,
                child: MacosSegmentedControl(
                  controller: _levels,
                  tabs: const [
                    MacosTab(label: '基础'),
                    MacosTab(label: '扩展'),
                  ],
                ),
              ),
            ),
            const SizedBox(height: MgMetrics.configHeaderGap),
            Expanded(
              child: SingleChildScrollView(
                controller: _scrollController,
                // 外层标签视图已提供 12pt，这里补足到统一标准的 24pt。
                padding: const EdgeInsets.fromLTRB(
                  MgMetrics.configTabBodyInset,
                  0,
                  MgMetrics.configTabBodyInset,
                  16,
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    for (final section in sections) ...[
                      PidSectionHeader(title: section.title),
                      const SizedBox(height: MgMetrics.configHeaderGap),
                      PidCardGrid(controller: controller, cards: section.cards),
                      const SizedBox(height: MgMetrics.configSectionGap),
                    ],
                  ],
                ),
              ),
            ),
          ],
        );
      },
    ),
  );
}
