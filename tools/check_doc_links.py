#!/usr/bin/env python3
"""文档一致性守卫：路径引用、ADR 索引、日期合理性。

`docs/` 是唯一没有门禁保护的目录——代码有 `preflight.sh`，文档此前只检查
「补丁格式」与「版本一致性」。结果是文档成为可以无声腐烂的地方：
指向已删除文件的引用、停更的 ADR 索引、早于仓库起始时间的日期。

本脚本检查三件事：

1. **路径存在性**——反引号里以仓内顶层目录开头的文件引用必须存在。
2. **ADR 索引同步**——`docs/adr/README.md` 必须列出目录下所有 ADR。
3. **日期合理性**——文档头部声明的日期不得早于仓库首次提交。

## 历史叙述的处理

文档记录历史时必然要提到**已删除**的文件（「删除 `core/src/geofence/geofence_manager.cc`」）。
若一律拦下，就会逼迫作者删掉有价值的历史记录——那是比腐烂更糟的结果。

因此失效引用必须**显式声明为历史**才放行。三个途径：

* 所在行含历史标记词（「已删除」「已移除」「已于」「删除前」「改造前」等）；
* 所在行或上文 5 行内含交割引用（`` `abc1234` `` 形式的 commit SHA）；
* 文件头部 frontmatter 声明 `` 历史快照 ``（整个文件按历史叙述处理）。

仍不满足的一律阻断——**要求作者说清楚「这个文件已经没了」比允许它静默悬空要好**。

用法：
    python3 tools/check_doc_links.py
    python3 tools/check_doc_links.py --verbose
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ADR_DIR = ROOT / "docs" / "adr"
ADR_INDEX = ADR_DIR / "README.md"

# 仓内顶层目录：只有以这些开头的引用才按「当前状态」要求存在。
# 其他路径（~/Projects/qgroundcontrol 等上游参考、build/ 产物）不在范围内。
IN_REPO_PREFIXES = (
    "core/",
    "bridge/",
    "flutter_shell/",
    "tools/",
    "tests/",
    "docs/",
    "third_party/",
)

# 反引号包裹的、带仓内顶层目录前缀的文件引用（可带 :行号 或 :起-止）
REF_RE = re.compile(
    r"`(" + "|".join(re.escape(p) for p in IN_REPO_PREFIXES) + r")"
    r"[A-Za-z0-9_./\-]*"
    r"\.(?:md|cc|h|hpp|dart|py|sh|json|yaml|yml|txt|xml|CMakeLists)"
    r"(?::\d+(?:-\d+)?)?`"
)

# 历史标记词：出现即认为该行在描述过去的状态，失效引用可放行。
HISTORY_MARKERS = (
    "已删除",
    "已移除",
    "已摘除",
    "删除了",
    "删除前",
    "删除后",
    "改造前",
    "改造后",
    "已于",
    "已随",
    "已在本轮",
    "曾经",
    "原为",
    "原本",
    "不再存在",
    "已不存在",
    "已废弃",
    "已下线",
    "已迁出",
    "迁移前",
    "旧路径",
    "S1 已删除",
)

# 交割引用：`27b5dfb` 形式的短 SHA，说明历史时通常会带上。
COMMIT_REF_RE = re.compile(r"`[0-9a-f]{7,40}`")

# 扫描的 Markdown 范围
def markdown_files() -> list[Path]:
    files = sorted(ROOT.glob("*.md"))
    files += sorted(ROOT.glob("docs/**/*.md"))
    out = []
    for f in files:
        # build* 目录下的生成物、以及 tools 的临时目录不在范围内
        parts = set(f.relative_to(ROOT).parts)
        if any(p.startswith("build") or p.startswith(".") for p in parts):
            continue
        out.append(f)
    return out


def file_is_history_snapshot(text: str) -> bool:
    """文件头部是否声明为历史快照。"""
    head = "\n".join(text.splitlines()[:25])
    return "历史快照" in head


def has_history_context(lines: list[str], idx: int) -> bool:
    """该行本身或上文 5 行内是否出现历史标记 / 交割引用。"""
    line = lines[idx]
    if any(m in line for m in HISTORY_MARKERS):
        return True
    if COMMIT_REF_RE.search(line):
        return True
    for j in range(max(0, idx - 5), idx):
        if any(m in lines[j] for m in HISTORY_MARKERS):
            return True
    return False


def strip_line_suffix(ref: str) -> tuple[str, str]:
    """把 `a/b.cc:12-18` 拆成 ('a/b.cc', '12-18')。"""
    if ":" in ref:
        path, _, lines = ref.rpartition(":")
        return path, lines
    return ref, ""


def check_paths(verbose: bool) -> tuple[list[str], int]:
    """返回 (问题列表, 已放行的历史引用数)。"""
    problems: list[str] = []
    excused = 0
    for f in markdown_files():
        text = f.read_text(encoding="utf-8", errors="ignore")
        rel = f.relative_to(ROOT).as_posix()
        snapshot = file_is_history_snapshot(text)
        lines = text.splitlines()
        for idx, line in enumerate(lines):
            for m in REF_RE.finditer(line):
                raw = m.group(0)[1:-1]
                path, _lines = strip_line_suffix(raw)
                if (ROOT / path).exists():
                    continue
                if snapshot or has_history_context(lines, idx):
                    excused += 1
                    if verbose:
                        print(f"  [历史] {rel}:{idx + 1} -> {path}")
                    continue
                problems.append(
                    f"{rel}:{idx + 1} 引用了不存在的文件 `{raw}`\n"
                    f"      若这是历史叙述，请加上「已删除」等说明或 commit 引用；"
                    f"否则修正路径。"
                )
    return problems, excused


def check_adr_index(verbose: bool) -> list[str]:
    """docs/adr/README.md 必须列出目录下所有 ADR。"""
    if not ADR_INDEX.exists():
        return [f"{ADR_INDEX.relative_to(ROOT).as_posix()} 不存在"]
    index_text = ADR_INDEX.read_text(encoding="utf-8")
    on_disk = sorted(
        p.name for p in ADR_DIR.glob("*.md") if p.name != "README.md"
    )
    missing = [name for name in on_disk if name not in index_text]
    if not missing:
        if verbose:
            print(f"  [index] {len(on_disk)} 个 ADR 均已登记")
        return []
    return [
        "docs/adr/README.md 未登记以下 ADR（新增 ADR 必须同步索引）：\n"
        + "\n".join(f"      - {n}" for n in missing)
    ]


def repo_first_commit_date() -> date | None:
    try:
        out = subprocess.run(
            ["git", "log", "--reverse", "--format=%ad", "--date=short"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip().splitlines()
        if out:
            y, m, d = out[0].split("-")
            return date(int(y), int(m), int(d))
    except (subprocess.CalledProcessError, FileNotFoundError, ValueError):
        pass
    return None


DATE_RE = re.compile(
    r"(?:最后更新|报告日期|报告时点|日期|更新日期)\**\s*[:：]\s*(\d{4}-\d{2}-\d{2})"
)


def check_dates(verbose: bool) -> list[str]:
    """文档头部日期不得早于仓库首次提交。"""
    first = repo_first_commit_date()
    if first is None:
        return []
    problems: list[str] = []
    for f in markdown_files():
        rel = f.relative_to(ROOT).as_posix()
        head = "\n".join(f.read_text(encoding="utf-8", errors="ignore").splitlines()[:25])
        for m in DATE_RE.finditer(head):
            try:
                y, mo, d = (int(x) for x in m.group(1).split("-"))
                declared = date(y, mo, d)
            except ValueError:
                continue
            if declared < first:
                problems.append(
                    f"{rel} 头部日期 {m.group(1)} 早于仓库首次提交 "
                    f"{first.isoformat()}（疑似笔误）"
                )
    if verbose and not problems:
        print(f"  [dates] 头部日期均不早于 {first.isoformat()}")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description="文档一致性守卫")
    ap.add_argument("--verbose", action="store_true", help="打印放行与通过明细")
    args = ap.parse_args()

    problems: list[str] = []
    path_problems, excused = check_paths(args.verbose)
    problems += path_problems
    problems += check_adr_index(args.verbose)
    problems += check_dates(args.verbose)

    if problems:
        for p in problems:
            print(f"[doc-links] {p}", file=sys.stderr)
        print(
            "[doc-links] 文档引用了不存在的文件、ADR 索引未同步，或日期早于仓库时间线。",
            file=sys.stderr,
        )
        return 1

    print(
        f"[doc-links] 通过：路径引用、ADR 索引、日期均已核对"
        f"（历史引用放行 {excused} 处）"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
