# 界面字体替代方案调研：找与当前字体高相似度的开放字体

研究日期：2026-09-29

## 结论

当前 mac 端实际渲染的是 **Helvetica Neue（西文/数字）+ PingFang SC（中文）**，两者都是
Apple 系统字体，不可打包再分发（见 [ADR 0007](adr/0007-unified-font-stack.md)）。
调研目标是找**相似度高**且**商业友好许可**的替代。

推荐组合：

| 用途 | 推荐 | 许可 | 体积 | 理由 |
| --- | --- | --- | --- | --- |
| 西文 + 数字 | **Arimo** | SIL OFL 1.1 | 变量字体 496 KB | 社区公认最接近 Helvetica Neue 的免费方案的可变字重版本 |
| 中文 | **Noto Sans SC** | SIL OFL 1.1 | 变量字体 17.7 MB | 许可最干净的开源 CJK 黑体；体积是唯一痛点 |

关键修正：ADR 0007 初稿选的是 **Inter**。调研发现 Inter 属于 SF 系（system-ui 风格），
在社区"替代 Helvetica / Helvetica Neue"的高票讨论里**从未出现**。如果目标是"像现在的
mac 端"，Arimo 的相似度明显更高；如果目标是"现代 UI 观感"而非"像 Helvetica"，
Inter 仍可用。两者都是 OFL，切换成本只是换一个字体文件。

## 调研对象

被替代的三款字体及来源：

- Helvetica Neue：macOS 系统字体，Monotype 商业授权。
- PingFang SC：macOS 系统字体，Apple 版权所有。
- Menlo：macOS 系统字体（本项目已把它并回西文字体，不再单独使用）。

## 西文：社区共识

Graphic Design Stack Exchange 上两个高票问题构成了主要社区共识来源：
"Is there a free 'Helvetica Neue' alternative?"（111 票）与
"Visually similar alternatives to Helvetica?"（24 票）。

| 字体 | 社区票数 | 许可 | 字重覆盖 | 变量字体体积 | 备注 |
| --- | --- | --- | --- | --- | --- |
| Liberation Sans | 83 | SIL OFL 1.1 | R/I/B/BI 四款 | 非变量 | 该问题得票最高的答案，称"渲染完美"；Arial 度量兼容 |
| Arimo | — | SIL OFL 1.1 | 变量 wght | 496 KB | 与 Liberation Sans 同源（Steve Matteson / Croscore 系），补上了可变字重 |
| TeX Gyre Heros | 34 + 10 | GUST Font License | R/I/B/BI + Condensed | 非变量 | 两个问题里都有人称"最好的一个"；Helvetica 克隆，但许可不是 OFL，且字重少 |
| Roboto | 56 | SIL OFL 1.1 | 变量 wght | 489 KB | 被称"Google 的 Helvetica"；但 2014 改版后最像 Helvetica 的特征被削弱 |
| Archivo | 13 | SIL OFL 1.1 | 变量 wght | — | 在需要 Narrow / Black 字重时被推荐 |
| GNU FreeSans | 19 | GPL + 字体例外 | — | — | 答案称"字形几乎一致"，但 **GPL 系被 `docs/CONSTRAINTS.md` 明确禁止** |
| Inter | 0（未出现） | SIL OFL 1.1 | 变量 opsz+wght | 877 KB | 现代 UI 取向，SF 系而非 Helvetica 系 |

结论：**Arimo 是唯一同时满足"社区公认高相似 + OFL 许可 + 完整字重 + 体积小"的选项**。
Liberation Sans 相似度同样高但只有四个字重，不满足本项目正文/强调/标题的多字重需求；
TeX Gyre Heros 相似度最高但许可需要单独记录、字重也不够。

## 中文：开放替代

Graphic Design Stack Exchange 上没有 PingFang 相关的有效讨论（查询返回 0 条），
因此这一侧只能依据各字体项目的一手许可文件判断。

| 字体 | 许可 | 体积 | 备注 |
| --- | --- | --- | --- |
| Noto Sans SC | SIL OFL 1.1 | 变量 17.7 MB | Google Fonts 维护，与 Source Han Sans 同源；许可最干净 |
| Source Han Sans SC | SIL OFL 1.1 | 大（按字重分文件） | Adobe 维护，与 Noto Sans CJK 同源 |
| Sarasa Gothic | SIL OFL 1.1 | 大 | 由 Inter（西文）+ Source Han（中文）等合成，**单一族即可覆盖中西文**，但体积更大 |
| MiSans | 小米自定义许可 | — | 免费商用，但许可要求"在软件中特别注明使用 MiSans 字体"、禁止改编，且授权写明**可撤销** |
| LXGW Neo XiHei | IPA Font License 1.0 | — | 项目 README 明确提示：**用于嵌入式用途需自行评估合规成本**；不是 OFL |
| HarmonyOS Sans | 华为自定义许可 | — | 官方页面为 JS 渲染，未能取到条款原文，需人工核对后再评估 |

结论：**Noto Sans SC 是唯一许可无附加条件的选项**，代价是 17.7 MB。若要压体积，
可选按 GB 18030 常用字集子集化 Noto Sans SC，或改用 MiSans / HarmonyOS Sans 并接受
自定义许可的署名、不可改编、可撤销等附加义务——这三条对本项目都属于实质风险，
尤其"可撤销"与商业产品长期分发相冲突。

Sarasa Gothic 值得单独记一笔：它把 Inter 与 Source Han Sans 合成在一个族里，
如果最终希望"只打包一套字体、连回退链都不要"，它是符合 OFL 的现成方案，
代价是体积比分开打包更大。

## 对 ADR 0007 的影响

ADR 0007 的选型表需要按本调研修正：

- 西文：Inter → **Arimo**（若更看重现代观感可保留 Inter 作为备选）。
- 中文：Noto Sans SC 不变。
- 数字：继续复用西文字体 + `tabularFigures`，不引入第三套。

体积预算从原先估算的约 11 MB 调整为约 **18 MB**（Arimo 0.5 MB + Noto Sans SC 17.7 MB），
中文占了绝大部分，是否可接受需要产品侧拍板。

## 来源

- Graphic Design Stack Exchange，Is there a free 'Helvetica Neue' alternative?
  https://graphicdesign.stackexchange.com/questions/15462/
- Graphic Design Stack Exchange，Visually similar alternatives to Helvetica?
  https://graphicdesign.stackexchange.com/questions/602/
- Google Fonts 字体元数据与许可（`google/fonts` 仓库）：
  Inter `ofl/inter`、Arimo `ofl/arimo`、Roboto `ofl/roboto`、Archivo `ofl/archivo`、
  Public Sans `ofl/publicsans`、Noto Sans SC `ofl/notosanssc`
  https://github.com/google/fonts
- Adobe Source Han Sans 许可 https://github.com/adobe-fonts/source-han-sans
- Sarasa Gothic 许可 https://github.com/be5invis/Sarasa-Gothic
- LXGW Neo XiHei 许可与嵌入说明 https://github.com/lxgw/LxgwNeoXiHei
- MiSans 许可协议 https://hyperos.mi.com/font/zh/download/
- HarmonyOS Sans 资源页 https://developer.huawei.com/consumer/cn/design/resource/
