import 'package:flutter/widgets.dart';

import 'mg_skin.dart';

final class MgSkinScope extends InheritedWidget {
  const MgSkinScope({required this.skin, required super.child, super.key});

  final MgSkin skin;

  static MgSkin of(BuildContext context) {
    final scope = context.dependOnInheritedWidgetOfExactType<MgSkinScope>();
    assert(scope != null, 'MgSkinScope was not found in the widget tree.');
    return scope!.skin;
  }

  @override
  bool updateShouldNotify(MgSkinScope oldWidget) => skin != oldWidget.skin;
}
