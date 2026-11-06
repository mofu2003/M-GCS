import 'dart:io';

import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:macos_ui/macos_ui.dart';

import '../widgets/mg_dialog.dart';

/// 静态外观画廊：只摆出 macos_ui 自带组件的样子，不做业务功能。
///
/// 运行：flutter run -d macos --target lib/component_gallery_demo.dart
///
/// 开发工具，不在应用主流程里：`main.dart` 不引用它，也不会进 release 包。
/// 画廊需要并排对照 `macos_ui` 的原始色板与字号，因此直接使用原始颜色和字号。
Future<void> main() async {
  if (!kIsWeb && Platform.isMacOS) {
    await const MacosWindowUtilsConfig().apply();
  }
  runApp(const ComponentGalleryApp());
}

class ComponentGalleryApp extends StatelessWidget {
  const ComponentGalleryApp({super.key});

  @override
  Widget build(BuildContext context) => MacosApp(
    title: 'macos_ui 组件外观',
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.system,
    theme: MacosThemeData.light(accentColor: AccentColor.blue),
    darkTheme: MacosThemeData.dark(accentColor: AccentColor.blue),
    home: const ComponentGalleryPage(),
  );
}

class ComponentGalleryPage extends StatefulWidget {
  const ComponentGalleryPage({super.key});

  @override
  State<ComponentGalleryPage> createState() => _ComponentGalleryPageState();
}

class _ComponentGalleryPageState extends State<ComponentGalleryPage> {
  static const _categoryLabels = [
    '按钮与选择',
    '输入与选择器',
    '显示与状态',
    '标签页与分栏',
    '工具栏与窗口',
    '弹窗与主题',
  ];

  var _selectedIndex = 0;

  List<Widget> get _sections => switch (_selectedIndex) {
    0 => const [_ButtonSection(), _SelectionSection()],
    1 => const [_InputSection()],
    2 => const [_DisplaySection()],
    3 => const [_TabSection()],
    4 => const [_AdditionalComponentsSection(), _WindowSection()],
    5 => const [_DialogSection(), _ThemeSection()],
    _ => const [],
  };

  @override
  Widget build(BuildContext context) => MacosWindow(
    sidebar: Sidebar(
      minWidth: 174,
      maxWidth: 220,
      startWidth: 190,
      top: const Padding(
        padding: EdgeInsets.fromLTRB(12, 8, 12, 8),
        child: MacosSearchField(placeholder: '搜索组件'),
      ),
      builder: (context, scrollController) => SidebarItems(
        scrollController: scrollController,
        currentIndex: _selectedIndex,
        onChanged: (index) => setState(() => _selectedIndex = index),
        items: [
          const SidebarItem(
            section: true,
            label: Text('基础控件', maxLines: 1, overflow: TextOverflow.ellipsis),
          ),
          _gallerySidebarItem(
            icon: CupertinoIcons.rectangle_on_rectangle,
            label: '按钮与选择',
          ),
          _gallerySidebarItem(
            icon: CupertinoIcons.slider_horizontal_3,
            label: '输入与选择器',
          ),
          _gallerySidebarItem(
            icon: CupertinoIcons.chart_bar,
            label: '显示与状态',
          ),
          const SidebarItem(
            section: true,
            label: Text('窗口组件', maxLines: 1, overflow: TextOverflow.ellipsis),
          ),
          _gallerySidebarItem(
            icon: CupertinoIcons.sidebar_left,
            label: '标签页与分栏',
          ),
          _gallerySidebarItem(
            icon: Icons.view_quilt,
            label: '工具栏与窗口',
          ),
          _gallerySidebarItem(
            icon: CupertinoIcons.square_stack_3d_up,
            label: '弹窗与主题',
          ),
        ],
      ),
    ),
    child: MacosScaffold(
      toolBar: ToolBar(
        title: Text('macos_ui 组件外观 · ${_categoryLabels[_selectedIndex]}'),
        actions: [
          ToolBarIconButton(
            label: '新建',
            icon: const Icon(Icons.add, size: 16),
            showLabel: true,
          ),
          ToolBarIconButton(
            label: '删除',
            icon: const Icon(Icons.delete_outline, size: 16),
            showLabel: false,
          ),
          const ToolBarSpacer(),
          const ToolBarDivider(),
          ToolBarPullDownButton(
            label: '更多',
            icon: CupertinoIcons.ellipsis_circle,
            items: [
              MacosPulldownMenuItem(
                label: '菜单项一',
                title: const Text('菜单项一'),
                onTap: () {},
              ),
              const MacosPulldownMenuDivider(),
              MacosPulldownMenuItem(
                label: '菜单项二',
                title: const Text('菜单项二'),
                onTap: () {},
              ),
            ],
          ),
          const ToolBarSpacer(),
          ToolBarIconButton(
            label: '溢出菜单',
            icon: const MacosIcon(CupertinoIcons.chevron_right_2, size: 16),
            showLabel: false,
          ),
        ],
      ),
      children: [
        ContentArea(
          builder: (context, scrollController) => SingleChildScrollView(
            key: ValueKey(_selectedIndex),
            controller: scrollController,
            padding: const EdgeInsets.fromLTRB(28, 22, 28, 60),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text(
                  'macos_ui 2.2.2 组件外观示例',
                  style: TextStyle(fontSize: 12),
                ),
                ..._sections,
              ],
            ),
          ),
        ),
      ],
    ),
  );
}

