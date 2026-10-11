# ADR-0005：持锁派发路径的发送改为延迟队列

状态：已接受
日期：2026-10-10

## 背景

`MavlinkAdapter::Impl` 用一把 `mtx` 保护全部可变状态。接收路径是
`feed_raw → Impl::process()`：加锁、分发处理器、跑超时检查，然后解锁执行 `deferred_cbs`。

处理器里有一批"回应飞控"的发送——任务项下发、参数重发、日志续传、命令队列重发、MAVLink FTP 应答。
它们直接调 `Impl::send_msg()`，而 `send_msg()` 最终调用 `send_cb`，也就是传输层的
`link->send()`。串口写最长可等待 200 ms（见 KD-25），于是：

- 违反 `architecture.md` A3（持锁路径禁止阻塞 I/O）：读线程在锁内写串口时，
  FFI 线程的状态查询只能干等。
- 违反 A4（发送回调不在锁内执行）：`send_cb` 是外部注入的回调，一旦它重入
  adapter（测试夹具、未来的回调式链路都可能这样做）就是自锁死。

这几处无法像公开发送入口那样"临时解锁"：处理器的调用点由 `process()` 统一持锁，
逐个把锁传进处理器会把锁的所有权打散。KD-22 因此挂着未修。

## 决定

1. 新增延迟帧队列与显式入队接口：
   - `Impl::queue_msg()`：在锁内把消息打包成字节（含签名与 tx 序号，仍受 `send_mtx` 保护），
     放进 `deferred_frames`，不做任何 I/O。
   - `Impl::flush_deferred_frames()`：在**锁外**统一调用 `send_cb`。
2. 持锁派发路径改用 `queue_msg()`：任务处理器、参数重发、日志续传、命令队列重发。
3. `process()` 与 `tick()` 在解锁后 flush；顺序是"先出帧，再跑 `deferred_cbs`"。
4. MAVLink FTP 客户端的发送回调不能显式改写，用 `thread_local` 派发标记
   （`Impl::tls_dispatching_impl`，与既有的 `s_active_send_cb` 同一手法）让它在
   派发线程内入队、在线程外直发。
5. 公开发送入口维持原语义：仍然"临时解锁 → `send_cb` → 重新加锁"，
   返回值表示真实发送结果，不进入延迟队列。

## 后果

- `send_cb` 不再可能在 `impl_->mtx` 内执行；A3/A4 的持锁约束闭环。
- 延迟队列里的发送返回"已入队"，**真实发送失败只能在 flush 时得知**。
  受影响的只有派发路径：链路写失败时，命令走 ACK 超时、参数写走写入超时、
  任务传输走 MISSION_ACK 超时重试——都已有超时与错误上报，不再有"立即失败"文案。
  公开入口（任务上传/下载/清除、日志状态、命令队列 API）保持立即失败语义。
- 出帧顺序仍在锁内确定，flush 在单个线程内按序执行，线路顺序不变。
- 代价是多一次内存拷贝（一帧 ≤ `MAVLINK_MAX_PACKET_LEN`）。

## 相关

- `docs/constraints/architecture.md`（A3、A4、A6）
- `docs/constraints/known-deviations.md`（KD-22、KD-25）
- `core/src/mavlink/mavlink_adapter_internal.h`、`mavlink_adapter_dispatch.cc`
