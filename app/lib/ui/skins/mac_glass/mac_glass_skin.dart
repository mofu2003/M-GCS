import 'package:flutter/material.dart';
import '../../shell/mg_frontstage_shell_contract.dart';
import '../../skin/mg_skin.dart';
import '../../skin/mg_skin_tokens.dart';
import 'mac_glass_frontstage_shell.dart';
import 'mac_glass_theme.dart';

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
  ThemeData get themeData => buildMacGlassTheme();

  @override
  Widget buildFrontstageShell({
    required MgFrontstageShellModel model,
    required MgFrontstageShellActions actions,
  }) {
    return MacGlassFrontstageShell(model: model, actions: actions);
  }
}