// ============================================================
// 按钮
// ============================================================

class _ButtonSection extends StatelessWidget {
  const _ButtonSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '按钮 Buttons',
    rows: [
      _Row(
        'PushButton · mini',
        PushButton(
          controlSize: ControlSize.mini,
          onPressed: () {},
          child: const Text('mini'),
        ),
      ),
      _Row(
        'PushButton · small',
        PushButton(
          controlSize: ControlSize.small,
          onPressed: () {},
          child: const Text('small'),
        ),
      ),
      _Row(
        'PushButton · regular',
        PushButton(
          controlSize: ControlSize.regular,
          onPressed: () {},
          child: const Text('regular'),
        ),
      ),
      _Row(
        'PushButton · large',
        PushButton(
          controlSize: ControlSize.large,
          onPressed: () {},
          child: const Text('large'),
        ),
      ),
      _Row(
        'PushButton · 次要',
        PushButton(
          controlSize: ControlSize.large,
          secondary: true,
          onPressed: () {},
          child: const Text('次要按钮'),
        ),
      ),
      const _Row(
        'PushButton · 禁用',
        PushButton(
          controlSize: ControlSize.large,
          onPressed: null,
          child: Text('禁用'),
        ),
      ),
      _Row(
        'MacosIconButton',
        Row(
          children: [
            MacosIconButton(
              icon: const Icon(Icons.copy, size: 16),
              onPressed: () {},
            ),
            const SizedBox(width: 12),
            MacosIconButton(
              icon: const Icon(Icons.star, size: 16),
              shape: BoxShape.circle,
              onPressed: () {},
            ),
            const SizedBox(width: 12),
            MacosIconButton(
              icon: const Icon(Icons.delete, size: 16),
              backgroundColor: MacosColors.systemRedColor,
              onPressed: () {},
            ),
            const SizedBox(width: 12),
            MacosIconButton(
              icon: const Icon(Icons.block, size: 16),
              onPressed: null,
            ),
          ],
        ),
      ),
      _Row(
        'MacosPulldownButton',
        MacosPulldownButton(
          title: '下拉按钮',
          items: [
            MacosPulldownMenuItem(
              label: '第一项',
              title: const Text('第一项'),
              onTap: () {},
            ),
            MacosPulldownMenuItem(
              label: '第二项',
              title: const Text('第二项'),
              onTap: () {},
            ),
            const MacosPulldownMenuDivider(),
            MacosPulldownMenuItem(
              label: '禁用项',
              enabled: false,
              title: const Text('禁用项'),
              onTap: () {},
            ),
          ],
        ),
      ),
      _Row(
        'MacosPopupButton',
        SizedBox(
          width: 240,
          child: MacosPopupButton<String>(
            value: '第二项',
            items: const ['第一项', '第二项', '第三项']
                .map(
                  (e) => MacosPopupMenuItem<String>(value: e, child: Text(e)),
                )
                .toList(),
            onChanged: (_) {},
          ),
        ),
      ),
      const _Row(
        'MacosSegmentedControl',
        Align(alignment: Alignment.centerLeft, child: _SegmentControlSample()),
      ),
      _Row(
        'MacosDisclosureButton',
        Row(
          children: [
            MacosDisclosureButton(isPressed: false, onPressed: () {}),
            const SizedBox(width: 20),
            MacosDisclosureButton(isPressed: true, onPressed: () {}),
          ],
        ),
      ),
      _Row('HelpButton', HelpButton(onPressed: () {})),
      _Row('MacosBackButton', MacosBackButton(onPressed: () {})),
    ],
  );
}

