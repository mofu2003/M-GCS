# 开发约束

这些约束是项目长期边界；突破它们必须先写 ADR。

## 安全

1. 连接、任务、参数、校准、命令、电机和解锁都是安全敏感路径。
2. 未经明确确认，不执行解锁、起飞、降落、任务启动、切模式、电机测试、写参数或危险校准。
3. RTL、设置 Home、任务上传和从飞控读取覆盖本地任务必须有明确的用户确认。
4. 超时、拒绝、能力不可用和断链不能被吞掉，必须返回稳定错误。
5. hardware/manual 测试不进入默认自动化门禁；真实飞控测试前必须确认未解锁。

## 后端和依赖

1. 优先复用官方 MAVLink 定义和生成产物；原始协议细节留在 adapter 内。
2. ArduPilot/PX4 差异通过 firmware profile/capability 表达，调用方不得猜测。
3. 任务、参数、校准、连接和命令生命周期变化必须有回归测试。
4. 依赖方向必须是 `flutter_shell -> bridge/ffi -> core -> third_party`。
5. Core 不 include bridge；持锁路径不做阻塞 I/O、不调用外部回调。
6. 跨平台代码明确区分 Windows、macOS、Linux 和 Android。
7. 任务文档、测绘参数、编译、校验和持久化归后端任务领域；前端不得复制几何或编译。
8. `MissionPlan` 是面向飞控的派生模型，不是可编辑任务文档的唯一事实来源。

## FFI

1. FFI 只使用 C 兼容类型，不暴露 C++ 类、模板、异常和所有权细节。
2. 所有 `char*` 和事件 JSON 由调用方用 `mg_free_string()` 释放。
3. 新接口放入对应 `ffi_api_*.h`，同步实现、测试、API Catalog、文档和前端绑定。
4. JSON 字段稳定；错误使用 `mg_error_t` 或稳定 envelope。
5. 新代码使用显式 session/vehicle 目标，逐步减少 active vehicle 隐式路由。

## Flutter UI

1. 字体统一使用打包的 MiSans，族名只在 `MgFonts` 定义。
2. 字号、颜色和尺寸只能来自 `MgTypography`、`MgColors`、`MgMetrics`。
3. 数字使用 w400 和 tabular figures；只有标题、区块标题、卡片标题和表头可加粗。
4. 新控件先复用现有 `MacosSegmentedControl`、`MacosIconButton`、`MacPanel`、
   `AdaptivePopupButton` 等组件。
5. 页面不写平台字体名、裸颜色、裸字号或重复业务组件。

## 性能

- 顶栏状态约 2Hz，波形约 10Hz；高频数据在后端聚合。
- 定时器只在页面可见时运行；隐藏页面立即取消轮询。
- 数据图表不做隐式动画，不把原始采样经 JSON 逐点传给前端。

## 编码和提交

- 自有文本 UTF-8 + LF；PowerShell 按 `.gitattributes` 保持 CRLF。
- 提交前运行 `git diff --check` 和 `tools/preflight.sh`。
- 发布使用 `tools/release.sh`，同步 CMake、pubspec、README、CHANGELOG；推送必须由用户明确要求。
