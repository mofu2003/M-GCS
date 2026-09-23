# 项目状态

更新日期：2026-09-23。当前版本：`0.0.5`。分支：`New-main`（后端与接口）。

## 当前结论

`New-main` 只保留可长期维护的 C++ 后端核心和 C FFI 契约，供新的前端实现接入。后端能力覆盖较广，但正式产品链路仍集中在 macOS 串口与 ArduPilot 只读验证，不能把接口存在等同于生产验收。

新前端不依赖本仓库的任何 UI 代码，只通过 `bridge/ffi/ffi_api.h` 的 C ABI 和 JSON 契约交互。

## 已实现

- 串口、TCP、UDP、内存链路、端口发现和连接生命周期；链路失效后由调用方显式重连。
- MAVLink adapter、命令队列、参数缓存、任务传输和遥测缓存。
- ArduPilot/PX4 profile、飞控模型、多机管理和活动飞控选择。
- 参数、任务、围栏、Rally、测绘、校准、日志、MAVLink FTP 文件、ADSB、地形和诊断接口。
- 高语义聚合接口：连接诊断、长任务快照、仪表盘快照和系统诊断。
- 参数变更集：去重、未知参数、`_ENABLE` 最后写入和逐项结果。
- 按 offline、simulation、hardware、manual 分类的 CTest 测试。

## 已知验证

- 2026-07-21：使用 AppleClang、CMake 4.4.0 和 Ninja 1.13.2 配置、编译后端成功；`offline` / `simulation` 门禁为 29/29 通过（18 个 offline、11 个 simulation）。
- ArduPilot Copter 曾完成只读连接、心跳、姿态、固件、参数和部分 servo output 验证。
- 2026-08-12：CI 加入 MAVLink 自动生成；`0.0.2` 后端 Release 构建成功，离线与模拟测试 29/29 通过。
- 2026-09-10：后端 Release 构建成功，离线/模拟门禁 29/29 通过。
- 2026-09-23：native Debug 重建成功，离线/模拟门禁 29/29 通过。
- 2026-09-23：校准事件回调改为锁外派发，`MAG_CAL_PROGRESS` 增加截断报文长度检查，参数写入异常恢复已覆盖。

真实硬件验证未覆盖参数写入、解锁、切模式、任务执行、校准和电机测试。这些能力必须单独设计安全验收。

## 生产缺口

- MAVLink XML 已进入仓库但 C 生成头不提交；本地、Docker 和 CI 统一通过 `tools/generate-mavlink.sh` 使用 `pymavlink 2.4.49` 生成。
- Android filesystem 当前仍是占位实现。
- 固件下载与刷写当前仍是占位实现。
- 视频接收/解码只有基础结构，没有完整生产验收。
- 多机批量命令仍通过“切换活动飞控后执行，再恢复活动飞控”的兼容方案路由；FFI 尚未提供显式 `vehicle_id` 命令目标。
- 日志文件可以浏览和传输，但 DataFlash/TLog 曲线、回放和故障报告尚未完成。
- Mission 命令覆盖、测绘相机模型、地形跟随和离线地图数据管理仍不完整。
- 需要在新前端接入前确定事件、operation id 和错误码的端到端语义。

## 下一阶段

1. 为多机命令增加显式 `vehicle_id` 路由，消除对 active vehicle 的临时依赖。
2. 在 Windows/macOS 真机完成串口、TCP、UDP 只读连接和遥测验收。
3. 完善 Mission CAD/任务命令覆盖、日志分析回放和载荷工作流。
4. 为任何危险写操作建立确认、能力检查、超时、审计和真实硬件验收流程。
5. 在新前端接入时固化事件、operation 状态和错误码契约。
