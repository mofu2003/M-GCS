import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import '../../application/connection_controller.dart';
import '../../application/calibration_models.dart';
import '../../application/connection_session_backend.dart';
import '../../application/parameter_controller.dart';
import '../../application/parameter_models.dart';
import '../../application/polling_task.dart';
import '../../ui/skins/mac_glass/mac_glass_menu.dart';
import '../../ui/skins/mac_glass/mac_glass_navigation_dock.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';
import 'parameter_tree.dart';

part 'backstage_calibration.dart';
part 'backstage_parameters.dart';
part 'backstage_pid_tuning.dart';
part 'backstage_remote_control.dart';
part 'backstage_safety_settings.dart';
part 'backstage_serial_ports.dart';
part 'backstage_servo_output.dart';
part 'backstage_shared.dart';

final class BackstagePage extends StatefulWidget {
  const BackstagePage({
    required this.connectionController,
    required this.parameterController,
    required this.onReturnToFrontstage,
    super.key,
  });

  final ConnectionController connectionController;
  final ParameterController? parameterController;
  final VoidCallback onReturnToFrontstage;

  @override
  State<BackstagePage> createState() => _BackstagePageState();
}

final class _BackstagePageState extends State<BackstagePage> {
  final _searchController = TextEditingController();
  String _prefix = '';
  String? _selectedName;
  _ParameterFilter _filter = _ParameterFilter.all;
  MacGlassBackstageSection _section = MacGlassBackstageSection.parameters;
  bool _fileDialogBusy = false;

  static const _parameterFileTypes = <XTypeGroup>[
    XTypeGroup(label: 'ArduPilot 参数文件', extensions: <String>['param', 'parm']),
  ];

  @override
  void initState() {
    super.initState();
    _searchController.addListener(() => setState(() {}));
  }

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  List<FlightControllerParameter> _visibleParameters(
    ParameterController controller,
  ) {
    final query = _searchController.text.trim().toUpperCase();
    final stagedNames = controller.stagedChanges
        .map((change) => change.parameter.name)
        .toSet();
    return controller.parameters
        .where((parameter) {
          if (_prefix.isNotEmpty && !parameter.name.startsWith(_prefix)) {
            return false;
          }
          if (query.isNotEmpty &&
              !parameter.name.toUpperCase().contains(query) &&
              !(controller
                      .descriptorFor(parameter.name)
                      ?.displayName
                      .toUpperCase()
                      .contains(query) ??
                  false) &&
              !(controller
                      .descriptorFor(parameter.name)
                      ?.description
                      .toUpperCase()
                      .contains(query) ??
                  false)) {
            return false;
          }
          return switch (_filter) {
            _ParameterFilter.all => true,
            _ParameterFilter.staged => stagedNames.contains(parameter.name),
          };
        })
        .toList(growable: false);
  }

