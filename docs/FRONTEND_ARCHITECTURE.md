# M-GCS 前端架构与本地化约定

## 目标

前端外壳可以替换，连接、飞控状态、任务和参数等业务逻辑不随视觉改版重写；中文和英文资源一一对应，并在测试中持续校验。

## 产品级 seam

```text
C++ Core
    ↓
C FFI / JSON / Events
    ↓
Frontend Adapter
    ↓
Flutter Presentation（当前第一版）
```

- `core/` 和 `bridge/ffi/` 是可被 Flutter、Qt、原生 Android 或其他前端共同使用的产品后端。
- `app/lib/application/` 只保存前端无关的数据模型、能力接口和确定性工作流，不依赖 Flutter、FFI 或具体 Adapter。
- `app/lib/infrastructure/` 保存 MgCore FFI、JSON decoder、演示和不可用后端等具体 Adapter。
- `app/lib/presentation/controllers/` 保存 Flutter 的 `ChangeNotifier` 状态；这些 controller 属于当前 Flutter 客户端，不被当成跨语言接口。
- `app/lib/presentation/features/` 保存前台、后台、地图和应用 shell；`app/lib/presentation/ui/` 保存 Flutter 皮肤、布局和 MacGlass 视觉。Flutter 代码统一归入 presentation，不再维护并行目录。
- `main.dart` 是 composition root，负责选择并组装具体 backend Adapter。

## 更换前端的方式

1. Flutter 第一版继续通过 `infrastructure/mgcore` 调用现有 C FFI。
2. 新前端实现自己的 Adapter，复用 `core/` 与 `bridge/ffi/`，不复用 Flutter Widget 或 Dart controller。
3. Flutter 内部换视觉仍可使用 `MgSkin`；这只是 Flutter 换皮肤，不等同于整个产品换前端。
4. 新业务能力优先进入 Core/FFI，再由 Flutter 接入，避免把 MAVLink、超时、日志解析或固件规则写进 Widget。

## 本地化规则

- 所有跨页面文案进入 `app/lib/l10n/app_zh.arb` 和 `app/lib/l10n/app_en.arb`。
- 两个资源文件必须拥有完全相同的业务 key、占位符和非空值。
- 协议名和飞控术语保持标准写法：`Serial`、`UDP`、`TCP`、`RSSI`、`PWM`、`PID`、`Copter`、`Plane`、`QuadPlane`。
- 业务层保存错误码或结构化数据，UI 层通过 `MgLocalizations` 生成当前语言的文案。
- `test/localization_catalog_test.dart` 会校验 key、占位符和关键术语；新增资源后必须通过该测试。
- 新增能力时，先在 `application/connection_session_backend.dart` 定义语义 contract，再在 `infrastructure/mgcore/mgcore_connection_session_backend.dart` 做 FFI 适配，最后由 presentation feature/controller 消费；不要在 Widget 中直接拼 JSON 或调用 native。
- 能力缺失时页面应显示不可用或只读状态，而不是假设每个飞控都支持全部 workflow。

## Flutter 第一版约束

Flutter 仍是当前正式前端，可以继续使用 `ChangeNotifier`、Widget、Material 和 MacGlass。需要遵守的规则只有：

1. Widget 不直接导入 `dart:ffi` 或 MgCore implementation。
2. Flutter controller 只调用 `application/` 定义的 capability interface。
3. 协议状态机、命令确认、重试、超时和跨飞控隔离优先位于 Core/FFI。
4. UI 草稿、弹窗、导航、主题和当前选择可以留在 Flutter。
5. 视频、高频波形和三维地图可以使用局部原生渲染，但数据与控制仍走同一后端 seam。

`frontend_architecture_test.dart` 会守护 application、infrastructure 和 presentation controller 的依赖方向。

## 当前验证

- `flutter analyze`：通过。
- `flutter test`：重构后重新运行完整门禁。
- `flutter build macos --debug`：作为本次改造的最终构建检查。
