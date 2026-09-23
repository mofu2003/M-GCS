import 'package:flutter/material.dart';

import 'mac_glass_tokens.dart';

final class MacGlassMenu {
  const MacGlassMenu._();

  static MenuStyle style({double minimumWidth = 220}) {
    return MenuStyle(
      backgroundColor: WidgetStatePropertyAll(
        Colors.white.withValues(alpha: 0.92),
      ),
      elevation: const WidgetStatePropertyAll(0),
      padding: const WidgetStatePropertyAll(EdgeInsets.all(5)),
      minimumSize: WidgetStatePropertyAll(Size(minimumWidth, 0)),
      shape: WidgetStatePropertyAll(
        RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      ),
      side: WidgetStatePropertyAll(
        BorderSide(color: Colors.white.withValues(alpha: 0.92)),
      ),
      visualDensity: VisualDensity.compact,
    );
  }

  static ButtonStyle itemStyle({required bool selected}) {
    return ButtonStyle(
      alignment: Alignment.centerLeft,
      backgroundColor: WidgetStateProperty.resolveWith((states) {
        if (selected) {
          return MacGlassTokens.green.withValues(alpha: 0.14);
        }
        if (states.contains(WidgetState.hovered) ||
            states.contains(WidgetState.focused)) {
          return MacGlassTokens.ink.withValues(alpha: 0.07);
        }
        return Colors.transparent;
      }),
      foregroundColor: const WidgetStatePropertyAll(MacGlassTokens.ink),
      overlayColor: const WidgetStatePropertyAll(Colors.transparent),
      padding: const WidgetStatePropertyAll(
        EdgeInsets.symmetric(horizontal: 10, vertical: 9),
      ),
      shape: WidgetStatePropertyAll(
        RoundedRectangleBorder(borderRadius: BorderRadius.circular(11)),
      ),
    );
  }
}