class _SegmentControlSample extends StatelessWidget {
  const _SegmentControlSample();

  @override
  Widget build(BuildContext context) => const _SegmentControlInner();
}

class _SegmentControlInner extends StatefulWidget {
  const _SegmentControlInner();

  @override
  State<_SegmentControlInner> createState() => _SegmentControlInnerState();
}

class _SegmentControlInnerState extends State<_SegmentControlInner> {
  final _controller = MacosTabController(initialIndex: 1, length: 3);

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MacosSegmentedControl(
    controller: _controller,
    tabs: const [
      MacosTab(label: '串口'),
      MacosTab(label: 'UDP'),
      MacosTab(label: 'TCP'),
    ],
  );
}

// ============================================================
// 开关与选择
// ============================================================

class _SelectionSection extends StatelessWidget {
  const _SelectionSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '开关与选择 Selection',
    rows: [
      _Row(
        'MacosCheckbox',
        Row(
          children: [
            MacosCheckbox(value: true, onChanged: (_) {}),
            const SizedBox(width: 24),
            MacosCheckbox(value: false, onChanged: (_) {}),
            const SizedBox(width: 24),
            MacosCheckbox(value: null, onChanged: (_) {}),
            const SizedBox(width: 24),
            MacosCheckbox(value: true, size: 20, onChanged: (_) {}),
            const SizedBox(width: 24),
            MacosCheckbox(value: true, onChanged: null),
          ],
        ),
      ),
      _Row(
        'MacosRadioButton',
        Row(
          children: [
            MacosRadioButton<String>(
              value: 'A',
              groupValue: 'A',
              onChanged: (_) {},
            ),
            const SizedBox(width: 8),
            const Text('已选'),
            const SizedBox(width: 28),
            MacosRadioButton<String>(
              value: 'B',
              groupValue: 'A',
              onChanged: (_) {},
            ),
            const SizedBox(width: 8),
            const Text('未选'),
            const SizedBox(width: 28),
            MacosRadioButton<String>(
              value: 'C',
              groupValue: 'A',
              onChanged: null,
            ),
            const SizedBox(width: 8),
            const Text('禁用'),
          ],
        ),
      ),
      _Row(
        'MacosSwitch',
        Row(
          children: [
            MacosSwitch(value: true, size: ControlSize.mini, onChanged: (_) {}),
            const SizedBox(width: 8),
            const Text('mini'),
            const SizedBox(width: 26),
            MacosSwitch(
              value: true,
              size: ControlSize.small,
              onChanged: (_) {},
            ),
            const SizedBox(width: 8),
            const Text('small'),
            const SizedBox(width: 26),
            MacosSwitch(value: true, onChanged: (_) {}),
            const SizedBox(width: 8),
            const Text('regular'),
            const SizedBox(width: 26),
            MacosSwitch(value: false, onChanged: (_) {}),
            const SizedBox(width: 8),
            const Text('关闭'),
            const SizedBox(width: 26),
            MacosSwitch(value: true, onChanged: null),
            const SizedBox(width: 8),
            const Text('禁用'),
          ],
        ),
      ),
      _Row(
        'MacosSlider · 连续',
        SizedBox(
          width: 340,
          child: MacosSlider(value: 0.55, onChanged: (_) {}),
        ),
      ),
      _Row(
        'MacosSlider · 离散',
        SizedBox(
          width: 340,
          child: MacosSlider(
            value: 6,
            min: 0,
            max: 10,
            discrete: true,
            splits: 11,
            onChanged: (_) {},
          ),
        ),
      ),
      _Row('MacosColorWell', MacosColorWell(onColorSelected: (_) {})),
      _Row('MacosDatePicker', MacosDatePicker(onDateChanged: (_) {})),
      _Row('MacosTimePicker', MacosTimePicker(onTimeChanged: (_) {})),
    ],
  );
}

// ============================================================
// 输入
// ============================================================

class _InputSection extends StatelessWidget {
  const _InputSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '输入 Inputs',
    rows: [
      const _Row(
        'MacosTextField',
        SizedBox(width: 340, child: MacosTextField(placeholder: '请输入内容')),
      ),
      const _Row(
        'MacosTextField · 禁用',
        SizedBox(
          width: 340,
          child: MacosTextField(enabled: false, placeholder: '禁用状态'),
        ),
      ),
      _Row(
        'MacosSearchField',
        SizedBox(
          width: 340,
          child: MacosSearchField(
            placeholder: '搜索',
            results: const [
              SearchResultItem('航点'),
              SearchResultItem('参数'),
              SearchResultItem('日志'),
            ],
            onResultSelected: (_) {},
          ),
        ),
      ),
    ],
  );
}

