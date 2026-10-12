# ADR-0006：.tlog 录制的写入器绑定与锁外写盘

状态：已接受
日期：2026-10-11

## 背景

`MavlinkLogWriter` 早已存在，`mg_tlog_start/stop/status` 也能启停，但全仓库
没有任何调用点把链路帧喂给 `write_frame()`——也就是说「录制」实际不产生
任何消息。补上这段接线时必须同时满足两条约束：

- A3（持锁路径禁止阻塞 I/O）：接收路径在 `receive_mtx`/`mtx` 下解析，发送路径
  在 `send_mtx` 下调用 `send_cb`。文件写不能落在这些锁里。
- A4（发送回调不在锁内执行）：`send_cb` 是外部注入回调，录制写盘同样不能在
  锁内触发它可能引发的重入。

历史上 `MavlinkLogWriter` 写在 `core/settings/`，`MavlinkAdapter` 写在
`core/mavlink/`，两者没有依赖关系；`Vehicle` 同时持有二者。

## 决定

1. `MavlinkAdapter` 增加 `set_log_sink(MavlinkLogWriter*)`，由 `Vehicle` 在自己
   的链路适配器上绑定本车的写入器（`wire_telemetry_callbacks_for` 内，幂等）。
   sink 用原子指针保存，跨线程只读。
2. 录制点固定在**所有 adapter 锁之外**：
   - 接收：`feed_raw()` 里 `Impl::process()` 返回之后，把解码消息重新序列化成
     完整帧字节再写盘（`process()` 释放 `mtx` 后才返回）。
   - 发送：`Impl::deliver_frame()` 在 `send_mtx` 释放之后写盘。`send_msg_unlocked()`
     的公开入口、`flush_deferred_frames()` 的延迟队列都复用 `deliver_frame()`，
     因此共享同一录制点。
3. 不新增线程、不新增依赖边。写盘只用 `MavlinkLogWriter` 自己的 mutex；
   未录制时 `record_frame()` 是空操作。
4. 日志规范（对齐 QGC `MAVLinkSigning::serializeUnsignedCopy` 与 MAVLink spec
   §Logging）：`SETUP_SIGNING`（含密钥）绝不入盘；已签名帧剥掉签名块并重算
   校验和，让回放侧按未签名帧解析。

## 后果

- 录制会在接收线程 / FFI 线程上做一次文件写（buffered `fwrite`），但发生在
  adapter 锁之外，不影响链路读线程与状态查询的时延。
- 记录格式由 `core/include/core/log/tlog_format.h` 唯一约束，writer / replayer / QGC / MP
  四方一致；旧版 writer 写的 `3E 00 00 00` + 长度前缀既不是 tlog 也无法回放，
  本次一并修正。
- 发送失败时仍会记录「已尝试发送」的帧（对齐 QGC：日志记录链路意图，
  不代表送达）。真实失败仍由各协议的 ACK/超时上报。

## 相关

- `docs/constraints/architecture.md`（A3、A4、A6）
- `docs/adr/0005-deferred-sends-outside-adapter-lock.md`
- `core/include/core/log/tlog_format.h`、`core/src/mavlink/mavlink_adapter.cc`、
  `core/src/settings/mavlink_log_writer.cc`
