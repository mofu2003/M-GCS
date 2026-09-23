import 'package:flutter/material.dart';

import '../../../features/map/mg_map_catalog.dart';
import 'mac_glass_menu.dart';
import 'mac_glass_surface.dart';
import 'mac_glass_tokens.dart';

final class MacGlassMapStyleSwitcher extends StatelessWidget {
  const MacGlassMapStyleSwitcher({
    required this.value,
    required this.onChanged,
    super.key,
  });

  final MgMapStyle value;
  final ValueChanged<MgMapStyle> onChanged;

  @override
  Widget build(BuildContext context) {
    final source = MgMapCatalog.sourceFor(value);
    return MacGlassSurface(
      key: const ValueKey('mg-map-style-switcher'),
      radius: MacGlassTokens.radiusPanel,
      child: Padding(
        padding: const EdgeInsets.all(4),
        child: MenuAnchor(
          style: MacGlassMenu.style(minimumWidth: 260),
          alignmentOffset: const Offset(-48, -156),
          menuChildren: [
            for (final style in MgMapStyle.values)
              MenuItemButton(
                key: ValueKey('mg-map-style-option-${style.name}'),
                onPressed: () => onChanged(style),
                style: MacGlassMenu.itemStyle(selected: style == value),
                child: Row(
                  children: [
                    Icon(
                      style == value
                          ? Icons.check_rounded
                          : Icons.layers_outlined,
                      size: 17,
                      color: style == value
                          ? MacGlassTokens.green
                          : MacGlassTokens.muted,
                    ),
                    const SizedBox(width: 9),
                    Expanded(
                      child: Text(
                        MgMapCatalog.sourceFor(style).label,
                        maxLines: 1,
                        style: const TextStyle(fontWeight: FontWeight.w700),
                      ),
                    ),
                  ],
                ),
              ),
          ],
          builder: (context, controller, child) => Material(
            color: Colors.transparent,
            child: InkWell(
              key: const ValueKey('mg-map-style-menu-button'),
              borderRadius: BorderRadius.circular(14),
              onTap: () {
                if (controller.isOpen) {
                  controller.close();
                } else {
                  controller.open();
                }
              },
              child: SizedBox(
                width: 212,
                height: 34,
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 8),
                  child: Row(
                    children: [
                      const Icon(Icons.layers_rounded, size: 15),
                      const SizedBox(width: 6),
                      Expanded(
                        child: FittedBox(
                          fit: BoxFit.scaleDown,
                          alignment: Alignment.centerLeft,
                          child: Text(
                            source.label,
                            maxLines: 1,
                            style: const TextStyle(
                              fontSize: 11,
                              fontWeight: FontWeight.w700,
                            ),
                          ),
                        ),
                      ),
                      const Icon(Icons.keyboard_arrow_up_rounded, size: 16),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
