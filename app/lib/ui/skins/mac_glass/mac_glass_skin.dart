import 'package:flutter/material.dart';

import '../../../features/frontstage/frontstage_models.dart';
import '../../../features/map/mg_map_catalog.dart';
import '../../skin/mg_skin.dart';
import '../../skin/mg_skin_tokens.dart';
import 'mac_glass_frontstage_shell.dart';

final class MacGlassSkin implements MgSkin {
  const MacGlassSkin();

  @override
  String get id => 'mac_glass';

  @override
  String get name => 'MacGlassSkin';

  @override
  MgSkinTokens get tokens => const MgSkinTokens(
    backgroundTop: Color(0xfff7f8fa),
    backgroundBottom: Color(0xffdfe8ee),
    textPrimary: Color(0xff15161a),
    textSecondary: Color(0xff68717c),
    accent: Color(0xff38a58e),
    warning: Color(0xffeda640),
    danger: Color(0xffd85d53),
    radiusPanel: 24,
    radiusControl: 16,
    blurStrong: 30,
  );

  @override
  ThemeData get themeData => ThemeData(
    colorScheme: ColorScheme.fromSeed(seedColor: tokens.accent),
    useMaterial3: true,
    visualDensity: VisualDensity.compact,
  );

  @override
  Widget buildFrontstageShell({
    required FrontstageModel model,
    required VoidCallback onConnectPressed,
    required VoidCallback onDisconnectPressed,
    required ValueChanged<FrontstageNavigationItem> onNavigationPressed,
    required ValueChanged<FrontstageConnectionTransport>
    onConnectionTransportChanged,
    ValueChanged<String>? onSerialPortChanged,
    ValueChanged<int>? onBaudRateChanged,
    Widget? missionPanel,
    required MgMapStyle mapStyle,
    required ValueChanged<MgMapStyle> onMapStyleChanged,
  }) {
    return MacGlassFrontstageShell(
      model: model,
      onConnectPressed: onConnectPressed,
      onDisconnectPressed: onDisconnectPressed,
      onNavigationPressed: onNavigationPressed,
      onConnectionTransportChanged: onConnectionTransportChanged,
      onSerialPortChanged: onSerialPortChanged,
      onBaudRateChanged: onBaudRateChanged,
      missionPanel: missionPanel,
      mapStyle: mapStyle,
      onMapStyleChanged: onMapStyleChanged,
    );
  }
}
