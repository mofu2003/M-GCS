/**
 * mgcs_test_check.h - core/tests 里唯一的断言宏
 *
 * 历史问题：每个测试文件各写一份 CHECK，参数顺序分叉成两种——
 * 多数是 (条件, 消息)，但 test_sim_fence_rally.cc / test_sim_full_flow.cc /
 * test_sim_params.cc / test_sim_telemetry.cc 以及 TEST()/CHECK(name, expr) 系列
 * 是 (消息, 条件)。跨文件复制调用点时抄错一次，条件就退化成字符串字面量（恒真），
 * 断言静默失效——正是 `docs/constraints/testing.md` 的 T2 禁止的假断言。
 *
 * 现在只有这一处定义，参数顺序固定为 (条件, 消息)。测试文件只包含本头文件，
 * 计数器和汇总输出由 mgcs_test::passed()/failed() 提供。
 *
 * `tools/check_fake_assertions.py` 在门禁里静态检查假断言写法（T2）。
 */

#ifndef MGCORE_TESTS_MGCS_TEST_CHECK_H
#define MGCORE_TESTS_MGCS_TEST_CHECK_H

#include <cmath>
#include <cstdio>

namespace mgcs_test {

/// 通过的断言数。共享头是唯一累加它的地方。
inline int& passed() {
    static int value = 0;
    return value;
}

/// 失败的断言数。
inline int& failed() {
    static int value = 0;
    return value;
}

/// ctest 退出码：只要有一条断言失败就是 1。
inline int exit_code() { return failed() > 0 ? 1 : 0; }

} // namespace mgcs_test

/// 唯一的断言宏：CHECK(条件, 消息)。
#define CHECK(cond, msg) do {                                                  \
    if (cond) {                                                                \
        ++mgcs_test::passed();                                                 \
        std::fprintf(stderr, "  [PASS] %s\n", (msg));                          \
    } else {                                                                   \
        ++mgcs_test::failed();                                                 \
        std::fprintf(stderr, "  [FAIL] %s\n", (msg));                          \
    }                                                                          \
} while (0)

/// 浮点近似断言：CHECK_NEAR(实际值, 期望值, 容差, 消息)。消息同样在最后一位。
#define CHECK_NEAR(actual, expected, eps, msg) \
    CHECK(std::abs((actual) - (expected)) < (eps), (msg))

/// 汇总行：=== Results: N passed, M failed ===
#define MGCS_TEST_REPORT()                                                     \
    std::fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n",          \
                 mgcs_test::passed(), mgcs_test::failed())

#endif // MGCORE_TESTS_MGCS_TEST_CHECK_H
