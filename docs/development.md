# 开发指南

## 环境

- CMake 3.20 及以上，C++17 编译器
- Flutter SDK，版本以 `flutter_shell/pubspec.lock` 为准
- Python 3，用于生成 MAVLink 头文件（脚本会在 `.venv` 中安装 pymavlink）
- 构建 macOS 应用需要 Xcode

## 生成 MAVLink 头文件

首次构建前执行：

```bash
./tools/generate-mavlink.sh
```

Windows 使用 `tools/generate-mavlink.ps1`。输出位于 `third_party/mavlink/include/`，已被 git 忽略。

## 构建共享库

```bash
cmake -S . -B build -DMGCORE_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target mgcore
```

产物位置：macOS 为 `build/mgcore.dylib`，Windows 为 `build/mgcore.dll`，Linux 库名待确认（见 `docs/constraints/known-deviations.md` 的 KD-39）。

## 运行前端

```bash
cd flutter_shell && flutter run -d macos
```

## 后端测试

测试源码位于 `core/tests/`，通过 CTest 标签分组：

- `offline`：不依赖外部资源
- `simulation`：使用回环和模拟飞控
- `hardware`、`manual`：需要真实飞控，默认不运行

```bash
cmake -S . -B build_check -DMGCORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_check
ctest --test-dir build_check -L "offline|simulation" -LE "hardware|manual"
```

## 门禁

提交前执行：

```bash
tools/preflight.sh
```

它依次检查补丁格式、版本一致性、UI token、Flutter 分析与测试、后端测试，以及 macOS debug 构建。

可用选项：

- `--frontend-only`：只跑前端相关检查
- `--backend-only`：只跑后端相关检查
- `--no-macos-build`：跳过 macOS 构建

启用提交钩子：

```bash
tools/install-hooks.sh
```

钩子要求工作区干净，然后运行 `preflight.sh`。

## Docker（仅后端）

```bash
tools/docker-dev.sh check
```

## 发布

遵循 `docs/constraints/release-and-hardware.md`：

1. `tools/release.sh <x.y.z>` 同步版本号。
2. 更新 `CHANGELOG.md`。
3. 本地提交并打标签。
4. 用户明确要求时才推送。
