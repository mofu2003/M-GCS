import 'package:flutter/material.dart';

import '../shell/mg_frontstage_shell_contract.dart';
import '../skin/mg_skin_scope.dart';

final class MgFrontstageShell extends StatelessWidget {
  const MgFrontstageShell({
    required this.model,
    required this.actions,
    super.key,
  });

  final MgFrontstageShellModel model;
  final MgFrontstageShellActions actions;

  @override
  Widget build(BuildContext context) {
    final skin = MgSkinScope.of(context);
    return skin.buildFrontstageShell(model: model, actions: actions);
  }
}
