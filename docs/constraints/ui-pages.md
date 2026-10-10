# 页面规划与控件清单

每个页面登记：用途、布局、使用的组件、映射到的 token。新增页面或改动页面骨架前，必须先在本表中登记（见 `ui.md` 的 U7）。

路径均相对于 `flutter_shell/lib/`。

## 页面

| 页面 | 代码位置 | 用途 | 布局规划 | 现状 |
|---|---|---|---|---|
| 地图主页 | `main.dart`、`map/amap_map_view.dart` | 地图、飞机位置、航点、顶栏状态与命令 | 待规划 | 已有实现 |
| 任务规划面板 | `mission/mission_planning_panel.dart` | 航点编辑、上传、清除、围栏与集结点 | 待规划 | 已有实现，体量大 |
| 配置工作区 | `gcs_configuration_workspace.dart`、`configuration/` | 参数、校准、端口、系统、签名 | 待规划 | 已有实现 |
| 飞行模式面板 | `flight/flight_mode_panel.dart` | 模式选择、解锁 | 待规划 | `FlightModeFloatingPanel` 为死代码（KD-13） |
| 遥测仪表 | `widgets/mg_telemetry_dashboard.dart` | 遥测读数 | 待规划 | 已有实现 |
| 连接 | `connection/` | 链路选择与连接 | 待规划 | 已有实现 |

## 组件

| 组件 | 代码位置 | 用途 | token 映射 | 备注 |
|---|---|---|---|---|
| 长按确认 | `widgets/mg_command_confirmation.dart` | 飞行命令确认（S3） | 待登记 | 500 ms |
| 顶栏状态条 | `widgets/mg_top_status_bar.dart` | 模式、解锁、连接状态 | 待登记 | 模式切换为 500 ms 长按确认；急停入口仅解锁时出现 |
| 遥测仪表 | `widgets/mg_telemetry_dashboard.dart` | 遥测读数 | 待登记 | |
| 实时波形 | `components/realtime_waveform.dart` | 波形显示 | 待登记 | |
| 地图操作菜单 | `widgets/mg_map_action_menu.dart` | 地图上的命令入口 | 待登记 | 六条命令均经 500 ms 长按确认 |

以上为代码盘点结果。布局规划、token 映射和组件归并，需要在后续逐页补全。
