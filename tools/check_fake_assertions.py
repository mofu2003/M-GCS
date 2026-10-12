#!/usr/bin/env python3
"""T2 守卫：静态扫描假断言写法。

`docs/constraints/testing.md` 的 T2 禁止四类写法，它们会让测试"通过但不验证行为"：

  1. 恒真条件：`CHECK(true, ...)`、`CHECK(1, ...)`、`CHECK(!false, ...)`；
  2. OR 组合恒真：`A || !A`、`x || true`、`x || 1`；
  3. 只检查非空：一个测试用例里**唯一**的断言就是空/非空/非 nullptr 检查
     （`CHECK(r != nullptr, "…")`、`CHECK(!json.empty(), "…")`）；
  4. 无条件 SKIP：`if (false)`、`#if 0`、`if (0)` 包住的断言或整个用例。

扫描对象：C++ 原生测试（core/tests、tests）与 Dart 测试（flutter_shell/test）。
判定按"断言"为单位：CHECK/CHECK_NEAR/TEST/expect(...)/assert(...)。

用法：
    python3 tools/check_fake_assertions.py            # 扫描仓库
    python3 tools/check_fake_assertions.py --self-test # 自检（构造违规样本，
                                                      # 必须被报出，否则脚本本身失效）
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SCAN_DIRS = [
    (ROOT / "core" / "tests", ("*.cc", "*.h")),
    (ROOT / "tests", ("*.cc", "*.h")),
    (ROOT / "flutter_shell" / "test", ("*.dart",)),
]

# 断言宏/函数：C++ 侧是 CHECK/CHECK_NEAR/TEST，Dart 侧是 expect(/assert(。
CPP_ASSERTION = re.compile(r"\b(CHECK_NEAR|CHECK|TEST_NEAR|TEST)\s*\(")
DART_ASSERTION = re.compile(r"\b(expect|assert)\s*\(")

# 恒真字面量。
TRUTHY_LITERALS = {"true", "1", "!false", "!0", "!= 0", "== 1"}

# 只检查"非空/非零/非 nullptr"的条件。
EMPTINESS_ONLY = re.compile(
    r"^\s*!?\s*[A-Za-z_][\w\.\(\)\[\]\->\*]*\s*"      # 变量/调用
    r"(!=\s*(nullptr|NULL|0)|==\s*(nullptr|NULL))?\s*$"
)
EMPTINESS_CALLS = re.compile(
    r"^\s*!?\s*[\w\.\(\)\[\]\->\*]+\.(empty|isEmpty|isNotEmpty)\s*\(\s*\)\s*$"
)

# 无条件 SKIP / 被 #if 0 包住的代码。
DISABLED_IF = re.compile(r"\bif\s*\(\s*(false|0)\s*\)")
DISABLED_PREPROC = re.compile(r"^\s*#if\s+0\b", re.M)
SKIP_MARKERS = re.compile(r"\b(SKIP|Skip|skip)\s*\(", re.M)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig", errors="ignore")
    except OSError:
        return ""


def strip_comments_and_strings(text: str) -> str:
    """去掉注释与字符串字面量，避免把说明文字当成代码。

    保留换行，行号不变。字符串统一替换为 "S"，因为断言消息里出现
    `true`/`nullptr` 之类的字眼很常见，但它们不是条件。
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 2
            continue
        if ch in "\"'":
            quote = ch
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == "\n":
                    out.append("\n")
                i += 1
            i += 1
            out.append('"S"')
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def split_args(text: str, open_index: int) -> list[str]:
    """切分 `text[open_index]` 处 `(` 的实参（顶层逗号）。"""
    args: list[str] = []
    depth = 0
    cur = ""
    i = open_index
    while i < len(text):
        ch = text[i]
        if ch in "([{":
            depth += 1
            if depth == 1:
                i += 1
                continue
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                args.append(cur.strip())
                return args
        if ch == "," and depth == 1:
            args.append(cur.strip())
            cur = ""
            i += 1
            continue
        cur += ch
        i += 1
    args.append(cur.strip())
    return args


def is_truthy(cond: str) -> bool:
    return cond.strip() in TRUTHY_LITERALS


