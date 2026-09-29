# 界面字体替代方案调研：找与当前字体高相似度的开放字体

研究日期：2026-09-29

## 结论

当前 mac 端实际渲染的是 **Helvetica Neue（西文/数字）+ PingFang SC（中文）**，两者都是
Apple 系统字体，不可打包再分发（见 [ADR 0007](adr/0007-unified-font-stack.md)）。

**西文侧有明确答案：TeX Gyre Heros。** 实测纯字形相似度 **90.5%**，远超其他候选。
代价是它只有 Regular / Italic / Bold / BoldItalic 四款，没有 Medium / SemiBold，
而本项目用到 w400 / w600 / w700 三档；且许可为 GUST Font License，不是 OFL。
若要保字重完整，次选 **Arimo**（与 Liberation Sans 同源，OFL，可变字重，相似度 81.8%）。

**中文侧是好消息里带坏消息：没有找到既许可干净、又和 PingFang 足够像的字体。**
许可最干净的 Noto Sans SC 实测只有 **51.5%**，甚至低于衬线字体 Songti SC 的 53.6%，
说明它和 PingFang 只是"同属黑体"，字形差别很大。真正接近的 Hiragino Sans GB 达到
75.6%，但那是 Apple 的字体，同样不可打包。社区常提的 MiSans / HarmonyOS Sans 均属
自定义许可，且本次未能取到可测量的官方字体文件。

## 实测方法

不依赖主观印象，直接量字形重叠度：

1. 用 CoreText 以同一字号把每个字形渲染成灰度位图；
2. 裁到该字形的墨迹外框，消除字体升降部度量差异造成的错位；
3. 拉伸归一化到 128×128，消除字面大小差异，只比形状；
4. 计算二值化后的交并比（IoU），逐字形取平均。

参考字体为运行环境里实际渲染的那一款（西文 Helvetica Neue、中文 PingFang SC）。

局限：IoU 对笔画粗细与末端处理敏感，能反映"形状是否接近"，但不能替代人眼最终确认；
变量字体按默认实例（Regular）参与比较。

## 西文实测：与 Helvetica Neue 的纯字形相似度

| 候选 | 平均相似度 | 最差字形 | 许可 | 字重 |
| --- | --- | --- | --- | --- |
| **TeX Gyre Heros** | **90.5%** | 77.7%（i） | GUST Font License | 4 款 |
| Arimo（与 Liberation Sans 同源） | 81.8% | 15.0%（1） | SIL OFL 1.1 | 可变 wght |
| Archivo | 64.5% | 10.2%（1） | SIL OFL 1.1 | 可变 wght |
| Roboto | 48.8% | 38.5%（Q） | SIL OFL 1.1 | 可变 wght |
| Inter | 46.5% | 32.4%（H） | SIL OFL 1.1 | 可变 opsz+wght |

关键点：

- **TeX Gyre Heros 是 Helvetica 克隆**（源自 URW Nimbus Sans），所以和 Helvetica Neue
  几乎重合。短板是字重只有四档，w600 会被 Flutter 就近映射到 Bold，界面丢掉"半粗"这一层。
- **Arimo 与 Liberation Sans 同源**。社区两个高票问题（Graphic Design Stack Exchange
  15462，得票最高答案 83 票；602）都把 Liberation Sans 列为最佳免费替代，实测支持该结论。
  它提供可变字重，能覆盖 w400–w700，代价是数字 "1" 这类字形与 Helvetica Neue 差异较大。
- **Inter 只有 46.5%**，证实它是 SF 系而非 Helvetica 系。ADR 0007 初稿选它作为
  "高相似"替代是错的，本调研已纠正。
- GNU FreeSans 在社区答案里被称"字形几乎一致"，但许可是 GPL + 字体例外，
  被 `docs/CONSTRAINTS.md` 明确禁止，未参与实测。

## 中文实测：与 PingFang SC 的纯字形相似度

