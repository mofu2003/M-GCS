#!/usr/bin/env python3
"""F1 守卫：`ffi_api*.h` 的改动必须是成套变更。

`docs/constraints/ffi-contract.md` 的 F1 要求：修改 `bridge/ffi/ffi_api*.h`
时，同一次变更必须同时包含

  * 头文件本身（改动集里当然有）；
  * 实现：`bridge/ffi/ffi_*.cc`；
  * 测试：`core/tests/`、`tests/` 或 `flutter_shell/test/`；
  * API 目录：`bridge/ffi/API_CATALOG.md`；
  * 受影响文档：`docs/` 下的条目。

缺一类就说明契约与实现/文档脱节，前端会基于错误假设开发，所以直接失败。

用法：
    python3 tools/check_api_change_set.py                    # 对比 HEAD 与工作区
    python3 tools/check_api_change_set.py --base origin/main  # 对比 ref 与工作区
    python3 tools/check_api_change_set.py --base X --head Y   # 对比两个 ref
    python3 tools/check_api_change_set.py --self-test         # 自检（合成改动集）
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

API_HEADER = re.compile(r"^bridge/ffi/ffi_api[\w_]*\.h$")
IMPLEMENTATION = re.compile(r"^bridge/ffi/ffi_[\w_]*\.cc$")
TESTS = ("core/tests/", "tests/", "flutter_shell/test/")
CATALOG = "bridge/ffi/API_CATALOG.md"
DOCS = "docs/"

CATEGORIES = (
    ("实现 bridge/ffi/ffi_*.cc", lambda p: bool(IMPLEMENTATION.match(p))),
    ("测试 core/tests、tests 或 flutter_shell/test", lambda p: p.startswith(TESTS)),
    ("API 目录 bridge/ffi/API_CATALOG.md", lambda p: p == CATALOG),
    ("受影响文档 docs/", lambda p: p.startswith(DOCS)),
)


def header_had_exports(path: str, base: str) -> bool:
    """该头文件在 `base` 上是否有导出声明（读基准版本，而非工作区）。"""
    result = subprocess.run(
        ["git", "show", f"{base}:{path}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return True  # 读不到就按「有导出」处理，宁可严不放松
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", result.stdout, flags=re.S)
    return "MGCORE_API" in source


def strip_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def api_declarations(text: str) -> list[str]:
    """头文件里承载 ABI 的声明：`MGCORE_API` 函数与 `typedef`。

    注释先剥离、空白归一，这样「只改注释」「只调换 include 顺序」不会算成
    契约变更——F1 要成套，针对的是契约与实现/文档脱节，不是注释。
    """
    src = strip_comments(text)
    raw = re.findall(r"MGCORE_API[^;]*;|typedef[^;]*;", src, flags=re.S)
    # 去掉**全部**空白再比：只是重新折行或调整括号内空格的排版改动不算 ABI 变更。
    return [re.sub(r"\s+", "", item) for item in raw]


def file_text(ref: str | None, path: str) -> str:
    """读 ref 上的文件内容；ref 为 None 时读工作区。读不到返回空串。"""
    if ref is None:
        file = ROOT / path
        if not file.exists():
            return ""
        return file.read_text(encoding="utf-8-sig", errors="replace")
    result = subprocess.run(
        ["git", "show", f"{ref}:{path}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout if result.returncode == 0 else ""


def abi_touched_headers(
    changed: list[str],
    empty_deletions: frozenset[str] = frozenset(),
    base: str = "HEAD",
    head: str | None = None,
) -> list[str]:
    """改动集里真正动了 ABI 的公开头。

    只有「承载导出声明」的头才算 ABI 表面。`ffi_api.h` 是纯 include 聚合
    （0 条 MGCORE_API），增删一个 include 不是 ABI 变更。被删除的头按基准版本判断，
    并以 `empty_deletions` 标出零导出空头。除此之外还要求声明确实变了——只改注释的
    头不再触发成套校验。
    """
    touched: list[str] = []
    for path in changed:
        if not API_HEADER.match(path):
            continue
        if path in empty_deletions:
            touched.append(path)
            continue
        if api_declarations(file_text(base, path)) != api_declarations(
            file_text(head, path)
        ):
            touched.append(path)
    return sorted(touched)


def missing_categories(
    headers: list[str],
    changed: list[str],
    empty_deletions: frozenset[str] = frozenset(),
) -> list[str]:
    """返回缺失的类别。

    一例外：整个改动集里的公开头**都**是「被删除且零导出」时，实现与 API 目录没有
    可改的东西——空头本来就没有实现，目录也不曾列过它。测试与文档仍然要求，
    以证明确实看过影响面。
    """
    if not headers:
        return []

    purely_empty_deletions = all(p in empty_deletions for p in headers)
    required = CATEGORIES
    if purely_empty_deletions:
        required = tuple(
            (label, matches)
            for label, matches in CATEGORIES
            if label not in (
                "实现 bridge/ffi/ffi_*.cc",
                "API 目录 bridge/ffi/API_CATALOG.md",
            )
        )

    return [
        label for label, matches in required if not any(matches(p) for p in changed)
    ]


def changed_files(base: str, head: str | None) -> list[str]:
    cmd = ["git", "diff", "--name-only", base]
    if head:
        cmd.append(head)
    result = subprocess.run(
        cmd, cwd=ROOT, capture_output=True, text=True, check=False
    )
    if result.returncode != 0:
        raise SystemExit(f"[api-change-set] git diff 失败：{result.stderr.strip()}")
    files = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not head:
        # 未跟踪的新文件也算改动（新导出常常先以新文件出现）。
        untracked = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        if untracked.returncode == 0:
            files += [line.strip() for line in untracked.stdout.splitlines() if line.strip()]
    return sorted(set(files))


SELF_TEST_CASES = (
    (
        "只有头文件：必须报缺四类",
        ["bridge/ffi/ffi_api_mission.h"],
        ["bridge/ffi/ffi_api_mission.h"],
        4,
    ),
    (
        "头文件 + 实现 + 测试 + catalog + 文档：完整成套",
        ["bridge/ffi/ffi_api_mission.h"],
        [
            "bridge/ffi/ffi_api_mission.h",
            "bridge/ffi/ffi_mission_commands.cc",
            "core/tests/test_ffi_contract.cc",
            "bridge/ffi/API_CATALOG.md",
            "docs/constraints/ffi-contract.md",
        ],
        0,
    ),
    (
        "头文件 + 实现 + 测试 + catalog，缺文档：报 1 类",
        ["bridge/ffi/ffi_api_payload.h"],
        [
            "bridge/ffi/ffi_api_payload.h",
            "bridge/ffi/ffi_payload_commands.cc",
            "flutter_shell/test/payload_controller_test.dart",
            "bridge/ffi/API_CATALOG.md",
        ],
        1,
    ),
    (
        "没有动公开头文件：不触发",
        [],
        ["bridge/ffi/ffi_internal.h", "core/tests/test_ffi_contract.cc"],
        None,
    ),
    (
        "公开头只改注释（ABI 未变）：不触发",
        [],
        [
            "bridge/ffi/ffi_api_flight.h",
            "bridge/ffi/ffi_config_extra.cc",
            "core/tests/test_ffi_contract.cc",
            "docs/constraints/ffi-contract.md",
        ],
        None,
    ),
    (
        "删除零导出空头 + 测试 + 文档：豁免实现与目录，判定通过",
        ["bridge/ffi/ffi_api_environment.h"],
        [
            "bridge/ffi/ffi_api_environment.h",
            "core/tests/test_bridge_architecture_guards.cc",
            "docs/features/feature-inventory.md",
        ],
        0,
        frozenset({"bridge/ffi/ffi_api_environment.h"}),
    ),
    (
        "删除零导出空头但缺测试：仍报 1 类",
        ["bridge/ffi/ffi_api_environment.h"],
        [
            "bridge/ffi/ffi_api_environment.h",
            "docs/features/feature-inventory.md",
        ],
        1,
        frozenset({"bridge/ffi/ffi_api_environment.h"}),
    ),
)

# `api_declarations` 的语义自检：区分「ABI 真的变了」与「只动了注释/空白/include」。
DECLARATIONS_CASES = (
    (
        "只改注释不算 ABI 变更",
        "MGCORE_API char* mg_a(void);\n// 说明\nMGCORE_API char* mg_b(void);\n",
        "MGCORE_API char* mg_a(void);\n/* 另一段说明 */\nMGCORE_API char* mg_b(void);\n",
        True,
    ),
    (
        "只改 include / 空行不算 ABI 变更",
        '#include "a.h"\n\nMGCORE_API char* mg_a(void);\n',
        '#include "b.h"\nMGCORE_API char* mg_a(void);\n',
        True,
    ),
    (
        "多行声明改参数类型算 ABI 变更",
        "MGCORE_API char* mg_a(mg_core_t* core, int n);\n",
        "MGCORE_API char* mg_a(mg_core_t* core, double n);\n",
        False,
    ),
    (
        "多行声明的换行与空白不算变更",
        "MGCORE_API char* mg_a(mg_core_t* core,\n    int n);\n",
        "MGCORE_API char*   mg_a( mg_core_t* core, int n );\n",
        True,
    ),
    (
        "改 typedef 算 ABI 变更",
        "typedef struct mg_core_t mg_core_t;\nMGCORE_API void mg_a(void);\n",
        "typedef struct mg_core_t* mg_core_t;\nMGCORE_API void mg_a(void);\n",
        False,
    ),
    (
        "增删导出算 ABI 变更",
        "MGCORE_API void mg_a(void);\n",
        "MGCORE_API void mg_a(void);\nMGCORE_API void mg_b(void);\n",
        False,
    ),
)


def self_test() -> int:
    failures: list[str] = []
    for label, headers, changed, expected, *rest in SELF_TEST_CASES:
        empty = rest[0] if rest else frozenset()
        missing = missing_categories(headers, changed, empty)
        if expected is None:
            if missing:
                failures.append(f"自检失败：{label} 不应触发，却报了 {missing}")
            continue
        if len(missing) != expected:
            failures.append(
                f"自检失败：{label} 期望缺 {expected} 类，实际缺 {len(missing)} 类：{missing}"
            )
    for label, before, after, should_be_equal in DECLARATIONS_CASES:
        equal = api_declarations(before) == api_declarations(after)
        if equal != should_be_equal:
            failures.append(
                f"自检失败（声明提取）：{label} 期望"
                f"{'相同' if should_be_equal else '不同'}，实际"
                f"{'相同' if equal else '不同'}"
            )
    if failures:
        for item in failures:
            print(f"[api-change-set] {item}", file=sys.stderr)
        return 1
    print(
        f"[api-change-set] 自检通过（{len(SELF_TEST_CASES)} 个合成改动集 + "
        f"{len(DECLARATIONS_CASES)} 个声明提取用例判定正确）"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="HEAD", help="对比基准 ref（默认 HEAD）")
    parser.add_argument("--head", default=None, help="可选：变更一端 ref（默认工作区）")
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="用合成改动集验证判定逻辑（不访问 git）",
    )
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    changed = changed_files(args.base, args.head)
    empty_deletions = frozenset(
        p
        for p in changed
        if API_HEADER.match(p)
        and ROOT.joinpath(p).exists() is False
        and not header_had_exports(p, args.base)
    )
    headers = abi_touched_headers(changed, empty_deletions, args.base, args.head)
    missing = missing_categories(headers, changed, empty_deletions)
    if empty_deletions:
        print(
            "[api-change-set] 删除零导出公开头，实现与目录无对应改动："
            + ", ".join(sorted(empty_deletions))
        )
    if not headers:
        print(
            "[api-change-set] 本次改动没有触及 bridge/ffi/ffi_api*.h 的 ABI 声明"
            "（纯注释/include 改动不算），无需成套校验"
        )
        return 0
    if missing:
        print(
            f"[api-change-set] 改了公开头文件 {', '.join(headers)}，"
            f"但改动集缺少：{'；'.join(missing)}（ffi-contract.md F1）",
            file=sys.stderr,
        )
        return 1
    print(
        f"[api-change-set] {', '.join(headers)} 的改动是成套的"
        "（实现 + 测试 + API_CATALOG + 文档）"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
