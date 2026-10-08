# M-GCS 与 QGC / MP 功能差距分析

当前基线：`0.0.79`

本文用于回答两个问题：

1. M-GCS 当前相对 QGroundControl（QGC）和 Mission Planner（MP）缺少什么？
2. M-GCS 是独立实现、二次开发，还是对上游的复刻？

本文比较的是本地源码快照：

- QGC：commit `bf0096d974dc2792556c61ca783779083d9de41c`，
  `v5.2.0-dev-134-gbf0096d97`
- MP：commit `a2fcd74d64513d0d8fb43fbdd72020fa75aaa974`
- M-GCS：`0.0.79`

## 一、项目定位

M-GCS 是**独立自研、兼容 QGC/MP 工作流的地面站**，不是 QGC/MP 的
源码分支，也不是界面和功能的完整复刻。

判断依据：

- 技术栈不同：M-GCS 使用 `C++17 Core + C FFI + Flutter`；QGC 主体是
  `C++/Qt/QML`；MP 是 `C#/.NET/WinForms`。
- 架构不同：M-GCS 使用自有 `MissionDocument`、`ConnectionManager`、
  `Vehicle` 和 JSON FFI 契约；没有引入 QGC 的 Qt 对象树或 MP 的全局
  `MainV2.comPort` 结构。
- 依赖不同：`third_party/` 当前只包含官方 MAVLink 和 cJSON 等通用依赖。
- 许可证不同：M-GCS 是 MIT；当前 QGC checkout 是 Apache-2.0/GPLv3
  双许可，MP 是 GPLv3。直接复制大段上游源码必须单独做许可证和授权评估。
- 借鉴方式：任务传输、错误语义、任务文件互操作、校准流程等按模块参考
  QGC/MP，但实现落在 M-GCS 自己的边界内。

更准确的产品表述是：

> 面向 ArduPilot 为主、兼容 PX4 的独立地面站；按模块对齐 QGC/MP 的
> 协议事实、状态机、字段和工作流，不复用其 UI 框架或代码分支。

## 二、当前已具备

### 连接与基础飞行

- 串口、TCP、UDP、内存链路、session/vehicle 目标模型。
- heartbeat、断连回收、连接诊断、活动飞控切换。
- 地图、Home、飞机位置、姿态仪、罗盘、遥测仪表条和飞控消息。
- 解锁、上锁、起飞、降落、返航、暂停、改变高度和航向、Go to、
  Orbit、ROI、估计原点等引导动作。
- 长按确认门控统一为 0.5 秒，确认成功后才下发危险命令。

### 任务规划

- `MissionDocument` 编辑、undo/redo、校验、估算和编译。
- SimpleItem、Home、默认高度/速度。
- 从飞控读取、上传、回读核对、清除和脏状态保护。
- `.waypoints`（QGC WPL 110）和 QGC `.plan` 导入导出。
- 固定翼/VTOL 相关任务语义、Home 动态同步区分。
- GeoFence 和 Rally 独立 MAVLink mission type 的编译、上传、下载、
  清除和 verify。
- 参数化 `survey_grid` / `corridor` 后端模型和 QGC ComplexItem 展开。

### 参数、校准与状态

- 参数搜索、分组、描述、类型/范围校验、批量刷新和写入状态。
- ArduPilot 参数重映射、canonical 覆盖率、未分类参数回退。
- 加速度、磁力计、遥控器校准；陀螺仪、气压计和空速后端接口已有。
- 系统状态、传感器健康、端口配置、电机输出、飞行模式、PID 和解锁检查
  的基础页面。
- 统一 FFI 长任务快照和取消入口。

## 三、相对 QGC/MP 的主要缺口

