# ADR 0007：界面字体跨平台统一，以 macOS 端为设计基准

## 状态

已采纳。实施中：字体文件与 `MgFonts` 改造尚未落地。

## 背景

界面在 macOS 和 Windows 上观感差异明显，根源不是"两个系统本来就不一样"，而是
字体链写错了。

原实现在 `MgFonts` 里按平台各挑一套系统字体，macOS 分支写的是 `SF Pro Text` /
`SF Mono`。但这两个名字在 macOS 上**不是可解析的字体族名**：系统 UI 字体族是
`.AppleSystemUIFont`（PostScript 名 `.SFNS-Regular`），Apple 只通过
`CTFontCreateUIFontForLanguage()` 这类 API 暴露它。

2026-09-29 用 Flutter 实际排版宽度反推，macOS 端真正渲染用的字体是：

| 用途 | 实际字体 | 期望字体 |
| --- | --- | --- |
| 西文 | Helvetica Neue | SF |
| 中文 | PingFang SC | PingFang SC |
| 数字读数 | Menlo | SF Mono |

原因是 Flutter 把 `fontFamily` 与 `fontFamilyFallback` 拼成一个候选列表，取其中
**第一个真实存在**的族：

```text
['SF Pro Text', 'SF Pro Text', 'Helvetica Neue', 'PingFang SC']
       ↑ 不存在         ↑ 不存在        ↑ 命中 → Helvetica Neue
```

也就是说，那条"为了防逐字回退"而写的候选链，恰好把字体从系统默认的 SF 拉到了
Helvetica Neue。同一个提交里 `MgTypography.data` 还硬编码了 `Cascadia Mono`
（Windows 字体），导致数字读数落到 Menlo。

结果是两边素材料完全不同：macOS 是 Helvetica Neue + PingFang SC + Menlo，
Windows 是 Segoe UI Variable Text + Microsoft YaHei UI + Cascadia Mono。字形、
字宽、字重全部不同，中文字形更是两套。再叠加 macOS 的 CoreText 灰度抗锯齿与
Windows 的 ClearType 光栅化差异，观感差距被进一步放大。

关键教训：**字体名写错不会报错**。Flutter 和 Skia 都不会因为族名不存在而失败，
只会静默换一款字体。所以"看起来还行"不能作为字体配置正确的证据。

## 决策

界面字体跨平台统一，并以 macOS 端的设计为基准。

1. **统一，而不是各自原生**。放弃"每个平台用自己的系统字体"。同一段文字在
   macOS / Windows / Linux 上必须由同一款打包字体渲染，这样设计稿、截图、
   bug 复现才有一致的参照。
2. **以 macOS 端为基准**。字体族、字重、字号、行高的取值都以 mac 端效果为准，
   其他平台向它对齐，而不是反过来让 mac 端迁就别的平台。
3. **字体打包进仓库**，放在 `flutter_shell/assets/fonts/`，由 `pubspec.yaml`
   声明，`MgFonts` 单点定义族名。业务页面不得出现字体族名或平台判断。
4. **许可必须商业友好**。本项目是商业项目，只采用 OFL、Apache-2.0 这类许可，
   来源、版本与分发义务记录在本文件。禁止从系统字体目录复制字体（Apple /
   Microsoft 的系统字体不可再分发）。

拟采用的字体（实施时以本文件为准并回填版本号）：

| 用途 | 字体 | 许可 |
| --- | --- | --- |
| 西文 UI | Inter | SIL OFL 1.1 |
| 中文 | Noto Sans SC | SIL OFL 1.1 |
| 数字 / 等宽读数 | JetBrains Mono | SIL OFL 1.1 |

选型理由：Inter 的字面气质最接近 SF / Segoe UI，小字号清晰；Noto Sans SC 中性，
与 Inter 混排不冲突；JetBrains Mono 满足 `tabularFigures` 之外的等宽对齐需求。

## 影响

正面影响：

- 三个平台的字形完全一致，设计稿与实现可以逐像素对照。
- 消除"字体名写错静默回退"这类问题——打包字体的族名由我们自己定义，写错会立刻
  在本地开发时暴露。
- 中文不再依赖各平台的中文字体，混排稳定性可控。

成本：

- 安装包体积增加约 11 MB（Inter ~0.8 MB、Noto Sans SC ~10 MB、JetBrains Mono ~0.2 MB）。
- 失去系统字体在中英文混排上的本地化细节。
- macOS 的 CoreText 灰度抗锯齿与 Windows 的 ClearType 属于系统级光栅化差异，
  打包字体消不掉，同一款字在 Windows 上仍会略微更细更锐。

回退方案：本次字体改造独立成一个提交，不满意可直接 revert 回到改动前状态。

## 验收条件

1. `flutter_shell/pubspec.yaml` 声明了打包字体，`assets/fonts/` 下的字体文件
   与 `MgFonts` 中引用的族名一一对应。
2. `MgFonts` 不再出现 `Platform.isMacOS` / `Platform.isWindows` 分支，也不再
   出现 `SF Pro Text`、`SF Mono`、`Menlo`、`Cascadia Mono` 之类的系统字体名。
3. `MgTypography.data` / `dataLarge` 不再硬编码字体族，数字读数走 `MgFonts.mono`。
4. macOS 与 Windows 上分别截图同一页面，字形一致（允许抗锯齿差异）。
5. 业务页面中不存在字体族名与平台判断；`dart tools/check_ui_tokens.dart` 扩展后
   能拦住新增的系统字体名。
