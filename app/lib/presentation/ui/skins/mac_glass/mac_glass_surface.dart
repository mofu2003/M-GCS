import 'package:flutter/material.dart';

import 'mac_glass_tokens.dart';

enum MacGlassSurfaceEmphasis { regular, strong }

/// Shared glass surface for floating controls and panels.
///
/// Keeping the blur, border, radius and shadow in one primitive makes it
/// possible to replace or restyle the shell without changing feature code.
final class MacGlassSurface extends StatelessWidget {
  const MacGlassSurface({
    required this.child,
    this.radius = MacGlassTokens.radiusPanel,
    this.emphasis = MacGlassSurfaceEmphasis.regular,
    super.key,
  });

  final Widget child;
  final double radius;
  final MacGlassSurfaceEmphasis emphasis;

  @override
  Widget build(BuildContext context) {
    final color = emphasis == MacGlassSurfaceEmphasis.strong
        ? MacGlassTokens.surfaceStrong
        : MacGlassTokens.surface;
    return ClipRRect(
      borderRadius: BorderRadius.circular(radius),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: color,
            border: Border.all(color: MacGlassTokens.controlBorder),
            borderRadius: BorderRadius.circular(radius),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Material(type: MaterialType.transparency, child: child),
        ),
      ),
    );
  }
}