  Future<void> _reviewAndWrite(ParameterController controller) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) =>
          _ParameterWriteReviewDialog(changes: controller.stagedChanges),
    );
    if (confirmed != true || !mounted) return;
    final result = await controller.writeStaged();
    if (!mounted || !result.hasFailures) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('${result.failed.length} 项参数写入失败，已保留草稿')),
    );
  }

  Future<void> _readParameterFile(ParameterController controller) async {
    if (_fileDialogBusy || !controller.canManageParameterFiles) return;
    setState(() => _fileDialogBusy = true);
    try {
      final file = await openFile(acceptedTypeGroups: _parameterFileTypes);
      if (file == null) return;
      final summary = await controller.importParameterFile(
        await file.readAsString(),
      );
      if (!mounted) return;
      if (summary.staged > 0) {
        _searchController.clear();
        setState(() {
          _prefix = '';
          _selectedName = null;
          _filter = _ParameterFilter.staged;
        });
      }
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            '已读取 ${summary.parsed} 项：${summary.staged} 项待写入，'
            '${summary.unchanged} 项未变化，${summary.unknown} 项未匹配',
          ),
        ),
      );
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('读取参数文件失败：$error')));
    } finally {
      if (mounted) setState(() => _fileDialogBusy = false);
    }
  }

  Future<void> _saveParameterFile(ParameterController controller) async {
    if (_fileDialogBusy || !controller.canManageParameterFiles) return;
    setState(() => _fileDialogBusy = true);
    try {
      final location = await getSaveLocation(
        suggestedName: 'parameters.param',
        acceptedTypeGroups: _parameterFileTypes,
      );
      if (location == null) return;
      final path = _ensureParameterFileExtension(location.path);
      final content = await controller.exportParameterFile();
      await File(path).writeAsString(content, encoding: utf8, flush: true);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('已保存 ${controller.parameters.length} 项参数')),
      );
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('保存参数文件失败：$error')));
    } finally {
      if (mounted) setState(() => _fileDialogBusy = false);
    }
  }

  String _ensureParameterFileExtension(String path) {
    final lower = path.toLowerCase();
    return lower.endsWith('.param') || lower.endsWith('.parm')
        ? path
        : '$path.param';
  }

  @override
  Widget build(BuildContext context) {
    final controller = widget.parameterController;
    return cupertino.CupertinoTheme(
      data: const cupertino.CupertinoThemeData(
        brightness: Brightness.light,
        primaryColor: Color(0xff007aff),
        scaffoldBackgroundColor: Color(0xfff2f2f7),
        barBackgroundColor: Color(0xfff7f7f8),
        textTheme: cupertino.CupertinoTextThemeData(
          textStyle: MacGlassTypography.body,
          actionTextStyle: MacGlassTypography.control,
          tabLabelTextStyle: MacGlassTypography.caption,
          navTitleTextStyle: MacGlassTypography.pageTitle,
        ),
      ),
      child: Scaffold(
        body: DecoratedBox(
          key: const ValueKey('mg-backstage-shell'),
          decoration: const BoxDecoration(color: Color(0xffedf2f5)),
          child: controller == null
              ? _UnavailableBackstage(onReturn: widget.onReturnToFrontstage)
              : ListenableBuilder(
                  listenable: Listenable.merge([
                    controller,
                    widget.connectionController,
                  ]),
                  builder: (context, _) {
                    final visible = _visibleParameters(controller);
                    final selected = controller.parameters
                        .where((item) => item.name == _selectedName)
                        .firstOrNull;
                    return LayoutBuilder(
                      builder: (context, constraints) => _BackstageWorkspace(
                        // Desktop windows may be narrower than 960px. Keep the
                        // navigation tree visible until the screen is genuinely
                        // phone-sized, where a horizontal workspace cannot work.
                        compact: constraints.maxWidth < 720,
                        controller: controller,
                        airframe: widget
                            .connectionController
                            .snapshot
                            ?.bootstrap
                            .identity
                            .airframe,
                        visibleParameters: visible,
                        selectedParameter: selected,
                        selectedPrefix: _prefix,
                        section: _section,
                        searchController: _searchController,
                        filter: _filter,
                        onReturn: widget.onReturnToFrontstage,
                        onSectionChanged: (section) => setState(() {
                          _section = section;
                          _selectedName = null;
                        }),
                        onPrefixSelected: (prefix) => setState(() {
                          _prefix = prefix;
                          _selectedName = null;
                        }),
                        onFilterChanged: (filter) =>
                            setState(() => _filter = filter),
                        onSelected: (parameter) {
                          setState(() => _selectedName = parameter.name);
                          controller.loadDescriptor(parameter.name);
                        },
                        onRead: () => _readParameterFile(controller),
                        onSave: () => _saveParameterFile(controller),
                        onRefresh: controller.refreshFromVehicle,
                        onReviewWrite: () => _reviewAndWrite(controller),
                        fileDialogBusy: _fileDialogBusy,
                      ),
                    );
                  },
                ),
        ),
      ),
    );
  }
}

enum _ParameterFilter { all, staged }

final class _BackstageWorkspace extends StatelessWidget {
  const _BackstageWorkspace({
    required this.compact,
    required this.controller,
    required this.airframe,
    required this.visibleParameters,
    required this.selectedParameter,
    required this.selectedPrefix,
    required this.section,
    required this.searchController,
    required this.filter,
    required this.onReturn,
    required this.onSectionChanged,
    required this.onPrefixSelected,
    required this.onFilterChanged,
    required this.onSelected,
    required this.onRead,
    required this.onSave,
    required this.onRefresh,
    required this.onReviewWrite,
    required this.fileDialogBusy,
  });

  final bool compact;
  final ParameterController controller;
  final String? airframe;
  final List<FlightControllerParameter> visibleParameters;
  final FlightControllerParameter? selectedParameter;
  final String selectedPrefix;
  final MacGlassBackstageSection section;
  final TextEditingController searchController;
  final _ParameterFilter filter;
  final VoidCallback onReturn;
  final ValueChanged<MacGlassBackstageSection> onSectionChanged;
  final ValueChanged<String> onPrefixSelected;
  final ValueChanged<_ParameterFilter> onFilterChanged;
  final ValueChanged<FlightControllerParameter> onSelected;
  final VoidCallback onRead;
  final VoidCallback onSave;
  final VoidCallback onRefresh;
  final VoidCallback onReviewWrite;
  final bool fileDialogBusy;

