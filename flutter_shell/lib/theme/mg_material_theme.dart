import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

/// Resolve the Cupertino → Material bridge once per theme change.
/// Without an inherited Material Theme, every Material control's Theme.of()
/// constructs a new color scheme while building the macOS interface.
class MgMaterialTheme extends StatefulWidget {
  const MgMaterialTheme({super.key, required this.child});

  final Widget child;

  @override
  State<MgMaterialTheme> createState() => _MgMaterialThemeState();
}

class _MgMaterialThemeState extends State<MgMaterialTheme> {
  late ThemeData _theme;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _theme = CupertinoBasedMaterialThemeData(
      themeData: CupertinoTheme.of(
        context,
      ).copyWith(brightness: CupertinoTheme.brightnessOf(context)),
    ).materialTheme;
  }

  @override
  Widget build(BuildContext context) =>
      Theme(data: _theme, child: widget.child);
}
