#!/usr/bin/env python3
"""S6 守卫：通用代码里不得出现固件专属参数名字面量。

`docs/constraints/safety.md` 的 S6 要求模式号、参数名、命令编码、校准流程
只能由 ArduPilot 插件提供。参数名的事实来源是
`core/src/config/ParameterDescriptors.json`；前端或桥接层直接写这些名字，
升级参数表后就会出现「页面按旧名字读写、飞控里根本没有该参数」。

扫描范围覆盖三处：
  - flutter_shell/lib（Dart 单引号字符串，含 ${...}/$ident 插值）
  - core/src（C++ 双引号字符串，含 %d 等格式符）
  - bridge/ffi（C++ 双引号字符串，含 %d 等格式符）

字符串插值与格式符会被当作通配符处理：'SERVO${channel}_MIN' 展开为模板
SERVO*_MIN，与描述符里的 SERVO1_MIN、SERVO2_MIN 等匹配。

脚本带一份基线 `tools/firmware_literals_baseline.json`，用于记录「暂缓清理」的
字面量。基线条目格式为 {file: {name: reason}}，每条必须有 reason 字段写明
豁免理由：

* 基线内已登记的字面量（含理由）不阻断；
* 基线外新出现的字面量直接失败，不新增欠账；
* 基线内已消失的条目提示清理，用 `--update-baseline` 收缩基线；
  `--update-baseline` 拒绝收编未写理由的新命中，新增豁免只能人工写进 JSON。

豁免清单只允许缩小，不允许新增。每条豁免必须写明理由，不允许静默放过。

范围边界：本脚本只拦「参数名」字面量。S6 里的模式号 / 命令编码（例如
`0x04040000U`、`42429`）是数值，不在本脚本的检测范围内；这类值的通用代码里
硬编码问题由评审与后续专门检查负责，不要因为本脚本变绿就认为 S6 全绿。

用法：
    python3 tools/check_firmware_literals.py
    python3 tools/check_firmware_literals.py --update-baseline
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tools" / "firmware_literals_baseline.json"

# 扫描范围：前端 Dart + 后端 C++（core/src 与 bridge/ffi）
# 注意：core/src/firmware/ 中的插件实现文件因属于 S6 的合法持有者，已单独豁免。
# 详见 ADR-0011 固件参数名持有机制与循环依赖处理。
SCAN_DIRS: list[tuple[Path, tuple[str, ...]]] = [
    (ROOT / "flutter_shell" / "lib", ("*.dart",)),
    (ROOT / "core" / "src", ("*.cc", "*.h")),
    (ROOT / "bridge" / "ffi", ("*.cc", "*.h")),
]

# 插件实现路径豁免：这些路径中的参数名是 S6 指定的合法持有者
# 格式：(base_dir, subdirs_to_exempt) 或直接 path.match()
PLUGIN_EXEMPT_PATHS = [
    "core/src/firmware/*.cc",
    "core/src/firmware/*.h",
]

DESCRIPTOR_FILES = [
    ROOT / "core" / "src" / "config" / "ParameterDescriptors.json",
]

# 只认「含下划线的整名」：`GPS`、`AUTO`、`RTL` 这类既是参数名又是模式名或
# 枚举名的短词会导致大量误报，不属于本守卫要拦的目标。
NAME_RE = re.compile(r"^[A-Z][A-Z0-9]*_[A-Z0-9_]+$")

# Dart 单引号字符串：匹配 '...' 内的内容，支持 ${...} 和 $ident 插值。
# 匹配组 1 是引号内的原始内容（含插值语法）。
DART_STRING_RE = re.compile(r"'([^']*)'")

# C++ 双引号字符串：匹配 "..." 内的内容，支持转义序列。
# 匹配组 1 是引号内的原始内容。
CPP_STRING_RE = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')

# Dart 插值语法：${...} 或 $identifier
DART_INTERP_RE = re.compile(r"\$\{[^}]*\}|\$[a-zA-Z_][a-zA-Z0-9_]*")

# C 格式符：%d, %u, %i, %ld, %lu, {n}, {0} 等
C_FORMAT_RE = re.compile(r"%[lL]?[diouxX]|%\d*[diouxX]|\{[a-zA-Z0-9_]*\}")

# 模板中非通配符段必须符合的形状：大写字母+数字+下划线
TEMPLATE_SHAPE_RE = re.compile(r"^[A-Z][A-Z0-9*]*_[A-Z0-9*]+$")


def template_to_regex(template: str) -> re.Pattern:
    """把模板（含 * 通配符）转为正则：* → [0-9]*"""
    escaped = re.escape(template)
    # re.escape 把 * 转义为 \*，这里还原为 [0-9]*
    escaped = escaped.replace(r"\*", r"[0-9]*")
    return re.compile(escaped)


def descriptor_names() -> set[str]:
    names: set[str] = set()

    def walk(node: object) -> None:
        if isinstance(node, dict):
            for key, value in node.items():
                if isinstance(key, str) and NAME_RE.match(key):
                    names.add(key)
                walk(value)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    for path in DESCRIPTOR_FILES:
        if not path.exists():
            raise SystemExit(f"[firmware-literals] 缺少参数描述符：{path}")
        walk(json.loads(path.read_text()))
    return names


def extract_templates(content: str) -> set[str]:
    """从字符串内容中提取可能的固件参数名模板。

    策略：找到所有引号字符串，去掉插值/格式符后得到模板，
    如果模板符合 NAME_RE 的形状（含下划线、全大写+数字+通配符），就保留。
    """
    templates: set[str] = set()
    # 同时搜索 Dart 和 C++ 风格的字符串
    for m in DART_STRING_RE.finditer(content):
        raw = m.group(1)
        # 去掉 Dart 插值
        stripped = DART_INTERP_RE.sub("*", raw)
        # 去掉 C 格式符（万一混用）
        stripped = C_FORMAT_RE.sub("*", stripped)
        if TEMPLATE_SHAPE_RE.match(stripped):
            templates.add(stripped)
    for m in CPP_STRING_RE.finditer(content):
        raw = m.group(1)
        # 去掉 C 格式符
        stripped = C_FORMAT_RE.sub("*", raw)
        # 去掉 Dart 插值（万一混用）
        stripped = DART_INTERP_RE.sub("*", stripped)
        if TEMPLATE_SHAPE_RE.match(stripped):
            templates.add(stripped)
    return templates


def match_templates(templates: set[str], universe: set[str]) -> set[str]:
    """把模板与描述符名集匹配，返回命中的模板。"""
    hits: set[str] = set()
    for template in templates:
        # 纯字面量（无通配符）：直接查集合
        if "*" not in template:
            if template in universe:
                hits.add(template)
        else:
            # 带通配符：检查是否有任何描述符名匹配此模板
            pat = template_to_regex(template)
            for name in universe:
                if pat.fullmatch(name):
                    hits.add(template)
                    break
    return hits


def scan() -> dict[str, list[str]]:
    universe = descriptor_names()
    found: dict[str, list[str]] = {}
    for scan_dir, patterns in SCAN_DIRS:
        if not scan_dir.exists():
            continue
        for pattern in patterns:
            for path in sorted(scan_dir.rglob(pattern)):
                # 检查是否在豁免路径中（插件实现）
                rel = str(path.relative_to(ROOT))
                is_exempt = any(Path(rel).match(exempt) for exempt in PLUGIN_EXEMPT_PATHS)
                if is_exempt:
                    continue

                text = path.read_text(encoding="utf-8", errors="ignore")
                templates = extract_templates(text)
                hits = match_templates(templates, universe)
                if hits:
                    found[rel] = sorted(hits)
    return found


def load_baseline() -> dict[str, dict[str, str]]:
    """加载基线，格式 {file: {name: reason}}。"""
    if not BASELINE.exists():
        return {}
    data = json.loads(BASELINE.read_text())
    # 兼容旧格式 {file: [names]}：旧条目无 reason，标记为 "(no reason)"
    result: dict[str, dict[str, str]] = {}
    for file, val in data.items():
        if isinstance(val, list):
            result[file] = {name: "(旧格式，无理由)" for name in val}
        elif isinstance(val, dict):
            result[file] = dict(val)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="把当前扫描结果写回基线（只应在基线收缩后使用）",
    )
    args = parser.parse_args()

    current = scan()
    total = sum(len(v) for v in current.values())

    if args.update_baseline:
        # --update-baseline 只做收缩：保留现有理由，移除已消失的条目。
        # 未登记过的新命中不在本命令里自动收编 —— 新增豁免必须人工写进 JSON 并附理由，
        # 否则「只允许缩小」会被一条命令绕过去。
        existing = load_baseline()
        new_data: dict[str, dict[str, str]] = {}
        new_entries: list[str] = []
        for file, names in current.items():
            new_data[file] = {}
            for name in names:
                reason = existing.get(file, {}).get(name)
                if reason is None:
                    new_entries.append(f"{file}: {name}")
                    continue
                new_data[file][name] = reason
        if new_entries:
            baseline_label = (
                BASELINE.relative_to(ROOT) if BASELINE.is_relative_to(ROOT) else BASELINE
            )
            print(
                "[firmware-literals] 以下命中尚未登记豁免理由，请手工写入 "
                f"{baseline_label}（格式为 文件 -> 参数名 -> 理由）"
                "后再收缩基线：\n  " + "\n  ".join(sorted(new_entries)),
                file=sys.stderr,
            )
            return 1
        BASELINE.write_text(
            json.dumps(new_data, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
        )
        print(f"[firmware-literals] 基线已收缩：{len(new_data)} 个文件 / {total} 处")
        return 0

    baseline = load_baseline()
    # 展开基线为 {file: [names]} 用于差集比较
    baseline_names: dict[str, list[str]] = {
        file: sorted(reasons.keys()) for file, reasons in baseline.items()
    }

    added = {
        file: sorted(set(names) - set(baseline_names.get(file, [])))
        for file, names in current.items()
        if set(names) - set(baseline_names.get(file, []))
    }
    removed = {
        file: sorted(set(baseline_names.get(file, [])) - set(current.get(file, [])))
        for file in baseline_names
        if set(baseline_names.get(file, [])) - set(current.get(file, []))
    }

    for file, names in sorted(added.items()):
        for name in names:
            print(f"[firmware-literals] 新增固件参数字面量：{file}: {name}", file=sys.stderr)
    for file, names in sorted(removed.items()):
        for name in names:
            print(f"[firmware-literals] 基线中已消失（可收缩）：{file}: {name}", file=sys.stderr)

    # 检查基线条目是否都有理由
    missing_reasons: list[str] = []
    for file, reasons in baseline.items():
        for name, reason in reasons.items():
            if not reason or reason == "(旧格式，无理由)" or reason == "(new, no reason)":
                missing_reasons.append(f"{file}: {name}")
    if missing_reasons:
        print(
            "[firmware-literals] 基线条目缺少豁免理由：\n  "
            + "\n  ".join(missing_reasons),
            file=sys.stderr,
        )
        return 1

    if added:
        print(
            "[firmware-literals] S6：参数名必须来自核心插件或参数描述符，"
            "不得在通用代码中硬编码。",
            file=sys.stderr,
        )
        return 1

    if total == 0:
        print("[firmware-literals] 通过：通用代码中没有固件专属参数字面量。")
    else:
        print(
            f"[firmware-literals] 未新增固件参数字面量；"
            f"基线内暂缓项 {len(current)} 个文件 / {total} 处"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
