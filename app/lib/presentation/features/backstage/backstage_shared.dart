part of 'backstage_page.dart';

class _BackstageAtmosphere extends StatelessWidget {
  const _BackstageAtmosphere();

  @override
  Widget build(BuildContext context) => Stack(
    fit: StackFit.expand,
    children: [
      Positioned(
        top: -150,
        right: -80,
        child: _AtmosphereOrb(
          size: 430,
          color: MacGlassTokens.blue.withValues(alpha: 0.10),
        ),
      ),
      Positioned(
        bottom: -190,
        left: -110,
        child: _AtmosphereOrb(
          size: 520,
          color: const Color(0xff8c79ff).withValues(alpha: 0.08),
        ),
      ),
      Positioned(
        top: 230,
        left: 280,
        child: _AtmosphereOrb(
          size: 240,
          color: const Color(0xff48c5b0).withValues(alpha: 0.045),
        ),
      ),
    ],
  );
}

class _AtmosphereOrb extends StatelessWidget {
  const _AtmosphereOrb({required this.size, required this.color});

  final double size;
  final Color color;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(shape: BoxShape.circle, color: color),
    child: SizedBox(width: size, height: size),
  );
}

class _GlassPanel extends StatelessWidget {
  const _GlassPanel({required this.child, this.radius = 18});
  final Widget child;
  final double radius;

  @override
  Widget build(BuildContext context) => MacGlassSurface(
    radius: radius,
    emphasis: MacGlassSurfaceEmphasis.strong,
    child: child,
  );
}

/// Reusable configuration-module surface for the backstage setting pages.
class _BackstageFeatureCard extends StatelessWidget {
  const _BackstageFeatureCard({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) =>
      MacGlassSurface(radius: MacGlassTokens.radiusCard, child: child);
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
    radius: MacGlassTokens.radiusLarge,
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
    color: Colors.white.withValues(alpha: 0.58),
    disabledColor: Colors.white.withValues(alpha: 0.28),
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
