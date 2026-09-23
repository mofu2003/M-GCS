import 'package:flutter/cupertino.dart' as cupertino;
import 'package:flutter/material.dart';

import 'mac_glass_tokens.dart';

/// The single visual source of truth for the MacOS glass skin.
///
/// Pages should consume this theme instead of creating their own ThemeData.
/// That keeps text contrast, controls, focus states and spacing consistent
/// across the frontstage and backstage.
ThemeData buildMacGlassTheme() {
  final scheme =
      ColorScheme.fromSeed(
        seedColor: MacGlassTokens.blue,
        brightness: Brightness.light,
      ).copyWith(
        surface: MacGlassTokens.surface,
        onSurface: MacGlassTokens.ink,
        surfaceContainerHighest: MacGlassTokens.surfaceStrong,
        primary: MacGlassTokens.blue,
        onPrimary: Colors.white,
        outline: MacGlassTokens.controlBorder,
      );
  final baseText = ThemeData.light().textTheme.apply(
    bodyColor: MacGlassTokens.ink,
    displayColor: MacGlassTokens.ink,
  );
  final textTheme = baseText.copyWith(
    titleLarge: MacGlassFonts.platformTextStyle(MacGlassTypography.pageTitle),
    titleMedium: MacGlassFonts.platformTextStyle(
      MacGlassTypography.sectionTitle,
    ),
    titleSmall: MacGlassFonts.platformTextStyle(MacGlassTypography.cardTitle),
    bodyLarge: MacGlassFonts.platformTextStyle(MacGlassTypography.body),
    bodyMedium: MacGlassFonts.platformTextStyle(MacGlassTypography.body),
    bodySmall: MacGlassFonts.platformTextStyle(MacGlassTypography.caption),
    labelLarge: MacGlassFonts.platformTextStyle(MacGlassTypography.control),
    labelMedium: MacGlassFonts.platformTextStyle(
      MacGlassTypography.tableHeader,
    ),
    labelSmall: MacGlassFonts.platformTextStyle(MacGlassTypography.caption),
  );
  final inputBorder = OutlineInputBorder(
    borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
    borderSide: const BorderSide(color: MacGlassTokens.controlBorder),
  );

  return ThemeData(
    brightness: Brightness.light,
    colorScheme: scheme,
    useMaterial3: true,
    visualDensity: VisualDensity.compact,
    scaffoldBackgroundColor: Colors.transparent,
    canvasColor: Colors.transparent,
    cardColor: MacGlassTokens.surface,
    dividerColor: MacGlassTokens.divider,
    textTheme: textTheme,
    iconTheme: const IconThemeData(color: MacGlassTokens.ink),
    inputDecorationTheme: InputDecorationTheme(
      isDense: true,
      filled: true,
      fillColor: MacGlassTokens.controlFill,
      contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 11),
      hintStyle: MacGlassFonts.platformTextStyle(
        MacGlassTypography.body.copyWith(color: MacGlassTokens.muted),
      ),
      labelStyle: MacGlassFonts.platformTextStyle(
        MacGlassTypography.body.copyWith(color: MacGlassTokens.ink),
      ),
      floatingLabelStyle: MacGlassFonts.platformTextStyle(
        MacGlassTypography.caption.copyWith(color: MacGlassTokens.blue),
      ),
      prefixIconColor: MacGlassTokens.muted,
      suffixIconColor: MacGlassTokens.muted,
      enabledBorder: inputBorder,
      focusedBorder: inputBorder.copyWith(
        borderSide: const BorderSide(color: MacGlassTokens.blue, width: 1.4),
      ),
      disabledBorder: inputBorder.copyWith(
        borderSide: const BorderSide(
          color: MacGlassTokens.controlBorderDisabled,
        ),
      ),
      errorBorder: inputBorder.copyWith(
        borderSide: const BorderSide(color: MacGlassTokens.red),
      ),
      focusedErrorBorder: inputBorder.copyWith(
        borderSide: const BorderSide(color: MacGlassTokens.red, width: 1.4),
      ),
    ),
    textSelectionTheme: TextSelectionThemeData(
      cursorColor: MacGlassTokens.blue,
      selectionColor: MacGlassTokens.blue.withValues(alpha: 0.22),
      selectionHandleColor: MacGlassTokens.blue,
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        minimumSize: const Size(0, 38),
        padding: const EdgeInsets.symmetric(horizontal: 14),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
        ),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        minimumSize: const Size(0, 38),
        padding: const EdgeInsets.symmetric(horizontal: 14),
        foregroundColor: MacGlassTokens.ink,
        side: const BorderSide(color: MacGlassTokens.controlBorder),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
        ),
      ),
    ),
    segmentedButtonTheme: SegmentedButtonThemeData(
      style: ButtonStyle(
        visualDensity: VisualDensity.compact,
        textStyle: WidgetStatePropertyAll(
          MacGlassFonts.platformTextStyle(MacGlassTypography.control),
        ),
        shape: WidgetStatePropertyAll(
          RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(MacGlassTokens.radiusControl),
          ),
        ),
      ),
    ),
    cupertinoOverrideTheme: cupertino.CupertinoThemeData(
      brightness: Brightness.light,
      primaryColor: MacGlassTokens.blue,
      scaffoldBackgroundColor: Colors.transparent,
      barBackgroundColor: MacGlassTokens.surfaceSubtle,
      textTheme: cupertino.CupertinoTextThemeData(
        textStyle: MacGlassFonts.platformTextStyle(MacGlassTypography.body),
        actionTextStyle: MacGlassFonts.platformTextStyle(
          MacGlassTypography.control.copyWith(color: MacGlassTokens.blue),
        ),
        tabLabelTextStyle: MacGlassFonts.platformTextStyle(
          MacGlassTypography.caption,
        ),
        navTitleTextStyle: MacGlassFonts.platformTextStyle(
          MacGlassTypography.pageTitle,
        ),
      ),
    ),
  );
}