| 领域 | 当前状态 | 主要缺口 |
| --- | --- | --- |
| 固件 | 占位 | 固件发现、下载、bootloader 刷写、断电恢复、版本校验；当前 `mg_firmware_flash` 返回 not supported |
| 飞控配置 | 部分完成 | 机架类型、机型模板、电池、失效保护、ESC、传感器、GPS/罗盘顺序、OSD、降落伞、Remote ID、舵机/继电器完整配置 |
| 校准 | 部分完成 | 加速度/磁力计/遥控器已有；陀螺仪、气压计、空速计完整 UI，传感器方向和容差向导 |
| 日志与分析 | 后端接口已有，UI 缺 | Onboard Logs、DataFlash/ULog 查看器、遥测回放、振动/FFT、GeoTag、日志参数分析 |
| MAVLink 诊断 | 缺 | MAVLink Inspector、Console、报文速率/丢包/错误图表、原始消息过滤 |
| 任务规划 | 基础闭环完成 | Survey/Corridor 可视创建编辑、Structure Scan、固定翼着陆模式、任务模板、KML/SHP/GeoJSON、测量和变换工具 |
| 围栏/Rally | 传输闭环完成，编辑缺 | 地图上直接画多边形/圆、编辑顶点、编辑 Rally 点和统一规划界面 |
| QGC ComplexItem | 可保留/展开，编辑只读 | ComplexItem 内部字段的创建、编辑和参数化回写 |
| 地形 | 后端查询已有，规划未接 | 高程剖面、地形冲突检查、地形跟随、地形瓦片缓存 |
| 地图 | 在线底图可用 | 离线瓦片缓存、WMS/WMTS/自定义源、3D/倾斜视图、空域和禁飞区图层 |
| 相机/云台 | 后端接口已有，UI 缺 | 相机定义、拍照/录像、云台管理、视频流、录制和 HUD 叠加 |
| 摇杆/Follow Me | 暂不做 | 手柄输入、虚拟摇杆、Follow Me、辅助轴和按钮动作映射 |
| 多机 | 后端 session 基础已有，UI 缺 | 多机列表、焦点切换、多机地图、分机任务和状态管理 |
| RTK/GPS | 遥测显示已有 | NTRIP、RTK 基站、GPS 注入、移动基站、定位源诊断 |
| 高级生态 | 大部分缺 | ADS-B 交通显示、天线跟踪器、DroneCAN/UAVCAN、Lua/脚本、REPL、插件、蜂群、Remote ID |
| 平台 | macOS 已验收 | Windows/Linux 串口和真机流程、Android USB 权限/插拔、iOS 支持 |
| 产品化 | 基础完成 | 单位系统、国际化、完整设置持久化、更新检查、崩溃诊断、引导页、离线帮助 |

## 四、建议优先级

### P0：安全和单机可用性

1. 固件刷写：实现完整流程，或正式标记为 out of scope。
2. 完整校准和飞控安全配置：机架、传感器、电池、失效保护。
3. 飞行前检查清单和关键遥测缺失提示。

### P1：现场排障

1. Onboard Logs、日志查看器和遥测回放。
2. MAVLink Inspector/Console。
3. 连接质量、丢包、消息速率和参数写入诊断。

### P2：规划与现场任务

1. 围栏/Rally 可视化编辑。
2. Survey/Corridor/Structure Scan/固定翼着陆等 ComplexItem 编辑。
3. 地形剖面、高程冲突和离线地图缓存。
4. KML/SHP/GeoJSON 和测量工具。

### P3：载荷与扩展

1. 相机、视频、云台。
2. NTRIP/RTK 和 Follow Me。
3. 多机、ADS-B、DroneCAN、脚本和插件。

以上功能不应被机械地“全部照搬”。如果 M-GCS 的目标是面向单机
ArduPilot 现场作业，蜂群、天线跟踪器、传统 OSD、Remote ID 等能力可以
明确保持 out of scope。

## 五、结论

M-GCS 当前是“独立自研 + 上游协议兼容 + 模块级语义借鉴”，不是 QGC/MP
的二次开发分支，也不是完整复刻。它已经形成可用的单机任务、参数、校准、
连接和地图基础，但距离 QGC/MP 的完整生态仍缺少固件配置、日志分析、
复杂任务编辑、地形/离线地图、相机视频和多机扩展等能力。