| 候选 | 平均相似度 | 最差字形 | 许可 | 可否打包 |
| --- | --- | --- | --- | --- |
| Hiragino Sans GB | 75.6% | 66.4%（数） | Apple 系统字体 | 否 |
| Songti SC（衬线下限基准） | 53.6% | 31.3%（参） | Apple 系统字体 | 否 |
| Noto Sans SC | 51.5% | 31.5%（查） | SIL OFL 1.1 | 是 |

关键点：

- **Noto Sans SC 只有 51.5%，低于衬线基准 53.6%**。说明"Noto Sans SC 是 PingFang 的
  开源替代"这个常见说法在字形层面站不住：两者只是同属现代黑体，笔画末端、字面比例、
  部件处理都差得很远。用它替换会让中文界面明显变样。
- Hiragino Sans GB 达到 75.6%，说明"接近 PingFang"技术上做得到，但它与 PingFang
  同属 Apple 资产，不可再分发。
- 社区常提的 MiSans、HarmonyOS Sans 未能取得可测量的官方文件：MiSans 官网为 JS 渲染，
  GitHub 镜像不含完整字体；HarmonyOS Sans 镜像仓库以 Git LFS 存放 52 MB 压缩包，
  且该仓库自身声明 GPL-3.0，字体原始许可需另行核实。两者都是自定义许可，
  不满足"许可干净"这一前提。

## 许可与体积

| 字体 | 许可 | 变量字体体积 | 备注 |
| --- | --- | --- | --- |
| TeX Gyre Heros | GUST Font License | 非变量，Regular 约 130 KB | 允许商业使用与再分发，但不属 MIT/BSD/Apache/OFL 体系，按 `docs/CONSTRAINTS.md` 需单独记录 |
| Arimo | SIL OFL 1.1 | 496 KB | 无额外义务 |
| Noto Sans SC | SIL OFL 1.1 | 17.7 MB | 无额外义务；体积与相似度都不理想 |
| MiSans | 小米自定义 | 未取得 | 免费商用，但要求软件内注明、禁止改编，且授权写明**可撤销** |
| HarmonyOS Sans | 华为自定义 | 未取得 | 官方条款页为 JS 渲染，未取到原文，需人工核对 |
| LXGW Neo XiHei | IPA Font License 1.0 | — | 项目 README 明确提示嵌入式使用需自评合规成本 |

体积已确认不作为约束，因此不因体积否决候选。中文侧的问题不是体积，
而是"没有既干净又相似"的选项。

## 对 ADR 0007 的影响

1. 西文选型由 Inter 改为 **TeX Gyre Heros**；若不能接受四档字重，则用 **Arimo**。
   这是"更像"与"字重完整"之间的取舍，需要产品侧拍板。
2. 中文选型暂不成立。Noto Sans SC 实测相似度不足以称为"高相似"，需要三选一：
   接受 Noto Sans SC 的观感变化（许可最干净）；采用 MiSans 或 HarmonyOS Sans
   并先做许可确认；或由人工提供字体文件后按本文方法复测。
3. 数字读数继续复用西文字体 + `tabularFigures`，不引入第三套。

## 待确认

- 西文：TeX Gyre Heros（90.5%，四档字重，GFL）还是 Arimo（81.8%，可变字重，OFL）？
- 中文：接受 Noto Sans SC，评估 MiSans / HarmonyOS Sans 的自定义许可，还是另行提供字体？

## 来源

- Graphic Design Stack Exchange，Is there a free 'Helvetica Neue' alternative?
  https://graphicdesign.stackexchange.com/questions/15462/
- Graphic Design Stack Exchange，Visually similar alternatives to Helvetica?
  https://graphicdesign.stackexchange.com/questions/602/
- Google Fonts 字体元数据与许可（`google/fonts` 仓库）：Arimo、Roboto、Inter、Archivo、
  Noto Sans SC https://github.com/google/fonts
- TeX Gyre Heros（CTAN）https://mirrors.ctan.org/fonts/tex-gyre/opentype/
- MiSans 许可协议 https://hyperos.mi.com/font/zh/download/
- HarmonyOS Sans 资源页 https://developer.huawei.com/consumer/cn/design/resource/
- LXGW Neo XiHei 许可与嵌入说明 https://github.com/lxgw/LxgwNeoXiHei
