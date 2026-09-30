# 故障排查

## 共享库找不到

1. 构建 `mgcore.dylib`、`mgcore.dll` 或 `libmgcore.so`。
2. 用绝对路径确认加载位置和架构（arm64/x86_64）。
3. macOS 用 `nm`，Windows 用 `dumpbin` 检查符号。
4. 确认没有在 `mg_destroy()` 后使用返回指针；所有字符串都要释放。

## 连接后没有遥测

1. 检查 session state：`waiting_heartbeat` 只表示链路打开，不能表示飞控就绪。
2. 检查 diagnostics 的心跳年龄、sysid/component、收包计数和最后错误。
3. servo、RC、电池、Home 和位置都要检查 `valid`/freshness，不能把零值当真值。
4. 地面站链路质量和遥控器 RSSI 是两个指标。
5. Home/飞行状态依赖 `HOME_POSITION` 与 `EXTENDED_SYS_STATE`，连接后 Core 会主动请求。

## 参数同步失败

1. 等待 `mg_params_get_progress()` 的 received 达到 total。
2. 断链后重新建立 session，不复用旧缓存。
3. 批量写入检查每项 succeeded/failed，不只看整体 success。
4. 参数类型和范围以飞控描述元数据为准。

## 任务读取/上传停滞

1. 任务上传返回“开始传输”，不是完成；轮询 `mg_mission_get_progress()`。
2. 从飞控读取先启动 `mg_mission_download()`，等待完成后再调用
   `mg_mission_doc_import_from_vehicle()`。
3. 上传/下载前检查本地文档是否有未保存改动，避免覆盖。
4. 验证必须在一次成功完整下载后调用 `mg_mission_doc_verify()`。
5. 任务传输不增长时检查心跳、目标 sysid/component、MAVLink 丢包和飞控 ACK。

## 校准异常

1. 轮询 `mg_calibration_get_status()`，不要从进度值自行推断终态。
2. 磁力计以 progress/report 消息共同判断；100% 不必然等于完成。
3. 校准历史写盘在锁外；回调可安全查询状态。
4. 参数读回失败应显示失败，不能当作成功。

## UI / FFI

- FFI 找不到符号：重新构建共享库，并确认 Flutter app bundle 嵌入了同一版本库。
- Flutter 页面空白：先确认页面是否有数据状态分支；无数据时也要保留组件骨架。
- UI token 失败：运行 `dart tools/check_ui_tokens.dart`，不要在页面写裸色/字号。

## 默认验证

```bash
tools/preflight.sh
```
