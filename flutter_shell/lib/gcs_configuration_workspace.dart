import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import 'map/amap_map_view.dart'
    show MapStyle, mapStyleAttribution, mapStyleLabel;
import 'connection/connection_controller.dart';
import 'connection/signing_key_store.dart';
import 'theme/mg_colors.dart';
import 'theme/mg_typography.dart';
import 'widgets/app_workspace_shell.dart';

/// 地面站配置。
///
/// 和飞控配置同构（左侧栏目 + 右侧内容），但配置的是**地面站本地行为**，不影响
/// 飞控。第一页是「地图配置」：把原先散在地图右下角的底图选择和数据来源说明收进来，
/// MP / QGC 里属于地面站本地的地图选项也都归到这一页。
class GcsConfigurationWorkspace extends StatefulWidget {
  const GcsConfigurationWorkspace({
    super.key,
    required this.mapStyle,
    required this.connectionController,
    required this.onReturnToMap,
  });

  /// 地图样式的唯一真源，地图页监听它切换图层并持久化。
  final ValueNotifier<MapStyle> mapStyle;
  final ConnectionController connectionController;
  final VoidCallback onReturnToMap;

  @override
  State<GcsConfigurationWorkspace> createState() =>
      _GcsConfigurationWorkspaceState();
}

class _GcsConfigurationWorkspaceState extends State<GcsConfigurationWorkspace> {
  String _selectedPageId = 'map';

  @override
  Widget build(BuildContext context) => AppWorkspaceShell(
    items: const [
      AppWorkspaceItem(id: 'map', label: '地图配置', icon: CupertinoIcons.map),
      AppWorkspaceItem(
        id: 'signing',
        label: 'MAVLink 签名',
        icon: CupertinoIcons.lock_shield,
      ),
    ],
    selectedId: _selectedPageId,
    onSelected: (id) => setState(() => _selectedPageId = id),
    onReturnToMap: widget.onReturnToMap,
    contentBuilder: (context, scrollController) => switch (_selectedPageId) {
      'signing' => _SigningSettingsPage(
        scrollController: scrollController,
        controller: widget.connectionController,
      ),
      _ => _MapSettingsPage(
        scrollController: scrollController,
        mapStyle: widget.mapStyle,
      ),
    },
  );
}

class _MapSettingsPage extends StatelessWidget {
  const _MapSettingsPage({
    required this.scrollController,
    required this.mapStyle,
  });

  final ScrollController scrollController;
  final ValueNotifier<MapStyle> mapStyle;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return ListView(
      controller: scrollController,
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          '地图配置',
          style: MgTypography.sans(
            MgTypography.title.copyWith(color: colors.textPrimary),
          ),
        ),
        const SizedBox(height: 4),
        Text(
          '这些选项只影响地面站本地显示，不会写入飞控。',
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textSecondary),
          ),
        ),
        const SizedBox(height: 18),
        _SectionCard(
          title: '底图来源',
          child: ValueListenableBuilder<MapStyle>(
            valueListenable: mapStyle,
            builder: (context, selected, _) => Column(
              children: [
                for (final style in MapStyle.values)
                  _MapStyleRow(
                    style: style,
                    selected: style == selected,
                    onSelected: () => mapStyle.value = style,
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 16),
        _SectionCard(
          title: '数据来源与署名',
          child: ValueListenableBuilder<MapStyle>(
            valueListenable: mapStyle,
            builder: (context, selected, _) {
              final attribution = mapStyleAttribution(selected);
              return Padding(
                padding: const EdgeInsets.fromLTRB(14, 4, 14, 14),
                child: Text(
                  attribution.isEmpty ? '当前底图无署名要求。' : attribution,
                  style: MgTypography.sans(
                    MgTypography.body.copyWith(color: colors.textSecondary),
                  ),
                ),
              );
            },
          ),
        ),
      ],
    );
  }
}

