# 故障排查

本文件只保留当前仍适用的后端排障路径。已经完成的 UI 调整和一次性环境问题不在这里记录。前端侧问题由各前端仓库自行记录。

## 共享库无法加载

症状：调用方报告 native library 加载失败或找不到符号。

检查顺序：

1. 确认后端已经构建出 `mgcore.dylib`、`mgcore.dll` 或 `libmgcore.so`。
2. 用绝对路径加载，排除当前工作目录差异。
3. 确认库架构与宿主进程一致（x86_64 / arm64）。
4. 用 `nm`/`dumpbin` 确认调用方需要的全部符号都由当前库导出。
5. 确认调用方在 `mg_destroy()` 之后不再使用任何返回指针。

所有 `char*` 返回值必须由调用方通过 `mg_free_string()` 释放；事件回调的 `json` 参数同样如此。泄漏字符串不会立刻报错，但会在长时间运行中累积。

## 串口列表为空

1. 确认设备已连接且飞控未解锁。
2. macOS 检查 `/dev/cu.*`，Windows 检查设备管理器中的 COM 端口。
3. 使用 `build_check/test_scan_connect` 做只读扫描，区分调用方枚举问题和底层发现问题。
4. 不要为了验证端口而发送解锁、切模式或写参数命令。

## 连接后没有遥测

1. 检查 `mg_connection_get_diagnostics()` 中的心跳年龄、目标 sysid/component 和接收计数。
2. `waiting_heartbeat` 表示链路已打开但尚未识别飞控。
3. servo output、RC channels 和电池等数据必须以 `valid` 与 freshness 为准，不能把零值当作真实遥测。
4. 地面站 MAVLink 链路质量与遥控器 RSSI 是不同指标，不能互相替代。

## 参数同步未完成

1. 确认连接状态已完成 bootstrap，而不只是串口打开成功。
2. 检查 `mg_params_get_progress()` 的 received、total 和 active 状态。
3. 参数列表下载是只读流程；不要用写参数操作测试下载是否工作。
4. 飞控断连后应重新建立会话，不复用旧缓存判断完成状态。
5. 批量写入使用参数变更集；逐项失败结果必须逐条检查，不能只看整体成功标志。

## 任务传输停滞

1. 用 `mg_operations_get_snapshot()` 确认长任务状态，而不是只看单次返回值。
2. `received` 不再增长通常是链路丢包或飞控未响应，需要先确认心跳和链路质量。
3. 取消使用 `mg_operations_cancel()`；当前对无活动任务返回明确 no-op 结果。
4. 任务上传属于安全敏感操作，不要在未确认飞控状态时重试。

## 校准流程异常

1. 校准状态通过 `mg_calibration_get_status()` 轮询，不要自行推断终态。
2. 罗盘校准以 `MAG_CAL_PROGRESS` 显示进度，以 `MAG_CAL_REPORT` 判定完成；进度到 100% 不代表已完成。
3. 截断或畸形的 `MAG_CAL_PROGRESS` 会被丢弃，这是预期行为，不是数据丢失。
4. 校准回调在释放内部锁之后派发；如果调用方在回调里再次查询状态，是安全且预期的用法。

## 编码显示异常

先按 [编码规范](ENCODING.md) 检查 UTF-8 和终端代码页。终端乱码不等于文件损坏，不要把显示异常的文本写回仓库。

## 默认验证

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check
ctest --test-dir build_check --output-on-failure \
  -L "offline|simulation" -LE "hardware|manual"
```
