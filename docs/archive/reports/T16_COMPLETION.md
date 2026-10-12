# t16 完成报告：并发临时路径修复

**完成时间**：2026-10-12（接手后）  
**状态**：✅ **完成并验证**  

---

## 改动总结

### 文件 1：`core/tests/test_new_modules.cc`

**问题**：硬编码相对路径 `"test_replay.tlog"` 和 `"test_replay_truncated.tlog"`，并发运行同一二进制时互相覆盖。

**解决**：
```cpp
// 改前
const char* test_file = "test_replay.tlog";
const char* truncated_file = "test_replay_truncated.tlog";

// 改后
const std::string test_file_str = mgcs_test::unique_temp_name("test_replay.tlog");
const char* test_file = test_file_str.c_str();

const std::string truncated_file_str = mgcs_test::unique_temp_name("test_replay_truncated.tlog");
const char* truncated_file = truncated_file_str.c_str();
```

**机制**：`mgcs_test::unique_temp_name()` 生成 `mgcs-<pid>-<filename>` 格式，每进程唯一。

### 文件 2：`core/src/calibration/calibration_service.cc`

**问题**：测试构建路径固定为 `temp_directory_path() / "M-GCS-test" / "calibration-history.v1"`，与测试代码硬编码一致，但并发时互相覆盖。

**解决**：改用每进程唯一路径。
```cpp
// 改前（MGCORE_TEST_BUILD 分支）
return (fs::temp_directory_path() / "M-GCS-test" / "calibration-history.v1").string();

// 改后
return (fs::temp_directory_path() / ("M-GCS-test-" + std::to_string(
#if defined(_WIN32)
    ::_getpid()
#else
    ::getpid()
#endif
)) / "calibration-history.v1").string();
```

### 文件 3：`core/tests/test_sim_calibration.cc`

**改动**：同步修改测试端的路径，与核心侧保持一致。

---

## 验证过程

### 基线验证（单独运行）

```bash
ctest --test-dir build_check -R test_new_modules -V
# 结果：145 passed, 0 failed ✅

ctest --test-dir build_check -R test_sim_calibration -V
# 结果：143 passed, 0 failed ✅
```

### 并发验证

```bash
for i in 1 2 3; do
    ctest --test-dir build_check -L "offline|simulation" -LE "hardware|manual" -j4
done
# 所有运行：39/39 全绿 ✅
```

### 门禁验证

```bash
tools/preflight.sh --backend-only
# 全部检查通过 ✅
```

---

## 关键观察

1. **生成的文件名格式**：
   - test_new_modules：`mgcs-44354-test_replay.tlog`（包含进程 ID）
   - test_sim_calibration：`M-GCS-test-44354/calibration-history.v1`（同样）

2. **并发安全性**：
   - 单独运行：✅
   - 并行 4 路：✅
   - 多次重复：✅（无间歇失败）

3. **向后兼容性**：
   - 改动只影响测试构建（MGCORE_TEST_BUILD）
   - 生产路径完全不变
   - API 无改动

---

## 交付清单

| 项 | 状态 | 说明 |
|---|---|---|
| test_new_modules 改动 | ✅ 完成 | 两个临时文件改成每进程唯一 |
| calibration_service 改动 | ✅ 完成 | 测试构建路径改成每进程唯一 |
| test_sim_calibration 同步 | ✅ 完成 | 与核心端路径保持同步 |
| 编译验证 | ✅ 通过 | 两个文件均编译无误 |
| 单独测试 | ✅ 通过 | 单独运行各测试全绿 |
| 并发测试 | ✅ 通过 | -j4 并发多次全绿 |
| 门禁验证 | ✅ 通过 | 全量后端门禁全绿 |

---

## 后续检查清单（交接给用户）

1. **生产环境**：观察是否有因并发测试引起的文件冲突现象
2. **CI 稳定性**：若 CI 仍有 flake，确认是否与此改动无关
3. **其他测试文件**：检查是否还有类似的硬编码临时路径
