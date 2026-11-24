import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../theme/mg_colors.dart';
import '../theme/mg_typography.dart';
import '../widgets/mg_floating_panel.dart';
import 'fleet_controller.dart';
import 'fleet_models.dart';
import 'squad_controller.dart';
import 'squad_settings_page.dart';

/// 小队浮动面板宽度。
const double kSquadPanelWidth = 520;

/// 地图右侧的小队管理浮层。
class SquadFloatingPanel extends StatefulWidget {
  const SquadFloatingPanel({
    super.key,
    required this.squads,
    required this.fleet,
    required this.onRequestAction,
    required this.onClose,
  });

  final SquadController? squads;
  final FleetController fleet;
  final void Function(FleetBatchTarget target) onRequestAction;
  final VoidCallback onClose;

  @override
  State<SquadFloatingPanel> createState() => _SquadFloatingPanelState();
}

class _SquadFloatingPanelState extends State<SquadFloatingPanel> {
  final ScrollController _scrollController = ScrollController();

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final squads = widget.squads;
    return SizedBox(
      width: kSquadPanelWidth,
      child: MgFloatingPanel(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 11, 10, 11),
              child: Row(
                children: [
                  Icon(CupertinoIcons.group, size: 17, color: colors.accent),
                  const SizedBox(width: 9),
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        '小队',
                        style: MacosTheme.of(context).typography.title3,
                      ),
                      Text(
                        '持久分组与整队执行',
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textSecondary,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const Spacer(),
                  if (squads != null)
                    AnimatedBuilder(
                      animation: squads,
                      builder: (context, _) => Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 8,
                          vertical: 3,
                        ),
                        decoration: BoxDecoration(
                          color: colors.accent.withValues(alpha: 0.10),
                          borderRadius: BorderRadius.circular(999),
                        ),
                        child: Text(
                          '${squads.squads.length} 个小队',
                          style: MgTypography.sans(
                            MgTypography.caption.copyWith(color: colors.accent),
                          ),
                        ),
                      ),
                    ),
                  const SizedBox(width: 6),
                  MacosIconButton(
                    key: const ValueKey('squad-panel-close'),
                    icon: const MacosIcon(CupertinoIcons.xmark, size: 13),
                    semanticLabel: '关闭小队面板',
                    onPressed: widget.onClose,
                  ),
                ],
              ),
            ),
            Divider(height: 1, color: colors.divider),
            Expanded(
              child: squads == null
                  ? Center(
                      child: Padding(
                        padding: const EdgeInsets.all(20),
                        child: Text(
                          '多机后端尚未就绪，暂时无法管理小队。',
                          textAlign: TextAlign.center,
                          style: MgTypography.sans(
                            MgTypography.caption.copyWith(
                              color: colors.textSecondary,
                            ),
                          ),
                        ),
                      ),
                    )
                  : SquadSettingsPage(
                      scrollController: _scrollController,
                      squads: squads,
                      fleet: widget.fleet,
                      onRequestAction: widget.onRequestAction,
                      onReturnToMap: widget.onClose,
                      embedded: true,
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
