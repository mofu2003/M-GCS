# M-GCS 前端架构与本地化约定

## 目标

前端外壳可以替换，连接、飞控状态、任务和参数等业务逻辑不随视觉改版重写；中文和英文资源一一对应，并在测试中持续校验。

## 依赖方向

```text
ConnectionController / MissionEditorController / ParameterController
                 ↓
        Feature model + action callbacks
                 ↓
       SessionCapabilities + domain controllers
                  ↓
       MgFrontstageShellModel/Actions
                 ↓
              MgSkin
                 ↓
      MacGlassSkin（当前实现）
```

- `application/` 只负责连接、同步、任务和参数等业务状态，不读取 `BuildContext`，不依赖 MacGlass 组件。
- `SessionCapabilities` 只负责从当前会话发现可用能力；`FleetController`、`SafetyController` 和任务控制器负责跨页面的状态与长操作，不把这些状态藏在 Widget 内。
- 多机面板通过 `FleetSessionBackend` 和 `FlightControlSessionBackend` 工作；当前批量操作会临时切换 active vehicle 并在结束时恢复，这是 native 显式 `vehicle_id` 路由完成前的兼容方案。
- 飞行日志页面通过 `VehicleFileSessionBackend` 访问 MAVLink FTP 文件树，上传/下载进度属于应用层模型，不依赖具体皮肤。
- `features/frontstage/` 把控制器状态转换为纯模型；`FrontstageControllerAdapter` 是业务到 UI 的适配器。
- `ui/shell/` 定义稳定的壳层 contract。新壳层只需实现 `MgSkin.buildFrontstageShell`，即可复用现有业务逻辑。
- `ui/skins/mac_glass/` 是一个具体视觉实现，不应被业务控制器反向依赖。
- `BackstagePage` 当前仍是功能页实现，页面内部组件可以继续演进；如果出现第二套后台视觉实现，再抽取 `MgBackstageShellModel/Actions`，避免提前增加一层空壳接口。

## 换壳流程

1. 新建一个实现 `MgSkin` 的皮肤或壳层。
2. 消费 `MgFrontstageShellModel` 和 `MgFrontstageShellActions`。
3. 不直接访问 `ConnectionController`、`MissionEditorController` 或参数控制器。
4. 在 `MgcsApp` 的装配点替换 `createDefaultSkin()`，业务模块无需修改。

## 本地化规则

- 所有跨页面文案进入 `app/lib/l10n/app_zh.arb` 和 `app/lib/l10n/app_en.arb`。
- 两个资源文件必须拥有完全相同的业务 key、占位符和非空值。
- 协议名和飞控术语保持标准写法：`Serial`、`UDP`、`TCP`、`RSSI`、`PWM`、`PID`、`Copter`、`Plane`、`QuadPlane`。
- 业务层保存错误码或结构化数据，UI 层通过 `MgLocalizations` 生成当前语言的文案。
- `test/localization_catalog_test.dart` 会校验 key、占位符和关键术语；新增资源后必须通过该测试。
- 新增能力时，先在 `connection_session_backend.dart` 定义语义 contract，再在 `mgcore_connection_session_backend.dart` 做 FFI 适配，最后由 feature/controller 消费；不要在 Widget 中直接拼 JSON 或调用 native。
- 能力缺失时页面应显示不可用或只读状态，而不是假设每个飞控都支持全部 workflow。

## 当前验证

- `flutter analyze`：通过。
- `flutter test`：通过（114 个测试）。
- `flutter build macos --debug`：作为本次改造的最终构建检查。
