# ADR-0007：`send_mtx` 边界定义与适配器生命周期所有权模型

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

状态：草案  
日期：2026-10-12

## 背景

ADR-0005 定义了延迟发送队列，解决了 `impl_->mtx` 内调用外部回调的问题（A3/A4），
但遗留了两个设计层的未定义行为：

### 问题 1：`send_mtx` 的锁序与边界未定义（KD-49）

`mavlink_adapter_internal.h:181-190` 的 `deliver_frame()` 在 `send_mtx` 临界区内
直接调用外部 `send_cb`：

```cpp
bool deliver_frame(const uint8_t* data, size_t size) {
    bool sent = false;
    {
        mg::core::platform::LockGuard send_lock(send_mtx);
        if (!send_cb) return false;
        sent = send_cb(data, size);  // ❌ 回调在锁内执行
    }
    record_frame(data, size);
    return sent;
}
```

这导致两个问题：

**(1) A4 违反**：`send_cb` 是传输层注入的回调（`link->send()`），一旦重入 adapter
（测试夹具、未来的回调式链路）即自锁死。

**(2) A3 潜在违反**：TCP 的 `send()` 在写缓冲满时可能阻塞（即使设置了 `TCP_NODELAY`，
内核缓冲满仍会阻塞）；串口写最长可等待 200 ms（KD-25）。现有锁序是 `mtx → send_mtx`
（`mavlink_adapter.cc:122-124`、`:266-268`、`:314-316`），若外部在 `send_cb` 阻塞
期间尝试反向获取 `send_mtx → mtx`（例如 disconnect → close → 等待 send 完成），
构成 ABBA 死锁。

**当前为何未崩溃**：
- 生产环境 `send_cb` 直接映射到链路层 `send()`，不会重入
- TCP/串口写阻塞时间通常很短（< 10 ms）
- 断链路径的 `reset()` 先 `link->close()` join 读线程，后 `mavlink.reset()`，
  时序上回避了竞态

但这些都是**未文档化的约定**，一旦违反就会触发死锁。

### 问题 2：适配器生命周期与回调捕获的所有权未定义（KD-52）

`connection_manager.cc:413` 的接收回调以**裸指针**捕获适配器：

```cpp
link_ptr->set_receive_callback(
    [this, mav = mavlink_ptr.get()](const uint8_t* data, size_t size) {
        const bool parsed_frame = mav->feed_raw(data, size);
        // ...
    });
```

**当前为何未崩溃**：完全依赖 `reset()` 的严格顺序（`:390` `link->close()` join 读线程
先于 `:392` `mavlink.reset()` 释放适配器），但该顺序约定未被测试或断言保护，一旦被改
（例如为缩短断链耗时改成异步 `close()`）即 use-after-free。

## 决定

### 1. 定义 `send_mtx` 的边界与锁序

#### 1.1 锁的职责

- **`impl_->mtx`**（主锁）：保护 adapter 的全部可变状态（消息队列、超时、处理器、
  回调注册）。
- **`send_mtx`**（发送锁）：**仅**保护 `send_cb` 的注册与检查，**不保护发送行为本身**。

#### 1.2 锁序约定（全局）

```
mtx → send_mtx
```

任何持有 `mtx` 的路径**可以**再获取 `send_mtx`；反向获取（先 `send_mtx` 后 `mtx`）
**禁止**。

#### 1.3 外部回调不在锁内执行（扩展 A4）

`send_cb` **必须**在所有 adapter 锁之外调用，包括 `send_mtx`。调用方式：

```cpp
// ✅ 正确：锁内取副本，锁外调用
std::function<bool(const uint8_t*, size_t)> cb;
{
    mg::core::platform::LockGuard send_lock(send_mtx);
    if (!send_cb) return false;
    cb = send_cb;  // 复制回调对象
}
return cb(data, size);  // 锁外调用
```

#### 1.4 `deliver_frame()` 的修正

修改 `mavlink_adapter_internal.h:181-190`：

```cpp
bool deliver_frame(const uint8_t* data, size_t size) {
    // Step 1: 锁内复制回调副本
    std::function<bool(const uint8_t*, size_t)> cb;
    {
        mg::core::platform::LockGuard send_lock(send_mtx);
        if (!send_cb) return false;
        cb = send_cb;
    }
    
    // Step 2: 锁外调用回调（满足 A3/A4）
    bool sent = cb(data, size);
    
    // Step 3: 记录帧（已在所有锁外）
    record_frame(data, size);
    return sent;
}
```

### 2. 定义适配器生命周期的所有权模型

#### 2.1 所有权规则

