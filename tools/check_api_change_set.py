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


def has_exports(path: str) -> bool:
    """工作区版本是否有导出声明。"""
    file = ROOT / path
    if not file.exists():
        return False
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", file.read_text(encoding="utf-8-sig"), flags=re.S)
    return "MGCORE_API" in source


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


def missing_categories(
    changed: list[str], empty_deletions: frozenset[str] = frozenset(),
    base: str = "HEAD",
) -> tuple[list[str], list[str]]:
    """返回 (缺失的类别, 触发的 API 头文件)。

    `empty_deletions` 是调用方判定的「被删除且在基准里零导出」的头文件集合
    （自检时显式传入，保持判定逻辑不依赖 git 状态）。

    一例外：整个改动集里的公开头**都**属于这种情况时，实现与 API 目录没有可改的
    东西——空头本来就没有实现，目录也不曾列过它。测试与文档仍然要求，以证明
    确实看过影响面。
    """
    # 只有「承载导出声明」的头才算 ABI 表面。`ffi_api.h` 是纯 include 聚合
    # （0 条 MGCORE_API），注释或增删一个 include 不是 ABI 变更，不该要求
    # 实现/目录/测试成套；被删除的头按基准版本判断。
    headers = sorted(
        p
        for p in changed
        if API_HEADER.match(p)
        and (
            has_exports(p)
            or (ROOT / p).exists() is False and p in empty_deletions
            or not (ROOT / p).exists() and header_had_exports(p, base)
        )
    )
    if not headers:
        return [], []

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

    missing = [
        label for label, matches in required if not any(matches(p) for p in changed)
    ]
    return missing, headers


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
        4,
    ),
    (
        "头文件 + 实现 + 测试 + catalog + 文档：完整成套",
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
        ["bridge/ffi/ffi_internal.h", "core/tests/test_ffi_contract.cc"],
        None,
    ),
    (
        "删除零导出空头 + 测试 + 文档：豁免实现与目录，判定通过",
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
        [
            "bridge/ffi/ffi_api_environment.h",
            "docs/features/feature-inventory.md",
        ],
        1,
        frozenset({"bridge/ffi/ffi_api_environment.h"}),
    ),
)


def self_test() -> int:
    failures: list[str] = []
    for label, changed, expected, *rest in SELF_TEST_CASES:
        empty = rest[0] if rest else frozenset()
        missing, headers = missing_categories(changed, empty, base="HEAD")
        if expected is None:
            if headers:
                failures.append(f"自检失败：{label} 不应触发，却报了 {headers}")
            continue
        if len(missing) != expected:
            failures.append(
                f"自检失败：{label} 期望缺 {expected} 类，实际缺 {len(missing)} 类：{missing}"
            )
    if failures:
        for item in failures:
            print(f"[api-change-set] {item}", file=sys.stderr)
        return 1
    print(f"[api-change-set] 自检通过（{len(SELF_TEST_CASES)} 个合成改动集判定正确）")
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
        if API_HEADER.match(p) and not (ROOT / p).exists() and not header_had_exports(p, args.base)
    )
    missing, headers = missing_categories(changed, empty_deletions)
    if empty_deletions:
        print(
            "[api-change-set] 删除零导出公开头，实现与目录无对应改动："
            + ", ".join(sorted(empty_deletions))
        )
    if not headers:
        print("[api-change-set] 本次改动没有触及 bridge/ffi/ffi_api*.h，无需成套校验")
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
