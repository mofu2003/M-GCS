import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

final class MacGlassTokens {
  const MacGlassTokens._();

  static const Color ink = Color(0xff15161a);
  static const Color muted = Color(0xff6e6e73);
  static const Color canvasTop = Color(0xffe7edf5);
  static const Color canvasMiddle = Color(0xfff7f8fb);
  static const Color canvasBottom = Color(0xffe9eef4);
  static const Color glass = Color(0x99ffffff);
  static const Color glassStrong = Color(0xb8ffffff);
  static const Color surface = Color(0x8cffffff);
  static const Color surfaceStrong = Color(0xb8ffffff);
  static const Color surfaceSubtle = Color(0x66ffffff);
  static const Color controlFill = Color(0x94ffffff);
  static const Color controlBorder = Color(0x8fffffff);
  static const Color controlBorderDisabled = Color(0x52ffffff);
  static const Color surfaceBorder = Color(0xc7ffffff);
  static const Color divider = Color(0x3d15161a);
  static const Color black = Color(0xff111215);
  static const Color green = Color(0xff38a58e);
  static const Color amber = Color(0xffeda640);
  static const Color red = Color(0xffd85d53);
  static const Color blue = Color(0xff007aff);

  static const double topBarHeight = 72;
  static const double contentTop = 102;
  static const double contentLeft = 106;
  static const double contentRight = 82;
  static const double bottomControlInset = 50;
  static const double dashboardHeight = 216;
  static const double overlayGap = 12;
  static const double dockWidth = 72;
  static const double pageInset = 18;
  static const double radiusLarge = 28;
  static const double radiusPanel = 18;
  static const double radiusCard = 18;
  static const double radiusControl = 12;
  static ImageFilter get strongBlur => ImageFilter.blur(sigmaX: 30, sigmaY: 30);

  static BoxShadow floatingShadow() {
    return BoxShadow(
      color: const Color(0xff161a20).withValues(alpha: 0.16),
      blurRadius: 60,
      offset: const Offset(0, 24),
    );
  }
}

/// Native UI font selection for the operational shell. Linux intentionally
/// stays on the fontconfig default so it can follow the host distribution.
final class MacGlassFonts {
  const MacGlassFonts._();

  static String? get uiFamily {
    if (defaultTargetPlatform == TargetPlatform.windows) {
      return 'Segoe UI';
    }
    if (defaultTargetPlatform == TargetPlatform.macOS) {
      return 'CupertinoSystemText';
    }
    return null;
  }

  static List<String>? get uiFallback {
    if (defaultTargetPlatform == TargetPlatform.windows) {
      return const ['Microsoft YaHei UI', 'Microsoft YaHei'];
    }
    if (defaultTargetPlatform == TargetPlatform.macOS) {
      return const ['PingFang SC'];
    }
    return null;
  }

  static TextStyle platformTextStyle(TextStyle style) {
    return style.copyWith(fontFamily: uiFamily, fontFamilyFallback: uiFallback);
  }
}

/// Shared type scale for the operational UI. Root themes apply
/// [MacGlassFonts.uiFamily]; only identifiers opt into monospace.
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
