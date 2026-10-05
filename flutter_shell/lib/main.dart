import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'connection/connection_controller.dart';
import 'configuration_workspace.dart';
import 'map/amap_map_view.dart';
import 'theme/mg_typography.dart';
import 'widgets/mg_top_status_bar.dart';

Future<void> main() async {
  if (!kIsWeb && Platform.isMacOS) {
    WidgetsFlutterBinding.ensureInitialized();
    await WindowManipulator.initialize(enableWindowDelegate: true);
    await WindowManipulator.setMaterial(
      NSVisualEffectViewMaterial.windowBackground,
    );
    await WindowManipulator.disableFullSizeContentView();
    await WindowManipulator.makeTitlebarOpaque();
    await WindowManipulator.showTitle();
  }
  runApp(const MgcsApp());
}

class MgcsApp extends StatelessWidget {
  const MgcsApp({super.key});

  @override
  Widget build(BuildContext context) => MacosApp(
    title: 'M-GCS',
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.system,
    theme: MacosThemeData.light().copyWith(
      typography: MgTypography.macos(Brightness.light),
    ),
    darkTheme: MacosThemeData.dark().copyWith(
      typography: MgTypography.macos(Brightness.dark),
    ),
    home: MapPage(),
  );
}

/// 地面站地图主页面。
class MapPage extends StatefulWidget {
  const MapPage({super.key});

  @override
  State<MapPage> createState() => _MapPageState();
}

class _MapPageState extends State<MapPage> {
  var _showConfiguration = false;
  late final ConnectionController _connectionController =
      ConnectionController();

  @override
  Widget build(BuildContext context) {
    if (_showConfiguration) {
      return ConfigurationWorkspace(
        connectionController: _connectionController,
        onReturnToMap: () => setState(() => _showConfiguration = false),
      );
    }

    return MacosWindow(
      child: Column(
        children: [
          AnimatedBuilder(
            animation: Listenable.merge([
              _connectionController.connectionChanges,
              _connectionController.telemetryChanges,
              _connectionController.profileChanges,
            ]),
            builder: (context, _) => MgTopStatusBar(
              data: MgTopStatusData.fromController(_connectionController),
            ),
          ),
          Expanded(
            child: MacosScaffold(
              children: [
                ContentArea(
                  builder: (context, _) => AMapMapView(
                    connectionController: _connectionController,
                    onOpenConfiguration: () =>
                        setState(() => _showConfiguration = true),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _connectionController.dispose();
    super.dispose();
  }
}