- **`ConnectionManager`** 是 `MavlinkAdapter` 与 `ILink` 的**唯一所有者**。
- **回调闭包**不得持有适配器的裸指针，必须使用：
  - `std::weak_ptr<MavlinkAdapter>` 捕获（推荐）
  - 或 `std::shared_ptr<MavlinkAdapter>` 捕获（延长生命周期）

#### 2.2 `wire_callbacks()` 的修正

修改 `connection_manager.cc:408-425`：

```cpp
void ConnectionManager::wire_callbacks() {
    // 调用者必须持有 link_mtx_
    if (!link_ptr || !mavlink_ptr) return;

    // ✅ 使用 weak_ptr 捕获适配器
    std::weak_ptr<mg::core::mavlink::MavlinkAdapter> weak_mav = mavlink_ptr;
    link_ptr->set_receive_callback(
        [this, weak_mav](const uint8_t* data, size_t size) {
            auto mav = weak_mav.lock();
            if (!mav) return;  // 适配器已释放，忽略
            
            const bool parsed_frame = mav->feed_raw(data, size);
            if (parsed_frame && state_.load() == ConnectionState::Connected) {
                record_activity();
            }
        });

    // 发送回调捕获 shared_ptr（ILink 生命周期由 ConnectionManager 管理）
    const std::shared_ptr<ILink> link = link_ptr;
    mavlink_ptr->set_send_callback(
        [link](const uint8_t* data, size_t size) -> bool {
            return link->send(data, size);
        });
}
```

#### 2.3 `reset()` 顺序约定的断言保护

`reset()` 的当前顺序（先 `link->close()` join 读线程，后 `mavlink.reset()`）是安全的，
但需要断言保护防止被静默更改。不直接修改 `reset()` 代码，而是**在测试中验证该顺序**。

### 3. KD-51 的处理路径

第二阶段已完成 FFI 层的统一迁移：原有约 110 处
`get_active_mavlink()` 借用访问全部改为 `shared_active_mavlink()` 租约，覆盖任务、参数、日志、
校准、飞行命令、载荷、配置、遥测和其他 FFI 入口。命令等待辅助函数直接接收
`std::shared_ptr<MavlinkAdapter>`，任务桥接与导航查询也在访问期间持有租约。

因此，FFI 层不再暴露或调用适配器借用访问器；断链时仍由调用方持有的租约保证适配器生命周期。

## 后果

### 正面

- ✅ `send_cb` 不再在任何 adapter 锁内执行，闭环 A3/A4
- ✅ 锁序明确定义，ABBA 死锁路径被阻断
- ✅ 回调捕获裸指针的 use-after-free 风险被消除
- ✅ `reset()` 的顺序约定有测试保护

### 负面

- `deliver_frame()` 多一次回调对象的拷贝（`std::function<>` 的复制构造）
- `wire_callbacks()` 的接收回调多一次 `weak_ptr::lock()` 检查（每帧 ~5-10 ns）

性能代价可忽略（相对于 MAVLink 解析的代价）。

### 兼容性

- 对外 API 不变（FFI 层无修改）
- 内部行为语义等价（回调仍在相同时机触发）
- 测试可能需要调整重入场景的预期行为

## 验证

1. **编译期**：无（锁序是运行时约束）
2. **测试覆盖**：
   - 新增 `test_mavlink_send_callback_reentrancy.cc`：回调重入不死锁
   - 新增 `test_connection_manager_concurrent_disconnect.cc`：并发 disconnect + callback 竞态
   - 扩展 `test_mavlink_protocol.cc`：验证 `send_cb` 在锁外执行
3. **ThreadSanitizer**：在 CI 中启用 TSan 构建（KD-49/52 的竞态条件检测）

## 相关

- `docs/constraints/architecture.md`（A3、A4）
- `docs/adr/0005-deferred-sends-outside-adapter-lock.md`（本 ADR 的前置）
- `core/src/mavlink/mavlink_adapter_internal.h`
- `core/src/transport/connection_manager.cc`

## 实施计划

| 步骤 | 描述 | 工作量 | 状态 |
|---|---|---|---|
| 1 | 修改 `deliver_frame()`（锁内复制、锁外调用） | 0.5 天 | 已完成 |
| 2 | 修改 `wire_callbacks()`（`weak_ptr` 捕获） | 0.5 天 | 已完成 |
| 3 | 补充并发回归测试（重入、竞态、TSan） | 2 天 | 回归、TSan、ASan 定向验证已完成 |
| 4 | 验证修复 | 0.5 天 | 已完成 |
| 5 | 代码审查与 CI 验证 | 1 天 | 阶段二定向构建与验证已完成；完整 preflight 受既有文档空白阻塞 |

**总计**：~4.5 天（约 1 周）
