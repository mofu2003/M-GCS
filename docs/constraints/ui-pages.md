# 页面规划与控件清单

每个页面登记：用途、布局、使用的组件、映射到的 token。新增页面或改动页面骨架前，必须先在本表中登记（见 `ui.md` 的 U6）。

路径均相对于 `flutter_shell/lib/`。

## 页面

| 页面 | 代码位置 | 用途 | 布局规划 | 现状 |
|---|---|---|---|---|
| 地图主页 | `main.dart`、`map/amap_map_view.dart` | 地图、飞机位置、航点、顶栏状态与命令 | 待规划 | 已有实现 |
| 任务规划面板 | `mission/mission_planning_panel.dart`、`mission/mission_export.dart` | 航点编辑（含高度基准）、上传、清除、围栏与集结点、导出文件 | 航点卡片列表 + 编辑弹层（含高度基准选择）+ 设置页（默认高度基准/高度/速度）+ 导出弹层 | 已有实现，体量大；导出文件（`waypoints` / `plan`）走 `mission_export.dart` 弹层（S3a）；高度基准（相对 Home / 地形跟随 AGL）写入 `frame`（S3b） |
| 配置工作区 | `gcs_configuration_workspace.dart`、`configuration/` | 参数、校准、端口、系统、签名 | 待规划 | 已有实现 |
| 飞行模式面板 | `widgets/mg_top_status_bar.dart`、`configuration/flight_modes/` | 模式选择、解锁 | 待规划 | 旧的独立浮动面板已删除，统一由顶栏和工作区承载 |
| 遥测仪表 | `widgets/mg_telemetry_dashboard.dart` | 遥测读数 | 待规划 | 已有实现 |
| 连接 | `connection/` | 链路选择与连接 | 待规划 | 已有实现 |
| 测绘规划 | `survey/survey_planning_page.dart` | 测绘参数编辑（含高度基准）、生成预览、并入任务 | 模式分段控件 + 两张参数卡（区域/航线含高度基准、相机与重叠）+ 生成行 + 预览统计卡 | 已有实现（S2，配置工作区 `survey`）；高度基准（相对 Home / 地形跟随 AGL）并入任务时写入 `frame`（S3b） |
| 机载日志 | `logging/onboard_log_page.dart` | 机载 DataFlash 日志列表、下载、取消、擦除 | 顶栏动作（刷新/擦除）+ 提示行 + 列表行 + 进度卡片 + 上传行 | 已有实现（S1，配置工作区 `logs`） |
| 飞行记录 | `logging/flight_log_page.dart` | 链路 `.tlog` 录制、本地文件列表、离线回放与消息检索 | 录制开关 + 路径行 + 文件列表 + 进度/跳转 + 消息列表 | 已有实现（S2，配置工作区 `flightLog`） |
| 机载文件 | `logging/file_browser_page.dart` | MAVLink FTP 目录浏览、下载、上传、取消；断链时回填本地缓存 | 路径行 + 常用目录 + 缓存/错误提示 + 传输卡片 + 条目列表 + 上传行 | 已有实现（S3，配置工作区 `files`） |
| 相机与云台 | `payload/camera_gimbal_page.dart` | 已发现的相机/云台清单与状态、拍照/录像/变焦/云台指向/回中，视频源元数据与流选择 | 三张卡片（相机 / 云台 / 视频源）+ 设备选择行 + 读数行 + 命令行 | 已有实现（S2，配置工作区 `payload`）；视频本地播放留待 S3 |

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
