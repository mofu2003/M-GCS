# 故障排查

本文件只保留当前仍适用的排障路径。已经完成的 UI 调整和一次性环境问题不在这里记录。

## MgCore 无法加载

症状：应用显示 `MgCore 不可用`，终端出现 native library 加载错误。

检查顺序：

1. 确认后端已经构建出 `mgcore.dylib` 或 `mgcore.dll`。
2. 用 `MGCORE_LIBRARY_PATH` 指向绝对路径，排除当前工作目录差异。
3. 确认库架构与 Flutter 进程一致。
4. 确认 Dart FFI 查找的全部符号都由当前库导出。
5. 正式应用包检查 Frameworks 或可执行文件目录内是否包含库。

运行时不会自动退回演示后端。演示后端只用于测试注入。

## 串口列表为空

1. 确认设备已连接且飞控未解锁。
2. macOS 检查 `/dev/cu.*`，Windows 检查设备管理器中的 COM 端口。
3. 使用后端 `test_scan_connect` 做只读扫描，区分 UI 枚举问题和底层发现问题。
4. 不要为了验证端口而发送解锁、切模式或写参数命令。

## 连接后没有遥测

1. 检查连接诊断中的心跳年龄、目标 sysid/component 和接收计数。
2. `waiting_heartbeat` 表示链路已打开但尚未识别飞控。
3. servo output、RC channels 和电池等数据必须以 `valid` 与 freshness 为准，不能把零值当作真实遥测。
4. 地面站 MAVLink 链路质量与遥控器 RSSI 是不同指标，不能互相替代。

## 参数同步未完成

1. 确认连接状态已完成 bootstrap，而不只是串口打开成功。
2. 检查 `mg_params_get_progress()` 的 received、total 和 active 状态。
3. 参数列表下载是只读流程；不要用写参数操作测试下载是否工作。
4. 飞控断连后应重新建立会话，不复用旧缓存判断完成状态。

## 地图为空或瓦片加载失败

1. macOS Debug/Profile/Release entitlements 都必须允许出站网络。
2. 公共瓦片 URL 可合法但在当前网络不可达；用实际 PNG GET 验证，不只做 HEAD 请求。
3. 自绘背景出现而真实瓦片缺失通常是网络或服务问题，不代表 Flutter 页面未渲染。
4. 生产环境不得依赖无 SLA 的公共瓦片服务。

## Flutter Widget 测试超时

页面包含持续动画时，`pumpAndSettle()` 不会结束。对明确时长的过渡使用固定 `pump(Duration)`，并避免在测试中启动不受控的定位或网络请求。

## 编码显示异常

先按 [编码规范](ENCODING.md) 检查 UTF-8 和终端代码页。终端乱码不等于文件损坏，不要把显示异常的文本写回仓库。

## 默认验证

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check
ctest --test-dir build_check --output-on-failure \
  -L "offline|simulation" -LE "hardware|manual"

cd app
source ~/.zprofile
flutter --no-version-check analyze
flutter --no-version-check test
```