class _SigningSettingsPage extends StatefulWidget {
  const _SigningSettingsPage({
    required this.scrollController,
    required this.controller,
  });

  final ScrollController scrollController;
  final ConnectionController controller;

  @override
  State<_SigningSettingsPage> createState() => _SigningSettingsPageState();
}

class _SigningSettingsPageState extends State<_SigningSettingsPage> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _keyController = TextEditingController();
  bool _signOutgoing = true;
  bool _busy = false;
  String? _message;
  String? _error;
  Map<String, Object?> _status = const {};

  @override
  void initState() {
    super.initState();
    _syncFieldsFromActive();
    widget.controller.connectionChanges.addListener(_refreshStatus);
    _refreshStatus();
  }

  @override
  void dispose() {
    widget.controller.connectionChanges.removeListener(_refreshStatus);
    _nameController.dispose();
    _keyController.dispose();
    super.dispose();
  }

  void _syncFieldsFromActive() {
    final active = widget.controller.signingPreference;
    _nameController.text = active?.name ?? '';
    _keyController.text = active?.keyHex ?? '';
    _signOutgoing = active?.signOutgoing ?? true;
  }

  Future<void> _refreshStatus() async {
    if (!mounted) return;
    try {
      final status = await widget.controller.readSigningStatus();
      if (!mounted) return;
      setState(() => _status = status);
    } on Object {
      // Connection status refresh is best effort.
    }
  }

  Future<void> _selectKey(SigningKeyPreference entry) async {
    if (_busy) return;
    setState(() {
      _busy = true;
      _error = null;
      _message = null;
    });
    try {
      await widget.controller.selectSigningKey(entry.name);
      if (!mounted) return;
      setState(() {
        _syncFieldsFromActive();
        _message = '已切换到密钥“${entry.name}”。';
      });
      await _refreshStatus();
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _save() async {
    final name = _nameController.text.trim();
    final key = _keyController.text.trim();
    if (name.isEmpty) {
      setState(() => _error = '请填写密钥名称。');
      return;
    }
    if (!isValidSigningKeyHex(key)) {
      setState(() => _error = '密钥必须是 64 位十六进制字符（32 字节）。');
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _message = null;
    });
    try {
      await widget.controller.upsertSigningKey(
        name: name,
        keyHex: key,
        signOutgoing: _signOutgoing,
      );
      await _refreshStatus();
      if (!mounted) return;
      setState(() {
        _message = widget.controller.sessionId == null
            ? '密钥已保存，连接飞控后会自动应用。'
            : '密钥已保存并应用到当前飞控。';
      });
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _sendSetupSigning() async {
    final key = _keyController.text.trim();
    if (!isValidSigningKeyHex(key)) {
      setState(() => _error = '密钥必须是 64 位十六进制字符（32 字节）。');
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _message = null;
    });
    try {
      await widget.controller.sendSetupSigning(
        keyHex: key,
        initialTimestampTicks:
            widget.controller.signingPreference?.lastTimestampTicks ?? 0,
      );
      if (!mounted) return;
      setState(() => _message = 'SETUP_SIGNING 已发送到当前飞控。');
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _removeCurrent() async {
    final name = _nameController.text.trim();
    if (name.isEmpty) return;
    setState(() {
      _busy = true;
      _error = null;
      _message = null;
    });
    try {
      await widget.controller.removeSigningKey(name);
      if (!mounted) return;
      setState(() {
        _syncFieldsFromActive();
        _message = '已删除密钥“$name”。';
      });
      await _refreshStatus();
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _clearAll() async {
    setState(() {
      _busy = true;
      _error = null;
      _message = null;
    });
    try {
      await widget.controller.clearSigningKeyPreference();
      if (!mounted) return;
      setState(() {
        _syncFieldsFromActive();
        _message = '已清除全部本地密钥和当前飞控签名配置。';
      });
      await _refreshStatus();
    } on Object catch (error) {
      if (!mounted) return;
      setState(() => _error = error.toString().replaceFirst('Bad state: ', ''));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  String get _statusLabel {
    if (widget.controller.sessionId == null) return '未连接';
    if (_status['enabled'] != true) return '未启用';
    return switch (_status['status']) {
      'ok' => '已启用 · 签名校验通过',
      'bad_signature' => '已启用 · 签名错误',
      'old_timestamp' => '已启用 · 时间戳过旧',
      'replay' => '已启用 · 检测到重放',
      'too_many_streams' => '已启用 · 签名流过多',
      'no_streams' => '已启用 · 无签名流',
      'offline' => '未连接',
      _ => '已启用',
    };
  }

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final keys = widget.controller.signingKeys;
    final activeName = widget.controller.signingPreference?.name;
    return ListView(
      controller: widget.scrollController,
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          'MAVLink 签名',
          style: MgTypography.sans(
            MgTypography.title.copyWith(color: colors.textPrimary),
          ),
        ),
        const SizedBox(height: 4),
        Text(
          '用于验证飞控链路和 GCS 指令的来源。密钥库保存在本机，不写入飞控参数。',
          style: MgTypography.sans(
            MgTypography.caption.copyWith(color: colors.textSecondary),
          ),
        ),
        const SizedBox(height: 18),
        _SectionCard(
          title: '当前状态',
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
            child: Row(
              children: [
                Icon(
                  _status['enabled'] == true
                      ? CupertinoIcons.lock_fill
                      : CupertinoIcons.lock_open,
                  size: 18,
                  color: _status['enabled'] == true
                      ? colors.success
                      : colors.textSecondary,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    _statusLabel,
                    style: MgTypography.sans(
                      MgTypography.body.copyWith(color: colors.textPrimary),
                    ),
                  ),
                ),
                if (activeName != null)
                  Text(
                    activeName,
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(
                        color: colors.textSecondary,
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 16),
        _SectionCard(
          title: '密钥库',
          child: keys.isEmpty
              ? Padding(
                  padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
                  child: Text(
                    '尚未保存密钥。',
                    style: MgTypography.sans(
                      MgTypography.body.copyWith(color: colors.textSecondary),
                    ),
                  ),
                )
              : Column(
                  children: [
                    for (final entry in keys)
                      _SigningKeyRow(
                        entry: entry,
                        active: entry.name == activeName,
                        busy: _busy,
                        onSelected: () => _selectKey(entry),
                      ),
                  ],
                ),
        ),
        const SizedBox(height: 16),
        _SectionCard(
          title: '编辑密钥',
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 14, 14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text(
                  '名称',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
                const SizedBox(height: 6),
                MacosTextField(
                  controller: _nameController,
                  enabled: !_busy,
                  placeholder: '例如 机队主密钥',
                  onChanged: (_) {
                    if (_error != null) setState(() => _error = null);
                  },
                ),
                const SizedBox(height: 12),
                Text(
                  '32 字节密钥（64 位十六进制）',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
                const SizedBox(height: 6),
                MacosTextField(
                  controller: _keyController,
                  enabled: !_busy,
                  placeholder: '例如 00112233445566778899aabbccddeeff...',
                  onChanged: (_) {
                    if (_error != null) setState(() => _error = null);
                  },
                ),
                const SizedBox(height: 12),
                MacosSwitch(
                  value: _signOutgoing,
                  onChanged: _busy
                      ? null
                      : (value) => setState(() => _signOutgoing = value),
                  semanticLabel: '签名出站 MAVLink 消息',
                ),
                const SizedBox(height: 4),
                Text(
                  '开启后，GCS 发出的 MAVLink 消息会附带签名；入站未签名消息会被拒绝。',
                  style: MgTypography.sans(
                    MgTypography.caption.copyWith(color: colors.textSecondary),
                  ),
                ),
                const SizedBox(height: 14),
                Row(
                  children: [
                    PushButton(
                      controlSize: ControlSize.regular,
                      onPressed: _busy ? null : _save,
                      child: const Text('保存并应用'),
                    ),
                    const SizedBox(width: 8),
                    PushButton(
                      controlSize: ControlSize.regular,
                      secondary: true,
                      onPressed: _busy ? null : _sendSetupSigning,
                      child: const Text('下发到飞控'),
                    ),
                    const SizedBox(width: 8),
                    PushButton(
                      controlSize: ControlSize.regular,
                      secondary: true,
                      onPressed: _busy ? null : _removeCurrent,
                      child: const Text('删除当前'),
                    ),
                    const Spacer(),
                    PushButton(
                      controlSize: ControlSize.regular,
                      secondary: true,
                      onPressed: _busy ? null : _clearAll,
                      child: const Text('清除全部'),
                    ),
                  ],
                ),
                if (_message != null) ...[
                  const SizedBox(height: 10),
                  Text(
                    _message!,
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(color: colors.success),
                    ),
                  ),
                ],
                if (_error != null) ...[
                  const SizedBox(height: 10),
                  Text(
                    _error!,
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(color: colors.danger),
                    ),
                  ),
                ],
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _SigningKeyRow extends StatelessWidget {
  const _SigningKeyRow({
    required this.entry,
    required this.active,
    required this.busy,
    required this.onSelected,
  });

  final SigningKeyPreference entry;
  final bool active;
  final bool busy;
  final VoidCallback onSelected;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    final masked = entry.keyHex.length <= 8
        ? entry.keyHex
        : '••••••••${entry.keyHex.substring(entry.keyHex.length - 8)}';
    return Material(
      color: active ? colors.accentSurface : colors.transparent,
      child: InkWell(
        onTap: busy || active ? null : onSelected,
        hoverColor: colors.mapControlHover,
        child: SizedBox(
          height: MgMetrics.rowHeight,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 14),
            child: Row(
              children: [
                Icon(
                  active ? CupertinoIcons.lock_fill : CupertinoIcons.lock,
                  size: 16,
                  color: active ? colors.accent : colors.textSecondary,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        entry.name,
                        style: MgTypography.sans(
                          MgTypography.body.copyWith(
                            color: active ? colors.accent : colors.textPrimary,
                          ),
                        ),
                      ),
                      Text(
                        masked,
                        style: MgTypography.sans(
                          MgTypography.caption.copyWith(
                            color: colors.textTertiary,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                if (active)
                  Text(
                    '使用中',
                    style: MgTypography.sans(
                      MgTypography.caption.copyWith(color: colors.accent),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// 与飞行模式列表一致的选中表现：底色高亮 + 强调色文字，不用对勾。
class _MapStyleRow extends StatelessWidget {
  const _MapStyleRow({
    required this.style,
    required this.selected,
    required this.onSelected,
  });

  final MapStyle style;
  final bool selected;
  final VoidCallback onSelected;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return Material(
      color: selected ? colors.accentSurface : colors.transparent,
      child: InkWell(
        onTap: onSelected,
        hoverColor: colors.mapControlHover,
        child: SizedBox(
          height: MgMetrics.rowHeight,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 14),
            child: Align(
              alignment: Alignment.centerLeft,
              child: Text(
                mapStyleLabel(style),
                style: MgTypography.sans(
                  MgTypography.body.copyWith(
                    color: selected ? colors.accent : colors.textPrimary,
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

class _SectionCard extends StatelessWidget {
  const _SectionCard({required this.title, required this.child});

  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final colors = context.mgColors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(MgMetrics.panelRadius),
        border: Border.all(color: colors.divider),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 14, 8),
            child: Text(
              title,
              style: MgTypography.sans(
                MgTypography.section.copyWith(color: colors.textPrimary),
              ),
            ),
          ),
          Divider(height: 1, color: colors.hairline),
          child,
        ],
      ),
    );
  }
}
