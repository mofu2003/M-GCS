# ADR 0007：界面字体跨平台统一，以 macOS 端为设计基准

## 状态

已采纳并实施。选型为 **MiSans 单套**（同时覆盖汉字与拉丁），
字体文件与 `MgFonts` 改造均已落地，实测数据见「决策」与调研文档。

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
4. **许可必须允许商用与打包**。优先 OFL、Apache-2.0；商业友好的自定义许可
   也可采用，但必须在本文件记录**署名、修改、再分发、可撤销性**四类义务与风险。
   禁止从系统字体目录复制字体（Apple / Microsoft 的系统字体不可再分发）。
5. **字体族总数控制在 1～2 套**。当前选型压到 **1 套**（MiSans），不再需要
   中文回退链。数字对齐由 `FontFeature.tabularFigures()` 保证，不用等宽字体实现
   ——多一套族就多一处跨平台回退风险。
6. **数字不加粗**。数字读数统一 `FontWeight.w400`。界面里数字密度高，加粗会让
   成片读数抢走标题的层级，也让等宽数字的笔画糊在一起；层级由字号与颜色承担。

已落地的第一步（2026-09-29）：删除了 `MgFonts.mono` 与 `MgTypography.mono`，
`MgTypography.data` / `dataLarge` 不再硬编码 `Cascadia Mono`，数字读数统一套用
界面字体族并保留 tabular figures。字体族由三套（Helvetica Neue + PingFang SC +
Menlo）降为两套（西文 + 中文回退）。

### 最终选型：MiSans（单套）

实测数据见 [字体替代方案调研](../../research/FONT_ALTERNATIVES_RESEARCH.md)。MiSans 是唯一
同时覆盖汉字与拉丁、且中文相似度可用的候选，因此全应用只用一套字体：

| 候选 | 拉丁 vs Helvetica Neue | 中文 vs PingFang SC | 结论 |
| --- | --- | --- | --- |
| **MiSans** | 78.4% | 74.7% | **采用**，单套覆盖中英文 |
| Arimo | 81.8% | 无汉字（0/22） | 拉丁略高但覆盖不了中文，需再配一套 |
| HarmonyOS Sans SC | 78.3% | 77.1% | 中文略高，但拉丁与 MiSans 打平，字重档位更少 |
| TeX Gyre Heros | 90.5% | 无汉字 | 最像 Helvetica，但仅 4 档字重且许可非 OFL |
| Noto Sans SC | — | 51.5% | 低于衬线基准 53.6%，不采用 |
| Inter | 46.5% | — | SF 系而非 Helvetica 系，作废 |

Arimo 曾作为西文首选落地过一版（实测 81.8%，拉丁确实更准），但 Arimo 不含任何
汉字，必须再配一套中文字体；改用 MiSans 后全项目只保留一套字体，回退链、
跨平台差异、许可条目都减少一条。已移除 Arimo 文件，需要时可按下节说明重新生成。

### MiSans 许可义务（原文存 `flutter_shell/assets/fonts/MiSans-LICENSE.txt`）

许可允许免费商用与随软件打包，但带四条约束，必须遵守：

1. **署名**：应在软件中特别注明使用了 MiSans 字体。已在系统状态页页脚给出署名。
2. **不可修改**：不得改编或二次开发字体本身。我们只做子集/字重声明以外的任何
   改动都不允许；当前用法是原样打包，未修改字形。
3. **不可单独再分发**：不得单独售卖或再分发字体文件；随软件分发不受限。
4. **可撤销**：授权写明 revocable。这是本选型的主要风险，若小米未来变更授权，
   需要切换到 HarmonyOS Sans SC 或 Noto Sans SC（接口只有 `MgFonts` 一处）。

### 如需重新生成 Arimo（备选路径）

1. 取上游变量字体：`google/fonts` 的 `ofl/arimo/Arimo[wght].ttf`（OFL 1.1）。
2. 实例化静态字重（上游不发静态文件）：
   `fonttools varLib.instancer -o Arimo-Regular.ttf Arimo[wght].ttf wght=400`
   （`wght` 轴范围 400–700，`600` / `700` 同理）。
3. 放回 `flutter_shell/assets/fonts/` 并在 `pubspec.yaml` 声明，再改
   `MgFonts.sans`。

注意：macOS 当前实际渲染用的 Helvetica Neue、PingFang SC、Menlo 都是 Apple 系统字体，
**不可打包再分发**，所以"沿用当前字体"只能取其观感、不能取其文件。

## 影响

正面影响：

- 三个平台的字形完全一致，设计稿与实现可以逐像素对照。
- 消除"字体名写错静默回退"这类问题——打包字体的族名由我们自己定义，写错会立刻
  在本地开发时暴露。
- 中文不再依赖各平台的中文字体，混排稳定性可控。

成本：

- 安装包体积增加约 19.5 MB（MiSans Regular / Semibold / Bold 三个 OTF，各约 6.5 MB）。
  体积已确认不作为淘汰条件。
- 主体授权写明可撤销，属于长期分发风险，需要跟踪许可变更。
- 失去系统字体在中英文混排上的本地化细节。
- macOS 的 CoreText 灰度抗锯齿与 Windows 的 ClearType 属于系统级光栅化差异，
  打包字体消不掉，同一款字在 Windows 上仍会略微更细更锐。

回退方案：本次字体改造独立成一个提交，不满意可直接 revert 回到改动前状态。

## 验收条件

1. `flutter_shell/pubspec.yaml` 声明的字体族与 `assets/fonts/` 下的文件一一对应，
   `MgFonts.sans` 指向同一个族名。
2. `MgFonts` 不再按平台返回不同的西文字体，也不再出现 `SF Pro Text`、`SF Mono`、
   `Menlo`、`Cascadia Mono` 之类的系统字体名。
3. `MgTypography.data` / `dataLarge` 不硬编码字体族，字重为 `w400`，
   并保留 `tabularFigures`。
4. 参数值格、PWM 读数、波形读数等数字显示均为常规字重。
5. 软件内有 MiSans 署名（许可要求）。
6. macOS 与 Windows 上分别截图同一页面，字形一致（允许抗锯齿差异）。
7. 业务页面中不存在字体族名与平台判断；`dart tools/check_ui_tokens.dart` 扩展后
   能拦住新增的系统字体名。
