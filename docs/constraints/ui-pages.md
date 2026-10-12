# 页面规划与控件清单

每个页面登记：用途、布局、使用的组件、映射到的 token。新增页面或改动页面骨架前，必须先在本表中登记（见 `ui.md` 的 U6）。

路径均相对于 `flutter_shell/lib/`。

## 页面

| 页面 | 代码位置 | 用途 | 布局规划 | 现状 |
|---|---|---|---|---|
| 地图主页 | `main.dart`、`map/amap_map_view.dart` | 地图、飞机位置、航点、顶栏状态与命令 | 待规划 | 已有实现 |
| 任务规划面板 | `mission/mission_planning_panel.dart`、`mission/mission_export.dart` | 航点编辑（含高度基准）、上传、清除、围栏与集结点、导出文件 | 航点卡片列表 + 编辑弹层（含高度基准选择）+ 设置页（默认高度基准/高度/速度）+ 导出弹层 | 已有实现，体量大；导出文件（`waypoints` / `plan`）走 `mission_export.dart` 弹层（S3a）；高度基准写入 `frame` |
| 配置工作区 | `gcs_configuration_workspace.dart`、`configuration/` | 参数、校准、端口、系统、签名 | 待规划 | 已有实现 |
| 飞行模式面板 | `widgets/mg_top_status_bar.dart`、`configuration/flight_modes/` | 模式选择、解锁 | 待规划 | 旧的独立浮动面板已删除，统一由顶栏和工作区承载 |
| 遥测仪表 | `widgets/mg_telemetry_dashboard.dart` | 遥测读数 | 待规划 | 已有实现 |
| 连接 | `connection/` | 链路选择与连接 | 待规划 | 已有实现 |
| 机载日志 | `logging/onboard_log_page.dart` | 机载 DataFlash 日志列表、下载、取消、擦除 | 顶栏动作（刷新/擦除）+ 提示行 + 列表行 + 进度卡片 + 上传行 | 已有实现（S1，配置工作区 `logs`） |
| 飞行记录 | `logging/flight_log_page.dart` | 链路 `.tlog` 录制、本地文件列表、离线回放与消息检索 | 录制开关 + 路径行 + 文件列表 + 进度/跳转 + 消息列表 | 已有实现（S2，配置工作区 `flightLog`） |
| 机载文件 | `logging/file_browser_page.dart` | MAVLink FTP 目录浏览、下载、上传、取消；断链时回填本地缓存 | 路径行 + 常用目录 + 缓存/错误提示 + 传输卡片 + 条目列表 + 上传行 | 已有实现（S3，配置工作区 `files`） |
| 相机与云台 | `payload/camera_gimbal_page.dart`、`payload/video_preview.dart`、`payload/video_storage.dart` | 已发现的相机/云台清单与状态、拍照/录像/变焦/云台指向/回中，视频源元数据、本地预览与本机录制/截图 | 三张卡片（相机 / 云台 / 视频源 + 预览）+ 设备选择行 + 读数行 + 命令行 + 录制面板（状态 / 目录 / 容量上限 / 开始停止 / 截图） | 已有实现（S2/S3/S4，配置工作区 `payload`）；本地视频解码/录制见 ADR-0008，后端不可用时整块录制面板不画，只显示「本平台未启用本地视频解码」，不谎报播放 |
| 系统状态 | `configuration/system/system_status_page.dart` | 飞控系统概览、传感器健康与实时遥测状态 | 待补全 | 已有实现（配置工作区 `system`）；用途/布局/组件待补全 |
| 飞控校准 | `configuration/calibration/calibration_page.dart` | 校准标签页入口，统一控制加速度计/磁力计/RC 三页的波形轮询并在退出时关闭 | 待补全 | 已有实现（配置工作区 `calibration`）；用途/布局/组件待补全 |
| 端口配置 | `configuration/ports/ports_configuration_page.dart` | 飞控串口参数配置（含硬件映射） | 待补全 | 已有实现（配置工作区 `ports`）；用途/布局/组件待补全 |
| 电机输出 | `configuration/motors/motor_output_page.dart` | 电机输出参数表，显示实时 PWM 并提供参数编辑 | 待补全 | 已有实现（配置工作区 `motors`）；用途/布局/组件待补全 |
| 飞行模式 | `configuration/flight_modes/flight_modes_page.dart` | 飞行模式 PWM 档位与辅助 RC 通道功能配置 | 待补全 | 已有实现（配置工作区 `flightModes`）；用途/布局/组件待补全 |
| PID 调参 | `configuration/pid/pid_tuning_page.dart`、`configuration/pid/pid_parameters_page.dart` | 按固件决定显示哪些调参标签页（`pid_tuning_page` 为外壳）；`pid_parameters_page` 管理基础/扩展层级与参数元数据加载 | 待补全 | 已有实现（配置工作区 `pid`）；用途/布局/组件待补全 |
| 电池配置 | `configuration/battery/battery_configuration_page.dart` | 电池配置占位页（本次拆分不新增配置功能） | 待补全 | 已有实现（配置工作区 `battery`）；目前是空占位（`SizedBox.expand()`），用途/布局/组件待补全 |
| 解锁检查 | `configuration/arming/arming_checks_page.dart` | 解锁检查参数配置，保留 `ARMING_CHECK`/`SKIPCHK` 的相反位掩码语义 | 待补全 | 已有实现（配置工作区 `arming`）；用途/布局/组件待补全 |
| 全部参数 | `configuration/parameters/all_parameters_page.dart` | 全部参数搜索与分组列表，沿用工作区滚动控制器 | 待补全 | 已有实现（配置工作区 `allParameters`）；用途/布局/组件待补全 |

## 组件

| 组件 | 代码位置 | 用途 | token 映射 | 备注 |
|---|---|---|---|---|
| 长按确认 | `widgets/mg_command_confirmation.dart` | 飞行命令确认（S3） | 待登记 | 500 ms |
| 顶栏状态条 | `widgets/mg_top_status_bar.dart` | 模式、解锁、连接状态 | 待登记 | 模式切换为 500 ms 长按确认；急停入口仅状态新鲜且解锁时出现 |
| 遥测仪表 | `widgets/mg_telemetry_dashboard.dart` | 遥测读数 | 待登记 | |
| 实时波形 | `components/realtime_waveform.dart` | 波形显示 | 待登记 | |
| 地图操作菜单 | `widgets/mg_map_action_menu.dart` | 地图上的命令入口 | 待登记 | 六条命令均经 500 ms 长按确认 |
| 页面内提示 | `widgets/mg_inline_notice.dart` | 单行提示 + 「知道了」按钮 | 待登记 | 机载日志、飞行记录、机载文件三页共用（U5） |

以上为代码盘点结果。布局规划、token 映射和组件归并，需要在后续逐页补全。
