# ADR-0002：上游参考与许可

状态：已接受
日期：2026-10-09

## 背景

M-GCS 对标 QGroundControl 与 Mission Planner。两者各有所长，许可证也不同：QGC 当前 checkout 为 Apache-2.0 与 GPLv3 双许可，MP 为 GPLv3，M-GCS 为 MIT。

## 决定

- 按模块分工：链路生命周期、状态机、测试治理、跨平台结构参考 QGC；ArduPilot 专用的命令、参数、校准、固件行为参考 MP。
- 只参考行为与协议事实，不复制源码。M-GCS 保持 MIT。
- 开发任何功能前，先阅读对应的 QGC 与 MP 源码，并把参考记录在 `docs/constraints/parity.md` 中。
- 引用 QGC 的 Apache-2.0 分支代码之前，需要核对 Qt 商业授权与再分发义务，并在本 ADR 中补充记录。已发生一次例外，见下一节：引用的是纯数据（PX4 生成的参数元数据 JSON），不是 Qt 代码。
- 再分发上游数据文件时，必须在仓库内保留版权与许可声明，并记录来源 commit 与校验和。已按此办理的清单：
  - `core/src/config/PX4ParameterMetaData.json`（PX4-Autopilot BSD-3-Clause，取自 QGC `bf0096d97`）；声明见 `core/src/config/PX4ParameterMetaData.LICENSE.txt`，决策见 ADR-0007。

## 后果

- 每个功能的规划周期会变长，但功能效果有可追溯的参考依据。
- 对照表需要持续维护。

## 相关

- `docs/constraints/upstream.md`
- `docs/constraints/parity.md`
- `docs/adr/0007-px4-parameter-metadata.md`（上游数据文件的许可与来源记录）
