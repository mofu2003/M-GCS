import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import '../../../application/connection_models.dart';
import '../../../application/calibration_models.dart';
import '../../../application/connection_session_backend.dart';
import '../../../application/parameter_models.dart';
import '../../../application/safety_models.dart';
import '../../../application/polling_task.dart';
import '../../ui/skins/mac_glass/mac_glass_menu.dart';
import '../../ui/skins/mac_glass/mac_glass_navigation_dock.dart';
import '../../ui/skins/mac_glass/mac_glass_surface.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';
import '../../ui/skin/mg_skin.dart';
import '../../ui/skins/mac_glass/mac_glass_skin.dart';
import 'parameter_tree.dart';
import '../../../application/vehicle_file_models.dart';
import '../../../l10n/mg_localizations.dart';
import '../../controllers/connection_controller.dart';
import '../../controllers/parameter_controller.dart';
import '../../controllers/safety_controller.dart';

part 'backstage_calibration.dart';
part 'backstage_airframe.dart';
part 'backstage_parameters.dart';
part 'backstage_pid_tuning.dart';
part 'backstage_remote_control.dart';
part 'backstage_safety_settings.dart';
part 'backstage_serial_ports.dart';
part 'backstage_servo_output.dart';
part 'backstage_flight_logs.dart';
part 'backstage_shared.dart';

final class BackstagePage extends StatefulWidget {
  const BackstagePage({
    required this.connectionController,
    required this.parameterController,
    required this.onReturnToFrontstage,
    this.active = true,
    this.skin = const MacGlassSkin(),
    super.key,
  });

  final ConnectionController connectionController;
  final ParameterController? parameterController;
  final VoidCallback onReturnToFrontstage;
  final bool active;
  final MgSkin skin;

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
      SnackBar(
        content: Text(
          mgLocalizationsOf(
            context,
          ).parameterWriteFailures(result.failed.length),
        ),
      ),
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
            mgLocalizationsOf(context).parameterFileReadSummary(
              summary.parsed,
              summary.staged,
              summary.unchanged,
              summary.unknown,
            ),
          ),
        ),
      );
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            mgLocalizationsOf(
              context,
            ).parameterFileReadFailed(error.toString()),
          ),
        ),
      );
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
        SnackBar(
          content: Text(
            mgLocalizationsOf(
              context,
            ).parametersSaved(controller.parameters.length),
          ),
        ),
      );
    } on Object catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            mgLocalizationsOf(
              context,
            ).parameterFileSaveFailed(error.toString()),
          ),
        ),
      );
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
    return Theme(
      data: widget.skin.themeData,
      child: Scaffold(
        body: DecoratedBox(
          key: const ValueKey('mg-backstage-shell'),
          decoration: const BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [
                MacGlassTokens.canvasTop,
                MacGlassTokens.canvasMiddle,
                MacGlassTokens.canvasBottom,
              ],
            ),
          ),
          child: Stack(
            fit: StackFit.expand,
            children: [
              IgnorePointer(child: _BackstageAtmosphere()),
              if (controller == null)
                _UnavailableBackstage(onReturn: widget.onReturnToFrontstage)
              else
                ListenableBuilder(
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
                        active: widget.active,
                        flightLogs:
                            widget.connectionController.capabilities.flightLogs,
                        vehicleFiles: widget
                            .connectionController
                            .capabilities
                            .vehicleFiles,
                        geofence:
                            widget.connectionController.capabilities.geofence,
                        rally: widget.connectionController.capabilities.rally,
                        airframe: widget
                            .connectionController
                            .snapshot
                            ?.bootstrap
                            .identity
                            .airframe,
                        firmwareType: widget
                            .connectionController
                            .snapshot
                            ?.bootstrap
                            .identity
                            .firmwareType,
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
            ],
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
    required this.active,
    required this.flightLogs,
    required this.vehicleFiles,
    required this.geofence,
    required this.rally,
    required this.airframe,
    required this.firmwareType,
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
  final bool active;
  final FlightLogSessionBackend? flightLogs;
  final VehicleFileSessionBackend? vehicleFiles;
  final GeofenceSessionBackend? geofence;
  final RallySessionBackend? rally;
  final String? airframe;
  final String? firmwareType;
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
    final l10n = mgLocalizationsOf(context);
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
      MacGlassBackstageSection.parameters => l10n.parameters,
      MacGlassBackstageSection.airframe => l10n.airframe,
      MacGlassBackstageSection.servoOutputs => l10n.motorOutputs,
      MacGlassBackstageSection.serialPorts => l10n.portConfiguration,
      MacGlassBackstageSection.remoteControl => l10n.remoteControl,
      MacGlassBackstageSection.pidTuning => l10n.pidTuning,
      MacGlassBackstageSection.calibration => l10n.calibration,
      MacGlassBackstageSection.safety => l10n.safetySettings,
      MacGlassBackstageSection.flightLogs => l10n.flightLogs,
    };
    final serialPortCount = _serialPortNumbers(controller.parameters).length;
    final sectionSubtitle = switch (section) {
      MacGlassBackstageSection.parameters => l10n.parametersDescription(
        controller.parameters.length,
      ),
      MacGlassBackstageSection.airframe => l10n.airframeDescription,
      MacGlassBackstageSection.servoOutputs => l10n.motorOutputsDescription,
      MacGlassBackstageSection.serialPorts => l10n.portConfigurationDescription(
        serialPortCount,
      ),
      MacGlassBackstageSection.remoteControl => l10n.remoteControlDescription,
      MacGlassBackstageSection.pidTuning => l10n.pidTuningDescription,
      MacGlassBackstageSection.calibration => l10n.calibrationDescription,
      MacGlassBackstageSection.safety => l10n.safetySettingsDescription,
      MacGlassBackstageSection.flightLogs => l10n.flightLogsDescription,
    };
    final sectionPage = switch (section) {
      MacGlassBackstageSection.parameters => list,
      MacGlassBackstageSection.airframe => _AirframePage(
        controller: controller,
        airframe: airframe,
        firmwareType: firmwareType,
      ),
      MacGlassBackstageSection.servoOutputs => _ServoOutputPage(
        controller: controller,
        active: active && section == MacGlassBackstageSection.servoOutputs,
      ),
      MacGlassBackstageSection.serialPorts => _SerialPortConfigPage(
        controller: controller,
      ),
      MacGlassBackstageSection.remoteControl => _RemoteControlConfigPage(
        controller: controller,
        active: active && section == MacGlassBackstageSection.remoteControl,
      ),
      MacGlassBackstageSection.pidTuning => _PidTuningPage(
        controller: controller,
        airframe: airframe,
      ),
      MacGlassBackstageSection.calibration => _CalibrationPage(
        controller: controller,
        active: active && section == MacGlassBackstageSection.calibration,
      ),
      MacGlassBackstageSection.safety => _SafetySettingsPage(
        controller: controller,
        geofence: geofence,
        rally: rally,
      ),
      MacGlassBackstageSection.flightLogs => _FlightLogsPage(
        backend: vehicleFiles,
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
