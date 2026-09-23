import 'package:flutter/widgets.dart';

import 'generated/app_localizations.dart';
import 'generated/app_localizations_zh.dart';

/// Looks up the app locale and keeps isolated widget tests usable without
/// repeating the localization delegate setup in every test harness.
MgLocalizations mgLocalizationsOf(BuildContext context) {
  return Localizations.of<MgLocalizations>(context, MgLocalizations) ??
      MgLocalizationsZh();
}
