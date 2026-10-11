#!/usr/bin/env python3
"""S6 守卫：通用前端代码里不得出现固件专属参数名字面量。

`docs/constraints/safety.md` 的 S6 要求模式号、参数名、命令编码、校准流程
只能由 ArduPilot / PX4 插件提供。参数名的事实来源是
`core/src/config/ParameterDescriptors.json`（ArduPilot）与
`core/src/config/PX4ParameterMetaData.json`（PX4）；前端直接写这些名字，
换固件或升级参数表后就会出现「页面按旧名字读写、飞控里根本没有该参数」。

历史遗留（KD-42）尚未清零，因此脚本带一份基线
`tools/firmware_literals_baseline.json`：

* 基线内已登记的字面量不阻断；
* 基线外新出现的字面量直接失败，不新增欠账；
* 基线内已消失的条目提示清理，用 `--update-baseline` 收缩基线。

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
SCAN_ROOT = ROOT / "flutter_shell" / "lib"
DESCRIPTOR_FILES = [
    ROOT / "core" / "src" / "config" / "ParameterDescriptors.json",
    ROOT / "core" / "src" / "config" / "PX4ParameterMetaData.json",
]

# 只认「含下划线的整名」：`GPS`、`AUTO`、`RTL` 这类既是参数名又是模式名或
# 枚举名的短词会导致大量误报，不属于本守卫要拦的目标。
NAME_RE = re.compile(r"^[A-Z][A-Z0-9]*_[A-Z0-9_]+$")
LITERAL_RE = re.compile(r"'([A-Z][A-Z0-9]*_[A-Z0-9_]+)'")


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


def scan() -> dict[str, list[str]]:
    universe = descriptor_names()
    found: dict[str, list[str]] = {}
    for path in sorted(SCAN_ROOT.rglob("*.dart")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        hits = {m.group(1) for m in LITERAL_RE.finditer(text)} & universe
        if hits:
            found[str(path.relative_to(ROOT))] = sorted(hits)
    return found


def load_baseline() -> dict[str, list[str]]:
    if not BASELINE.exists():
        return {}
    data = json.loads(BASELINE.read_text())
    return {k: sorted(v) for k, v in data.items()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="把当前扫描结果写回基线（只应在 KD-42 收缩后使用）",
    )
    args = parser.parse_args()

    current = scan()
    total = sum(len(v) for v in current.values())

    if args.update_baseline:
        BASELINE.write_text(
            json.dumps(current, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
        )
        print(f"[firmware-literals] 基线已更新：{len(current)} 个文件 / {total} 处")
        return 0

    baseline = load_baseline()
    added = {
        file: sorted(set(names) - set(baseline.get(file, [])))
        for file, names in current.items()
        if set(names) - set(baseline.get(file, []))
    }
    removed = {
        file: sorted(set(baseline.get(file, [])) - set(current.get(file, [])))
        for file in baseline
        if set(baseline.get(file, [])) - set(current.get(file, []))
    }

    for file, names in sorted(added.items()):
        for name in names:
            print(f"[firmware-literals] 新增固件参数字面量：{file}: {name}", file=sys.stderr)
    for file, names in sorted(removed.items()):
        for name in names:
            print(f"[firmware-literals] 基线中已消失（可收缩）：{file}: {name}", file=sys.stderr)

    if added:
        print(
            "[firmware-literals] S6：参数名必须来自核心插件或参数描述符，"
            "不得在前端硬编码。",
            file=sys.stderr,
        )
        return 1

    print(
        f"[firmware-literals] 未新增固件参数字面量；"
        f"KD-42 欠账 {len(current)} 个文件 / {total} 处"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
