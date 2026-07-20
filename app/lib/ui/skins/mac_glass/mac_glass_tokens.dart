import 'dart:ui';

import 'package:flutter/material.dart';

final class MacGlassTokens {
  const MacGlassTokens._();

  static const Color ink = Color(0xff15161a);
  static const Color muted = Color(0xff68717c);
  static const Color glass = Color(0x99ffffff);
  static const Color glassStrong = Color(0xb8ffffff);
  static const Color black = Color(0xff111215);
  static const Color green = Color(0xff38a58e);
  static const Color amber = Color(0xffeda640);
  static const Color red = Color(0xffd85d53);
  static const Color blue = Color(0xff648ed2);

  static const double topBarHeight = 72;
  static const double dockWidth = 72;
  static const double pageInset = 18;
  static const double radiusLarge = 28;
  static const double radiusPanel = 24;
  static const double radiusControl = 16;
  static ImageFilter get strongBlur => ImageFilter.blur(sigmaX: 30, sigmaY: 30);

  static BoxShadow floatingShadow() {
    return BoxShadow(
      color: const Color(0xff161a20).withValues(alpha: 0.16),
      blurRadius: 60,
      offset: const Offset(0, 24),
    );
  }
}