def is_or_tautology(cond: str) -> bool:
    if "||" not in cond:
        return False
    parts = [p.strip() for p in cond.split("||")]
    if any(p in ("true", "1", "false == false") for p in parts):
        return True
    # A || !A
    for left in parts:
        if left.startswith("!"):
            if left[1:].strip() in parts:
                return True
        else:
            if ("!" + left) in parts:
                return True
    return False


def is_emptiness_only(cond: str) -> bool:
    """只认"无歧义的空/非空检查"。

    刻意不把裸标识符（`CHECK(!received, ...)`）算进来：那是行为断言（"回调没被
    调用过"），不是"只检查非空"。误报会让守卫失去可信度。
    """
    text = cond.strip()
    if not text:
        return False
    if EMPTINESS_CALLS.match(text):
        return True
    if re.fullmatch(r"!?\s*[A-Za-z_][\w\.\(\)\[\]\->\*]*", text) and (
        text.endswith("empty()") or text.endswith("isEmpty()")
    ):
        return True
    if re.fullmatch(r"[A-Za-z_][\w\.\(\)\[\]\->\*]*\s*!=\s*(nullptr|NULL)", text):
        return True
    if re.fullmatch(r"[A-Za-z_][\w\.\(\)\[\]\->\*]*\s*==\s*(nullptr|NULL)", text):
        return True
    return False


def scan_cpp(path: Path, raw: str) -> list[str]:
    code = strip_comments_and_strings(raw)
    findings: list[str] = []

    for match in CPP_ASSERTION.finditer(code):
        macro = match.group(1)
        open_index = match.end() - 1
        args = split_args(code, open_index)
        if not args:
            continue
        line = code[: match.start()].count("\n") + 1
        if macro in ("CHECK", "TEST"):
            if len(args) < 2:
                findings.append(f"{path}:{line}: {macro} 只有条件没有消息（无法判断断言意图）")
                continue
            cond = args[0]
        else:  # CHECK_NEAR(actual, expected, eps, msg)
            if len(args) < 4:
                findings.append(f"{path}:{line}: {macro} 参数不足")
                continue
            cond = f"abs({args[0]} - {args[1]}) < {args[2]}"
        if is_truthy(cond):
            findings.append(f"{path}:{line}: 恒真断言 {macro}({cond}, ...)（T2）")
        if is_or_tautology(cond):
            findings.append(f"{path}:{line}: OR 组合恒真 {macro}({cond}, ...)（T2）")

    return findings


def scan_disabled_code(path: Path, raw: str) -> list[str]:
    findings: list[str] = []
    for match in DISABLED_IF.finditer(raw):
        line = raw[: match.start()].count("\n") + 1
        findings.append(f"{path}:{line}: if (false/0) 包住代码即无条件跳过（T2）")
    for match in DISABLED_PREPROC.finditer(raw):
        line = raw[: match.start()].count("\n") + 1
        findings.append(f"{path}:{line}: #if 0 包住代码即无条件跳过（T2）")
    return findings


def scan_emptiness_only_tests(path: Path, raw: str) -> list[str]:
    """一个测试函数里只有"非空"断言 = 不验证行为。

    实现方式很保守：把每个 `static void test_*()` / `test('…', () {…})` 函数体
    切出来，统计其中的断言；只有当断言数为 1 且该断言是纯非空检查时才报告。
    断言 ≥2 的用例（哪怕其中一条只是非空）不报告，因为那通常是在为后续断言
    做前置检查。
    """
    findings: list[str] = []
    code = strip_comments_and_strings(raw)

    # C++：void test_xxx() { ... } 或 TEST(name, cond) 由宏判定
    for match in re.finditer(r"\bstatic\s+void\s+(test_\w+)\s*\([^)]*\)\s*\{", code):
        name = match.group(1)
        body_start = match.end()
        depth = 1
        i = body_start
        while i < len(code) and depth:
            if code[i] == "{":
                depth += 1
            elif code[i] == "}":
                depth -= 1
            i += 1
        body = code[body_start : i - 1]
        conds: list[str] = []
        for assertion in CPP_ASSERTION.finditer(body):
            args = split_args(body, assertion.end() - 1)
            if not args:
                continue
            if assertion.group(1) in ("CHECK", "TEST"):
                if len(args) >= 2:
                    conds.append(args[0])
            elif len(args) >= 3:
                conds.append("near")
        if len(conds) == 1 and is_emptiness_only(conds[0]):
            line = code[: match.start()].count("\n") + 1
            findings.append(
                f"{path}:{line}: 用例 {name} 只有一条非空断言（{conds[0]}），未验证行为（T2）"
            )

    # Dart：test('…', () { ... }) 里的唯一 expect 是 isNotNull 之类
    for match in re.finditer(r"\btest\w*\(\s*(['\"])(.*?)\1\s*,\s*\(\)\s*(?:async\s*)?\{", code):
        body_start = match.end()
        depth = 1
        i = body_start
        while i < len(code) and depth:
            if code[i] == "{":
                depth += 1
            elif code[i] == "}":
                depth -= 1
            i += 1
        body = code[body_start : i - 1]
        expectations = list(DART_ASSERTION.finditer(body))
        if len(expectations) != 1:
            continue
        args = split_args(body, expectations[0].end() - 1)
        if len(args) < 2:
            continue
        matcher = args[1].strip()
        if matcher in ("isNotNull", "isNotEmpty"):
            line = code[: match.start()].count("\n") + 1
            findings.append(
                f"{path}:{line}: 用例「{match.group(2)}」只有一条 {matcher} 断言，未验证行为（T2）"
            )
    return findings


