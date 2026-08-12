part of 'backstage_page.dart';

class _GlassPanel extends StatelessWidget {
  const _GlassPanel({required this.child, this.radius = 8, this.blur = false});
  final Widget child;
  final double radius;
  final bool blur;

  @override
  Widget build(BuildContext context) {
    final panel = DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xfffbfbfd),
        border: Border.all(color: const Color(0xffd1d1d6)),
        borderRadius: BorderRadius.circular(radius),
      ),
      child: Material(type: MaterialType.transparency, child: child),
    );
    return ClipRRect(borderRadius: BorderRadius.circular(radius), child: panel);
  }
}

/// Reusable configuration-module surface for the backstage setting pages.
class _BackstageFeatureCard extends StatelessWidget {
  const _BackstageFeatureCard({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: const Color(0xfff7f7f9),
      border: Border.all(color: MacGlassTokens.ink.withValues(alpha: 0.10)),
      borderRadius: BorderRadius.circular(8),
    ),
    child: Material(type: MaterialType.transparency, child: child),
  );
}

class _BackstageHeader extends StatelessWidget {
  const _BackstageHeader({
    required this.title,
    required this.subtitle,
    required this.showActions,
    required this.onRead,
    required this.onSave,
    required this.onRefresh,
    required this.onReviewWrite,
  });
  final String title;
  final String subtitle;
  final bool showActions;
  final VoidCallback? onRead;
  final VoidCallback? onSave;
  final VoidCallback? onRefresh;
  final VoidCallback? onReviewWrite;
  @override
  Widget build(BuildContext context) => _GlassPanel(
    radius: MacGlassTokens.radiusPanel,
    blur: true,
    child: SizedBox(
      key: const ValueKey('mg-backstage-header'),
      height: MacGlassTokens.topBarHeight,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 0, 12, 0),
        child: Row(
          children: [
            Expanded(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title, style: Theme.of(context).textTheme.titleMedium),
                  Text(subtitle, style: MacGlassTypography.caption),
                ],
              ),
            ),
            if (showActions) ...[
              _CommandButton(
                icon: Icons.folder_open_rounded,
                label: '读取',
                onPressed: onRead,
              ),
              const SizedBox(width: 6),
              _CommandButton(
                icon: Icons.save_alt_rounded,
                label: '保存',
                onPressed: onSave,
              ),
              const SizedBox(width: 6),
              _CommandButton(
                icon: Icons.refresh_rounded,
                label: '刷新',
                onPressed: onRefresh,
              ),
              const SizedBox(width: 6),
              _CommandButton(
                icon: Icons.upload_rounded,
                label: '写入',
                onPressed: onReviewWrite,
              ),
            ],
          ],
        ),
      ),
    ),
  );
}

class _CommandButton extends StatelessWidget {
  const _CommandButton({
    required this.icon,
    required this.label,
    required this.onPressed,
  });

  final IconData icon;
  final String label;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) => cupertino.CupertinoButton(
    onPressed: onPressed,
    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
    minimumSize: const Size(0, 34),
    borderRadius: BorderRadius.circular(7),
    color: const Color(0xfff2f2f7),
    disabledColor: const Color(0xfff2f2f7),
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 16),
        const SizedBox(width: 5),
        Text(label, style: MacGlassTypography.control),
      ],
    ),
  );
}
