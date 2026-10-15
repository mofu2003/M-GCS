# ADR-0002：上游参考与许可

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

状态：已接受
日期：2026-10-09

## 背景

M-GCS 对标 QGroundControl 与 Mission Planner。两者各有所长，许可证也不同：QGC 当前 checkout 为 Apache-2.0 与 GPLv3 双许可，MP 为 GPLv3，M-GCS 为 MIT。

## 决定

- 按模块分工：链路生命周期、状态机、测试治理、跨平台结构参考 QGC；ArduPilot 专用的命令、参数、校准、固件行为参考 MP。
- 只参考行为与协议事实，不复制源码。M-GCS 保持 MIT。
- 开发功能时，可按需参考对应的 QGC 与 MP 源码。
- 引用 QGC 的 Apache-2.0 分支代码之前，需要核对 Qt 商业授权与再分发义务，并在本 ADR 中补充记录。
- 再分发上游数据文件时，必须在仓库内保留版权与许可声明，并记录来源 commit 与校验和。
- 上游数据文件的许可分支**一律取 Apache-2.0**：GPLv3 分支的代码与数据不进入 M-GCS（MIT 与 GPLv3 不兼容）。衍生作品还必须按 Apache-2.0 §4(b) 显著声明所做的修改。
- **GPLv3 上游的文本一律不得内嵌**：ArduPilot 是 GPLv3（`COPYING.txt`），其 `@Description` / `@DisplayName` 是源码的一部分。MIT 项目内嵌这些文本构成许可冲突，因此：
  - 只保留**结构性事实**（参数名、类型、范围、单位、默认值、增量、枚举数值）——这些是接口事实，不受版权保护；
  - 散文式描述**不入库**，需要时参照 Mission Planner 的做法在运行时从 `autotest.ardupilot.org` 获取；
  - 中文名称与说明必须是**我们自己的表述**（从事实出发独立撰写，不是逐句翻译），见 `core/src/config/ParameterTranslations.zh-CN.json`。
- 已按本条约办理的文件：
  - `core/src/transport/USBBoardInfo.json`（QGC `src/Comms/Serial/USBBoardInfo.json`，commit `bf0096d97`）。**衍生作品**：boardInfo 75 条中 52 条来自上游、23 条为本仓新增；boardDescriptionFallback 51 条与上游逐条相同。声明见同目录 `USBBoardInfo.LICENSE.txt`。
  - `core/src/config/ParameterDescriptors.json`（ArduPilot `ParameterRepository`，`Copter-4.8/apm.pdef.json` 等）。**只保留结构字段**：原 11,655 条 `@Description` 与 `@DisplayName` 已于 2026-10 移除（逐字比对确认过是 ArduPilot 原文），文件内 `metadata_note` 记录理由。

## 后果

- 每个功能的规划周期会变长，但功能效果有可追溯的参考依据。
- 对照表需要持续维护。

## 相关

- `docs/constraints/upstream.md`
