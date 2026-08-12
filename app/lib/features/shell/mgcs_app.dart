import 'dart:async';

import 'package:flutter/material.dart';

import '../../application/connection_controller.dart';
import '../../application/connection_models.dart';
import '../../application/connection_session_backend.dart';
import '../../application/parameter_controller.dart';
import '../backstage/backstage_page.dart';
import '../frontstage/frontstage_page.dart';
import '../../ui/skins/mac_glass/mac_glass_tokens.dart';

typedef ConnectionBackendFactory = ConnectionSessionBackend Function();

final class MgcsApp extends StatefulWidget {
  const MgcsApp({required this.backendFactory, super.key});

  final ConnectionBackendFactory backendFactory;

  @override
  State<MgcsApp> createState() => _MgcsAppState();
}

final class _MgcsAppState extends State<MgcsApp> {
  late final ConnectionSessionBackend _backend;
  late final ConnectionController _connectionController;
  late final ParameterController? _parameterController;
  var _backstageVisible = false;

  @override
  void initState() {
    super.initState();
    _backend = widget.backendFactory();
    _connectionController = ConnectionController(backend: _backend);
    _parameterController = _backend is ParameterSessionBackend
        ? ParameterController(
            backend: _backend as ParameterSessionBackend,
            isSessionReady: () =>
                _connectionController.status == ConnectionStatus.connected &&
                _connectionController.bootstrapState ==
                    ConnectionBootstrapState.complete,
          )
        : null;
    _connectionController.addListener(_refreshParametersWhenReady);
    _connectionController.initialize();
  }

  void _refreshParametersWhenReady() {
    final controller = _parameterController;
    if (controller != null &&
        controller.canEdit &&
        controller.parameters.isEmpty &&
        !controller.refreshing) {
      unawaited(controller.refresh());
    }
  }

  @override
  void dispose() {
    _connectionController.removeListener(_refreshParametersWhenReady);
    _parameterController?.dispose();
    _connectionController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'M-GCS',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        textTheme: const TextTheme(
          titleLarge: MacGlassTypography.pageTitle,
          titleMedium: MacGlassTypography.pageTitle,
          titleSmall: MacGlassTypography.sectionTitle,
          bodyLarge: MacGlassTypography.body,
          bodyMedium: MacGlassTypography.body,
          bodySmall: MacGlassTypography.caption,
          labelLarge: MacGlassTypography.control,
          labelMedium: MacGlassTypography.tableHeader,
        ),
      ),
      home: IndexedStack(
        index: _backstageVisible ? 1 : 0,
        children: [
          FrontstagePage(
            controller: _connectionController,
            onOpenBackstage: () => setState(() => _backstageVisible = true),
          ),
          BackstagePage(
            connectionController: _connectionController,
            parameterController: _parameterController,
            onReturnToFrontstage: () =>
                setState(() => _backstageVisible = false),
          ),
        ],
      ),
    );
  }
}
