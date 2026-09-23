import 'package:flutter/material.dart';

import 'mac_glass_tokens.dart';

final class MacGlassCloseButton extends StatelessWidget {
  const MacGlassCloseButton({
    required this.tooltip,
    required this.onPressed,
    super.key,
  });

  final String tooltip;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return SizedBox.square(
      dimension: 28,
      child: IconButton(
        tooltip: tooltip,
        onPressed: onPressed,
        padding: EdgeInsets.zero,
        constraints: const BoxConstraints.tightFor(width: 28, height: 28),
        style: IconButton.styleFrom(
          fixedSize: const Size.square(28),
          minimumSize: const Size.square(28),
          shape: const CircleBorder(),
          backgroundColor: MacGlassTokens.black.withValues(alpha: 0.05),
        ),
        icon: const Icon(Icons.close_rounded, size: 16),
      ),
    );
  }
}