def scan(path: Path) -> list[str]:
    raw = read(path)
    if not raw:
        return []
    findings = scan_cpp(path, raw)
    findings += scan_disabled_code(path, raw)
    findings += scan_emptiness_only_tests(path, raw)
    return findings


def repo_findings() -> list[str]:
    findings: list[str] = []
    for base, patterns in SCAN_DIRS:
        if not base.exists():
            continue
        for pattern in patterns:
            for path in sorted(base.rglob(pattern)):
                if path.name.startswith("."):
                    continue
                findings += scan(path)
    return findings


SELF_TEST_SAMPLES = {
    "恒真断言": "static void test_x() {\n  CHECK(true, \"always\");\n}\n",
    "OR 组合恒真": "static void test_x() {\n  CHECK(a || !a, \"tautology\");\n}\n",
    "OR 与 true：": "static void test_x() {\n  CHECK(ready || true, \"tautology\");\n}\n",
    "只查非空": "static void test_x() {\n  CHECK(result != nullptr, \"non-null\");\n}\n",
    "无条件跳过": "static void test_x() {\n  if (false) { CHECK(a, \"skipped\"); }\n}\n",
}

SELF_TEST_CLEAN = {
    "正常断言": "static void test_x() {\n  CHECK(value == 7, \"value\");\n}\n",
    "两条断言": "static void test_x() {\n  CHECK(r != nullptr, \"non-null\");\n  CHECK(r->count == 3, \"count\");\n}\n",
}


def self_test() -> int:
    failures: list[str] = []
    scratch = ROOT / "core" / "tests" / "_t2_self_test.cc"
    for label, sample in SELF_TEST_SAMPLES.items():
        scratch.write_text(sample, encoding="utf-8")
        if not scan(scratch):
            failures.append(f"自检失败：{label} 应被判违规，但脚本没有报出来")
    for label, sample in SELF_TEST_CLEAN.items():
        scratch.write_text(sample, encoding="utf-8")
        found = scan(scratch)
        if found:
            failures.append(f"自检失败：{label} 是合法写法，却被误报：{found}")
    scratch.unlink(missing_ok=True)

    if failures:
        for item in failures:
            print(f"[fake-assertions] {item}", file=sys.stderr)
        return 1
    print(
        f"[fake-assertions] 自检通过（{len(SELF_TEST_SAMPLES)} 类违规全部报出，"
        f"{len(SELF_TEST_CLEAN)} 类合法写法无误报）"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="用合成样本验证脚本本身能识别违规（不扫描仓库）",
    )
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    findings = repo_findings()
    if findings:
        for item in findings:
            print(f"[fake-assertions] {item}", file=sys.stderr)
        print(
            f"[fake-assertions] {len(findings)} 处假断言写法（testing.md T2）",
            file=sys.stderr,
        )
        return 1
    print("[fake-assertions] 未发现 T2 禁止的假断言写法")
    return 0


if __name__ == "__main__":
    sys.exit(main())