// ============================================================
// 显示与指示
// ============================================================

class _DisplaySection extends StatelessWidget {
  const _DisplaySection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '显示与指示 Display',
    rows: [
      const _Row(
        'Label',
        Label(
          icon: Icon(Icons.info_outline, size: 18),
          text: Text('Label 标题'),
          child: Text('Label 右侧内容'),
        ),
      ),
      const _Row(
        'MacosIcon',
        Row(
          children: [
            MacosIcon(CupertinoIcons.airplane, size: 16),
            SizedBox(width: 20),
            MacosIcon(CupertinoIcons.airplane, size: 24),
            SizedBox(width: 20),
            MacosIcon(CupertinoIcons.airplane, size: 32),
          ],
        ),
      ),
      _Row(
        'MacosTooltip',
        MacosTooltip(
          message: '把鼠标停在按钮上',
          child: PushButton(
            controlSize: ControlSize.regular,
            onPressed: () {},
            child: const Text('悬停查看提示'),
          ),
        ),
      ),
      const _Row(
        'ProgressBar',
        SizedBox(
          width: 340,
          child: Column(
            children: [
              ProgressBar(value: 30),
              SizedBox(height: 12),
              ProgressBar(value: 70),
              SizedBox(height: 12),
              ProgressBar(value: 100, height: 8),
            ],
          ),
        ),
      ),
      const _Row(
        'ProgressCircle',
        Row(
          children: [
            ProgressCircle(value: 25),
            SizedBox(width: 20),
            ProgressCircle(value: 75, radius: 16),
            SizedBox(width: 20),
            ProgressCircle(),
          ],
        ),
      ),
      _Row(
        'RatingIndicator',
        RatingIndicator(value: 3, amount: 5, onChanged: (_) {}),
      ),
      const _Row(
        'RelevanceIndicator',
        Row(
          children: [
            // ignore: deprecated_member_use
            RelevanceIndicator(value: 12, amount: 20, barHeight: 22),
            SizedBox(width: 20),
            // ignore: deprecated_member_use
            RelevanceIndicator(value: 6, amount: 10, barHeight: 22),
          ],
        ),
      ),
      _Row(
        'MacosImageIcon',
        MacosImageIcon(
          const AssetImage(
            'macos/Runner/Assets.xcassets/AppIcon.appiconset/app_icon_32.png',
          ),
          size: 32,
          semanticLabel: '应用图像图标示例',
        ),
      ),
      const _Row(
        'CapacityIndicator',
        SizedBox(
          width: 320,
          child: Column(
            children: [
              CapacityIndicator(value: 35, onChanged: null),
              SizedBox(height: 14),
              CapacityIndicator(value: 80, discrete: true, splits: 10),
            ],
          ),
        ),
      ),
    ],
  );
}

// ============================================================
// 工具栏与扩展组件
// ============================================================

class _AdditionalComponentsSection extends StatelessWidget {
  const _AdditionalComponentsSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '工具栏与扩展组件 Toolbar / More',
    rows: [
      _Row(
        'ToolbarOverflowButton',
        ToolbarOverflowButton(
          overflowContentBuilder: (context) => const ToolbarOverflowMenu(
            children: [
              ToolbarOverflowMenuItem(label: '保存项目'),
              ToolbarOverflowMenuItem(label: '导出数据'),
              ToolbarOverflowMenuItem(label: '删除项目'),
            ],
          ),
        ),
      ),
      const _Row(
        'ToolbarOverflowMenu + ToolbarOverflowMenuItem',
        SizedBox(
          width: 190,
          child: ToolbarOverflowMenu(
            children: [
              ToolbarOverflowMenuItem(label: '保存项目'),
              ToolbarOverflowMenuItem(label: '导出数据'),
              ToolbarOverflowMenuItem(label: '不可用', onPressed: null),
            ],
          ),
        ),
      ),
      _Row(
        'SliverToolBar',
        SizedBox(
          width: 440,
          height: 150,
          child: CustomScrollView(
            slivers: [
              SliverToolBar(
                height: 44,
                automaticallyImplyLeading: false,
                title: const Text('SliverToolBar'),
                actions: [
                  ToolBarIconButton(
                    label: '添加',
                    icon: const Icon(Icons.add, size: 16),
                    showLabel: true,
                  ),
                ],
              ),
              const SliverToBoxAdapter(
                child: SizedBox(
                  height: 100,
                  child: Center(child: Text('可与滚动内容组合的工具栏')),
                ),
              ),
            ],
          ),
        ),
      ),
      _Row(
        'MacosOverlayFilter',
        SizedBox(
          width: 340,
          height: 60,
          child: MacosOverlayFilter(
            borderRadius: BorderRadius.circular(10),
            child: const Center(child: Text('半透明模糊覆盖层')),
          ),
        ),
      ),
    ],
  );
}

