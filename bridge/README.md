# FFI 桥接说明

`bridge/ffi/` 是 C++ 后端核心与外部消费者之间的契约边界。任何前端、测试工具或其他外部调用方都应通过这里的 C API 访问后端能力。

## 目标

- 通过纯 C API 暴露后端能力。
- 隐藏 C++ 类型、生命周期和所有权规则。
- 把结果归一化为稳定 JSON envelope。
- 把后端失败映射为明确的 `mg_error_t`。
- 保持前端或外部绑定重建时底层能力不被重写。

## 重要文件

- `ffi_api.h`：umbrella header，聚合所有领域 API。
- `ffi_api_core.h`：生命周期、错误、事件回调、版本和核心公共类型。
- `ffi_api_*.h`：按连接、遥测、任务、参数、飞行、日志等领域拆分的 C API。
- `ffi_core_context.h`：`mg_core_t` 内部状态、字符串分配工具和共享运行态。
- `ffi_internal.h`：桥接适配器共享的内部工具和活动飞控路由。
- `ffi_error.cc`：错误码映射和 last-error 处理。
- `ffi_vehicle_manager.cc`：多机打开、关闭、选择和设备发现。
- `ffi_diagnostics.cc`：聚合连接、操作、遥测和系统诊断。
- `ffi_*.cc`：按功能拆分的适配器，例如命令、参数、任务、遥测、设置和校准。
- `API_CATALOG.md`：公开 API 的分组、稳定性和兼容入口策略。

## 高语义接口

前端接入和现场排障优先使用聚合接口，底层细粒度接口继续保留：

- `mg_connection_get_diagnostics()`：连接、心跳、活动飞控和目标系统诊断。
- `mg_operations_get_snapshot()`：参数同步和任务传输的统一操作视图。
- `mg_operations_cancel()`：统一取消参数、任务、校准和日志操作；无活动操作时返回
  `canceled:false`，不会把已完成或失败的传输追认为取消。
- `mg_telemetry_get_dashboard_snapshot()`：仪表盘所需遥测、freshness 和 validity 聚合。
- `mg_system_get_diagnostics()`：一次性导出系统诊断。

## 字符串和内存规则

1. 普通 FFI 函数返回的 `char*` 必须由调用方通过 `mg_free_string()` 释放。
2. `mg_event_callback_t` 回调收到的 `json` 参数也由回调方通过 `mg_free_string()` 释放。
3. FFI 边界不得要求外部调用方释放 C++ 对象或理解 C++ 所有权。
4. 不得把栈上字符串、静态临时缓冲或 C++ `std::string::c_str()` 直接返回给外部。

## 契约规则

1. 不得跨 FFI 边界暴露 C++ 类、模板、异常或所有权规则。
2. success/error JSON 结构必须保持稳定。
3. 超时、拒绝、能力不可用等操作者相关结果必须使用明确错误码。
4. 外部可见行为变化必须同步更新文档和测试。
5. 新增接口前先确认已有接口是否可复用，不能凭空创造重复接口。
6. 新增 API 必须放入合适的 `ffi_api_*.h` 并由 `ffi_api.h` 暴露。
7. 不得复用或扩展标记为 compatibility 的入口；新调用方使用目录中指定的首选 API。
8. `mg_vehicle_open()` 只创建会话，不改变活动焦点；需要使用旧 active-vehicle 路由时必须显式调用 `mg_vehicle_select()`。