  @override
  Widget build(BuildContext context) {
    final selected = selectedParameter;
    final tree = _ParameterTree(
      parameters: controller.parameters,
      selectedPrefix: selectedPrefix,
      onSelected: onPrefixSelected,
    );
    final list = _ParameterList(
      parameters: visibleParameters,
      selectedName: selected?.name,
      staged: controller.stagedChanges,
      searchController: searchController,
      descriptorFor: controller.descriptorFor,
      onSelected: onSelected,
    );
    final inspector = _ParameterInspector(
      parameter: selected,
      descriptor: selected == null
          ? null
          : controller.descriptorFor(selected.name),
      staged: controller.stagedChanges
          .where((change) => change.parameter.name == selected?.name)
          .firstOrNull,
      enabled: controller.canEdit,
      onStage: (value) => controller.stageValue(selected!.name, value),
    );
    final pendingBar = _PendingWriteBar(
      count: controller.stagedChanges.length,
      writing: controller.writing,
      onDiscard: controller.discardAll,
      onReviewWrite: onReviewWrite,
    );
    final sectionTitle = switch (section) {
      MacGlassBackstageSection.parameters => '参数配置',
      MacGlassBackstageSection.servoOutputs => '舵机输出',
      MacGlassBackstageSection.serialPorts => '串口配置',
      MacGlassBackstageSection.remoteControl => '遥控配置',
      MacGlassBackstageSection.pidTuning => 'PID 调参',
      MacGlassBackstageSection.calibration => '传感器/外设配置',
      MacGlassBackstageSection.safety => '安全设置',
    };
    final serialPortCount = _serialPortNumbers(controller.parameters).length;
    final sectionSubtitle = switch (section) {
      MacGlassBackstageSection.parameters =>
        '${controller.parameters.length} 个飞控参数',
      MacGlassBackstageSection.servoOutputs => 'SERVO1-16 · 功能、反向和 PWM 范围',
      MacGlassBackstageSection.serialPorts =>
        '$serialPortCount 个可用串口 · 修改后立即写入飞控',
      MacGlassBackstageSection.remoteControl => '飞行模式、通道功能与摇杆校准',
      MacGlassBackstageSection.pidTuning => '根据当前机架自动显示控制器 · 修改后立即写入飞控',
      MacGlassBackstageSection.calibration => 'IMU、罗盘、GPS 与空速计',
      MacGlassBackstageSection.safety => '围栏、解锁检查与电量保护',
    };
    final sectionPage = switch (section) {
      MacGlassBackstageSection.parameters => list,
      MacGlassBackstageSection.servoOutputs => _ServoOutputPage(
        controller: controller,
      ),
      MacGlassBackstageSection.serialPorts => _SerialPortConfigPage(
        controller: controller,
      ),
      MacGlassBackstageSection.remoteControl => _RemoteControlConfigPage(
        controller: controller,
      ),
      MacGlassBackstageSection.pidTuning => _PidTuningPage(
        controller: controller,
        airframe: airframe,
      ),
      MacGlassBackstageSection.calibration => _CalibrationPage(
        controller: controller,
      ),
      MacGlassBackstageSection.safety => _SafetySettingsPage(
        controller: controller,
      ),
    };

    return SafeArea(
      child: Padding(
        padding: EdgeInsets.fromLTRB(
          compact ? 12 : MacGlassTokens.pageInset,
          MacGlassTokens.pageInset,
          MacGlassTokens.pageInset,
          MacGlassTokens.pageInset,
        ),
        child: Column(
          children: [
            _BackstageHeader(
              title: sectionTitle,
              subtitle: sectionSubtitle,
              showActions: section == MacGlassBackstageSection.parameters,
              onRead:
                  section == MacGlassBackstageSection.parameters &&
                      controller.canManageParameterFiles &&
                      !fileDialogBusy
                  ? onRead
                  : null,
              onSave:
                  section == MacGlassBackstageSection.parameters &&
                      controller.canManageParameterFiles &&
                      !fileDialogBusy
                  ? onSave
                  : null,
              onRefresh:
                  section == MacGlassBackstageSection.parameters &&
                      controller.canEdit &&
                      !fileDialogBusy
                  ? onRefresh
                  : null,
              onReviewWrite:
                  section != MacGlassBackstageSection.parameters ||
                      controller.stagedChanges.isEmpty ||
                      !controller.canEdit ||
                      fileDialogBusy
                  ? null
                  : onReviewWrite,
            ),
            const SizedBox(height: 12),
            Expanded(
              child: compact
                  ? Column(
                      children: [
                        Expanded(child: sectionPage),
                        if (section == MacGlassBackstageSection.parameters) ...[
                          const SizedBox(height: 12),
                          SizedBox(height: 230, child: inspector),
                        ],
                        if (controller.stagedChanges.isNotEmpty) ...[
                          const SizedBox(height: 12),
                          pendingBar,
                        ],
                      ],
                    )
                  : Row(
                      children: [
                        SizedBox(
                          width: MacGlassTokens.dockWidth,
                          child: MacGlassBackstageNavigationDock(
                            selectedSection: section,
                            onSectionChanged: onSectionChanged,
                            onReturnToFrontstage: onReturn,
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Column(
                            children: [
                              Expanded(
                                child:
                                    section ==
                                        MacGlassBackstageSection.parameters
                                    ? Row(
                                        children: [
                                          SizedBox(width: 216, child: tree),
                                          const SizedBox(width: 12),
                                          Expanded(child: list),
                                          const SizedBox(width: 12),
                                          SizedBox(
                                            width: 306,
                                            child: inspector,
                                          ),
                                        ],
                                      )
                                    : sectionPage,
                              ),
                              if (controller.stagedChanges.isNotEmpty) ...[
                                const SizedBox(height: 12),
                                pendingBar,
                              ],
                            ],
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
}
