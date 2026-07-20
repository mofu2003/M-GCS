import 'package:flutter/material.dart';

import 'mac_glass_tokens.dart';

final class MacGlassMapToolbar extends StatelessWidget {
  const MacGlassMapToolbar({
    required this.onZoomIn,
    required this.onZoomOut,
    required this.onReset,
    required this.onNorth,
    required this.onLayers,
    super.key,
  });

  final VoidCallback onZoomIn;
  final VoidCallback onZoomOut;
  final VoidCallback onReset;
  final VoidCallback onNorth;
  final VoidCallback onLayers;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      key: const ValueKey('mg-map-toolbar'),
      borderRadius: BorderRadius.circular(24),
      child: BackdropFilter(
        filter: MacGlassTokens.strongBlur,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.58),
            border: Border.all(color: Colors.white.withValues(alpha: 0.86)),
            borderRadius: BorderRadius.circular(24),
            boxShadow: [MacGlassTokens.floatingShadow()],
          ),
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 8),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                _ToolButton(
                  tooltip: '放大',
                  icon: Icons.add_rounded,
                  onPressed: onZoomIn,
                ),
                _ToolButton(
                  tooltip: '缩小',
                  icon: Icons.remove_rounded,
                  onPressed: onZoomOut,
                ),
                const _Divider(),
                _ToolButton(
                  tooltip: '回到默认位置',
                  icon: Icons.my_location_rounded,
                  onPressed: onReset,
                ),
                _ToolButton(
                  tooltip: '恢复正北',
                  icon: Icons.explore_rounded,
                  onPressed: onNorth,
                ),
                const _Divider(),
                _ToolButton(
                  tooltip: '图层',
                  icon: Icons.layers_rounded,
                  onPressed: onLayers,
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

final class _ToolButton extends StatelessWidget {
  const _ToolButton({
    required this.tooltip,
    required this.icon,
    required this.onPressed,
  });

  final String tooltip;
  final IconData icon;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      child: Tooltip(
        message: tooltip,
        child: IconButton(
          onPressed: onPressed,
          icon: Icon(icon),
          color: MacGlassTokens.ink,
          style: IconButton.styleFrom(
            fixedSize: const Size(40, 40),
            backgroundColor: Colors.white.withValues(alpha: 0.40),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(15),
            ),
          ),
        ),
      ),
    );
  }
}

final class _Divider extends StatelessWidget {
  const _Divider();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 28,
      height: 1,
      margin: const EdgeInsets.symmetric(vertical: 5),
      color: Colors.white.withValues(alpha: 0.72),
    );
  }
}
