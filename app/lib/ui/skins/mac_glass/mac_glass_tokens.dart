import 'dart:ui';

import 'package:flutter/material.dart';

final class MacGlassTokens {
  const MacGlassTokens._();

  static const Color ink = Color(0xff15161a);
  static const Color muted = Color(0xff6e6e73);
  static const Color glass = Color(0x99ffffff);
  static const Color glassStrong = Color(0xb8ffffff);
  static const Color black = Color(0xff111215);
  static const Color green = Color(0xff38a58e);
  static const Color amber = Color(0xffeda640);
  static const Color red = Color(0xffd85d53);
  static const Color blue = Color(0xff007aff);

  static const double topBarHeight = 72;
  static const double dockWidth = 72;
  static const double pageInset = 18;
  static const double radiusLarge = 28;
  static const double radiusPanel = 8;
  static const double radiusControl = 8;
  static ImageFilter get strongBlur => ImageFilter.blur(sigmaX: 30, sigmaY: 30);

  static BoxShadow floatingShadow() {
    return BoxShadow(
      color: const Color(0xff161a20).withValues(alpha: 0.16),
      blurRadius: 60,
      offset: const Offset(0, 24),
    );
  }
}

/// Shared type scale for the operational UI. The default platform family keeps
/// CJK fallback correct on macOS, Windows and Linux; only identifiers use mono.
final class MacGlassTypography {
  const MacGlassTypography._();

  static const TextStyle pageTitle = TextStyle(
    fontSize: 17,
    height: 1.2,
    fontWeight: FontWeight.w600,
  );
  static const TextStyle sectionTitle = TextStyle(
    fontSize: 15,
    height: 1.25,
    fontWeight: FontWeight.w600,
  );
  static const TextStyle cardTitle = TextStyle(
    fontSize: 13,
    height: 1.25,
    fontWeight: FontWeight.w600,
  );
  static const TextStyle body = TextStyle(fontSize: 13, height: 1.3);
  static const TextStyle caption = TextStyle(
    color: MacGlassTokens.muted,
    fontSize: 11,
    height: 1.25,
    fontWeight: FontWeight.w500,
  );
  static const TextStyle tableHeader = TextStyle(
    color: MacGlassTokens.muted,
    fontSize: 11,
    height: 1.2,
    fontWeight: FontWeight.w600,
  );
  static const TextStyle control = TextStyle(
    fontSize: 12.5,
    height: 1.2,
    fontWeight: FontWeight.w600,
  );
  static const TextStyle value = TextStyle(
    fontSize: 12,
    height: 1.2,
    fontWeight: FontWeight.w600,
    fontFeatures: [FontFeature.tabularFigures()],
  );
  static const TextStyle code = TextStyle(
    fontFamily: 'monospace',
    fontSize: 11,
    height: 1.2,
    fontWeight: FontWeight.w600,
    fontFeatures: [FontFeature.tabularFigures()],
  );
}