class _WindowSection extends StatelessWidget {
  const _WindowSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '窗口与列表 Window / List',
    rows: [
      _Row(
        'MacosScrollbar + MacosListTile',
        SizedBox(
          height: 150,
          width: 360,
          child: MacosScrollbar(
            child: ListView(
              children: const [
                MacosListTile(
                  leading: Icon(Icons.usb, size: 20),
                  title: Text('列表行标题'),
                  subtitle: Text('列表行副标题'),
                ),
                MacosListTile(
                  leading: Icon(Icons.map, size: 20),
                  title: Text('第二行'),
                  subtitle: Text('副标题'),
                ),
                MacosListTile(
                  leading: Icon(Icons.settings, size: 20),
                  title: Text('第三行'),
                  subtitle: Text('副标题'),
                ),
                MacosListTile(
                  leading: Icon(Icons.article, size: 20),
                  title: Text('第四行'),
                  subtitle: Text('副标题'),
                ),
              ],
            ),
          ),
        ),
      ),
      const _Row(
        'TitleBar',
        SizedBox(height: 28, child: TitleBar(title: Text('标题栏 TitleBar'))),
      ),
      _Row(
        'WallpaperTintedArea',
        SizedBox(
          height: 56,
          child: WallpaperTintedArea(
            backgroundColor: MacosTheme.of(context).canvasColor,
            child: const Center(child: Text('桌面雾气着色区域')),
          ),
        ),
      ),
    ],
  );
}

// ============================================================
// 标签页与分栏
// ============================================================

class _TabSection extends StatelessWidget {
  const _TabSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '标签页与分栏 Tabs / Panes',
    rows: [
      const _Row(
        'MacosTabView',
        SizedBox(height: 230, width: 440, child: _TabViewSample()),
      ),
      _Row(
        'ResizablePane',
        SizedBox(
          height: 160,
          width: 460,
          child: ResizablePane.noScrollBar(
            minSize: 120,
            startSize: 180,
            maxSize: 280,
            resizableSide: ResizableSide.right,
            child: const Center(child: Text('可拖动分栏')),
          ),
        ),
      ),
    ],
  );
}

class _TabViewSample extends StatefulWidget {
  const _TabViewSample();

  @override
  State<_TabViewSample> createState() => _TabViewSampleState();
}

class _TabViewSampleState extends State<_TabViewSample> {
  final _controller = MacosTabController(initialIndex: 0, length: 3);

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MacosTabView(
    controller: _controller,
    tabs: const [
      MacosTab(label: '串口'),
      MacosTab(label: 'UDP'),
      MacosTab(label: 'TCP'),
    ],
    children: const [
      Center(child: Text('串口页内容')),
      Center(child: Text('UDP 页内容')),
      Center(child: Text('TCP 页内容')),
    ],
  );
}

// ============================================================
// 弹窗
// ============================================================

class _DialogSection extends StatelessWidget {
  const _DialogSection();

