import 'package:flutter/material.dart';

import '../../features/frontstage/frontstage_models.dart';
import '../../features/map/mg_map_catalog.dart';
import 'mg_skin_tokens.dart';

abstract interface class MgSkin {
  String get id;
  String get name;
  MgSkinTokens get tokens;
  ThemeData get themeData;

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
  });
}
