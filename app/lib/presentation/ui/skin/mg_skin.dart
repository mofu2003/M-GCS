import 'package:flutter/material.dart';
import '../shell/mg_frontstage_shell_contract.dart';
import 'mg_skin_tokens.dart';

abstract interface class MgSkin {
  String get id;
  String get name;
  MgSkinTokens get tokens;
  ThemeData get themeData;

  Widget buildFrontstageShell({
    required MgFrontstageShellModel model,
    required MgFrontstageShellActions actions,
  });
}