  @override
  Widget build(BuildContext context) => _Section(
    title: '弹窗 Dialogs',
    rows: [
      _Row(
        'MacosAlertDialog',
        PushButton(
          controlSize: ControlSize.large,
          onPressed: () => showMgAlertDialog<void>(
            context: context,
            builder: (dialogContext) => MacosAlertDialog(
              appIcon: const Icon(
                Icons.warning_rounded,
                size: 64,
                color: MacosColors.systemYellowColor,
              ),
              title: const Text('系统警告框'),
              message: const Text('这是 macos_ui 自带的警告框外观。'),
              horizontalActions: true,
              secondaryButton: PushButton(
                controlSize: ControlSize.large,
                secondary: true,
                onPressed: () => Navigator.of(dialogContext).pop(),
                child: const Text('取消'),
              ),
              primaryButton: PushButton(
                controlSize: ControlSize.large,
                onPressed: () => Navigator.of(dialogContext).pop(),
                child: const Text('进一步了解'),
              ),
            ),
          ),
          child: const Text('打开警告框'),
        ),
      ),
      _Row(
        'MacosSheet',
        PushButton(
          controlSize: ControlSize.large,
          onPressed: () => showMgSheet<void>(
            context: context,
            builder: (sheetContext) => MacosSheet(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const Text('MacosSheet 表单弹窗外观'),
                    const SizedBox(height: 16),
                    PushButton(
                      controlSize: ControlSize.large,
                      onPressed: () => Navigator.of(sheetContext).pop(),
                      child: const Text('完成'),
                    ),
                  ],
                ),
              ),
            ),
          ),
          child: const Text('打开表单弹窗'),
        ),
      ),
    ],
  );
}

// ============================================================
// 主题
// ============================================================

class _ThemeSection extends StatelessWidget {
  const _ThemeSection();

  static const _swatches = <String, Color>{
    'label': MacosColors.labelColor,
    'secondaryLabel': MacosColors.secondaryLabelColor,
    'blue': MacosColors.systemBlueColor,
    'green': MacosColors.systemGreenColor,
    'red': MacosColors.systemRedColor,
    'orange': MacosColors.systemOrangeColor,
    'yellow': MacosColors.systemYellowColor,
    'purple': MacosColors.systemPurpleColor,
    'gray': MacosColors.systemGrayColor,
  };

  @override
  Widget build(BuildContext context) {
    final typography = MacosTheme.of(context).typography;
    return _Section(
      title: '主题与字号 Theme',
      rows: [
        _Row(
          'MacosColors',
          Wrap(
            spacing: 12,
            runSpacing: 12,
            children: [
              for (final entry in _swatches.entries)
                Column(
                  children: [
                    Container(
                      width: 60,
                      height: 34,
                      decoration: BoxDecoration(
                        color: entry.value,
                        borderRadius: BorderRadius.circular(6),
                        border: Border.all(
                          color: MacosTheme.of(context).dividerColor,
                        ),
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(entry.key, style: const TextStyle(fontSize: 10)),
                  ],
                ),
            ],
          ),
        ),
        _Row(
          'MacosTypography',
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('title1 标题一', style: typography.title1),
              Text('title2 标题二', style: typography.title2),
              Text('title3 标题三', style: typography.title3),
              Text('headline 强调', style: typography.headline),
              Text('body 正文', style: typography.body),
              Text('callout 说明', style: typography.callout),
              Text('caption1 注释', style: typography.caption1),
              Text('caption2 小注释', style: typography.caption2),
            ],
          ),
        ),
        const _Row('ControlSize', Text('mini / small / regular / large')),
        const _Row(
          'AccentColor',
          Text(
            'blue / purple / pink / red / orange / yellow / green / graphite / multicolor',
          ),
        ),
        _Row(
          'MacosOverlayFilter',
          SizedBox(
            width: 340,
            height: 60,
            child: MacosOverlayFilter(
              borderRadius: BorderRadius.circular(10),
              child: const Center(child: Text('毛玻璃叠加层')),
            ),
          ),
        ),
      ],
    );
  }
}

// ============================================================
// 布局辅助
// ============================================================

class _Section extends StatelessWidget {
  const _Section({required this.title, required this.rows});

  final String title;
  final List<Widget> rows;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      const SizedBox(height: 30),
      Text(
        title,
        style: MacosTheme.of(
          context,
        ).typography.title3.copyWith(fontWeight: FontWeight.w700),
      ),
      const SizedBox(height: 14),
      for (final row in rows) ...[row, const SizedBox(height: 16)],
    ],
  );
}

class _Row extends StatelessWidget {
  const _Row(this.name, this.child);

  final String name;
  final Widget child;

  @override
  Widget build(BuildContext context) => Row(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      SizedBox(
        width: 230,
        child: Text(
          name,
          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
        ),
      ),
      Expanded(
        child: Align(alignment: Alignment.centerLeft, child: child),
      ),
    ],
  );
}

SidebarItem _gallerySidebarItem({required IconData icon, required String label}) =>
    SidebarItem(
      leading: MacosIcon(icon),
      label: Text(label, maxLines: 1, overflow: TextOverflow.ellipsis),
    );
