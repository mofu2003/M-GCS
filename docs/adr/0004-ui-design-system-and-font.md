# ADR-0004：UI 设计系统与字体

状态：已接受（字体许可**已登记**：`flutter_shell/assets/fonts/MiSans-LICENSE.txt` 随包分发，`flutter_shell/pubspec.yaml:31-35` 已在 `assets` 与 `fonts` 两段登记）
日期：2026-10-09

## 背景

前端需要在 Windows、macOS、Android 上呈现一致的界面，并支持后续更换视觉风格而不大面积重写。字体与颜色如果分散在各页面，就会出现页面之间风格不一致的问题。

## 决定

- 字体族统一为 MiSans，只在 `MgFonts` 中定义。不使用平台专属字体名。
- 字号、颜色、尺寸分别只取自 `MgTypography`、`MgColors`、`MgMetrics`。
- 组件采用 macOS 风格，优先复用已有组件，非必要不手绘。
- 只有标题加粗，其余文字使用 `w400`。
- 打包字体的许可必须允许商用与随软件分发，优先 OFL 或 Apache-2.0。**MiSans 的许可条款已登记**：许可原文随包分发在 `flutter_shell/assets/fonts/MiSans-LICENSE.txt`，并在 `flutter_shell/pubspec.yaml:31-35` 的 `assets`（许可副本）与 `fonts`（Regular/Bold/Semibold 三个字重）两段同时登记。条款要点：小米授予不可转让、非独占、免版税、**可撤销**的全球版权许可，条件是（1）**在软件中特别注明使用了 MiSans**（2）不得改编/二次开发字体（3）不得单独再分发字体本身（用于创作的作品不受此限）。它**不是 OFL/Apache-2.0**，属于自有许可；可撤销这一点需要在发布流程中持续留意（`docs/constraints/release-and-hardware.md`）。

## 后果

- 设计 token 的修改影响全局，需要统一评审。
- CJK 回退链允许包含平台字体名，但只能写在 `MgFonts` 内部，且只用于 MiSans 缺失的生僻字与符号；业务页面只能引用 `MgFonts`，不得自行指定回退。浏览器/系统层面的差异属于 U2 允许的原生集成例外。

## 相关

- `docs/constraints/ui.md`
- `docs/constraints/ui-pages.md`
