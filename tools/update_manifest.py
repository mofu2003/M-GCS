#!/usr/bin/env python3
"""Generate the HTTPS update manifest consumed by M-GCS."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote, urljoin, urlparse


def _version(value: str) -> str:
    if not re.fullmatch(r"\d+\.\d+\.\d+", value):
        raise argparse.ArgumentTypeError("版本号必须形如 1.3.0")
    return value


def _build(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("构建号必须是整数") from error
    if parsed < 0:
        raise argparse.ArgumentTypeError("构建号不能为负数")
    return parsed


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _asset_arguments(values: list[list[str]]) -> list[dict[str, object]]:
    assets: list[dict[str, object]] = []
    seen: set[tuple[str, str | None]] = set()
    for values_for_asset in values:
        if len(values_for_asset) not in (3, 4):
            raise ValueError(
                "每个 --asset 需要 PLATFORM ARCH PATH [URL] 四项参数"
            )
        platform, architecture, raw_path = values_for_asset[:3]
        if not re.fullmatch(r"[a-z0-9_-]+", platform):
            raise ValueError(f"平台名无效：{platform}")
        normalized_architecture = (
            None if architecture in ("", "-", "any") else architecture.lower()
        )
        if normalized_architecture is not None and not re.fullmatch(
            r"[a-z0-9_-]+", normalized_architecture
        ):
            raise ValueError(f"架构名无效：{architecture}")
        key = (platform, normalized_architecture)
        if key in seen:
            raise ValueError(f"重复的平台与架构：{platform}/{architecture}")
        seen.add(key)

        path = Path(raw_path).expanduser().resolve()
        if not path.is_file():
            raise ValueError(f"更新包不存在：{path}")
        url = values_for_asset[3] if len(values_for_asset) == 4 else None
        assets.append(
            {
                "platform": platform,
                "architecture": normalized_architecture,
                "path": path,
                "url": url,
            }
        )
    return assets


def _secure_url(value: str) -> str:
    parsed = urlparse(value)
    if (
        parsed.scheme != "https"
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
    ):
        raise ValueError(f"更新包地址必须是匿名可访问的 HTTPS URL：{value}")
    return value


def _build_manifest(args: argparse.Namespace) -> dict[str, object]:
    raw_assets = _asset_arguments(args.asset)
    if not raw_assets:
        raise ValueError("至少需要一个 --asset")

    base_url = args.base_url
    if base_url is not None:
        base_url = _secure_url(base_url)

    notes = args.notes or ""
    if args.notes_file is not None:
        notes = Path(args.notes_file).expanduser().read_text(encoding="utf-8")
    notes = notes.strip()

    output_assets: list[dict[str, object]] = []
    for asset in raw_assets:
        path = asset["path"]
        assert isinstance(path, Path)
        explicit_url = asset["url"]
        if explicit_url is not None:
            url = _secure_url(str(explicit_url))
        elif base_url is not None:
            url = _secure_url(urljoin(base_url.rstrip("/") + "/", quote(path.name)))
        else:
            raise ValueError("请提供 --base-url，或给每个 --asset 指定 URL")
        entry: dict[str, object] = {
            "platform": asset["platform"],
            "url": url,
            "sha256": _sha256(path),
            "file_name": path.name,
            "size": path.stat().st_size,
        }
        architecture = asset["architecture"]
        if architecture is not None:
            entry["architecture"] = architecture
        output_assets.append(entry)

    published_at = args.published_at
    if published_at is None:
        published_at = (
            datetime.now(timezone.utc)
            .replace(microsecond=0)
            .isoformat()
            .replace("+00:00", "Z")
        )
    return {
        "version": args.version,
        "build": args.build,
        "published_at": published_at,
        "release_notes": notes,
        "assets": output_assets,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="生成 M-GCS update.json，并计算每个更新包的 SHA-256。",
        epilog=(
            "示例："
            "python3 tools/update_manifest.py --version 1.3.0 --build 4 "
            "--base-url https://download.example.com/mgcs/1.3.0 "
            "--asset macos arm64 dist/M-GCS-macos-arm64.zip "
            "--asset windows x64 dist/M-GCS-windows-x64.zip "
            "--notes-file RELEASE_NOTES.md --output dist/update.json"
        ),
    )
    parser.add_argument("--version", required=True, type=_version)
    parser.add_argument("--build", required=True, type=_build)
    parser.add_argument("--notes", default="", help="直接提供纯文本发布说明")
    parser.add_argument("--notes-file", help="从 UTF-8 文件读取发布说明")
    parser.add_argument(
        "--base-url",
        help="安装包统一下载目录；文件名会自动拼到该地址后面",
    )
    parser.add_argument(
        "--asset",
        action="append",
        nargs="+",
        default=[],
        metavar=("PLATFORM ARCH PATH [URL]"),
        help="可重复；ARCH 填 - 表示不限架构；也可用第 4 项覆盖下载 URL",
    )
    parser.add_argument("--published-at", help="ISO-8601 发布时间，默认当前 UTC")
    parser.add_argument("--output", type=Path, help="输出文件；省略时写到 stdout")
    return parser


def main() -> int:
    parser = _parser()
    args = parser.parse_args()
    try:
        manifest = _build_manifest(args)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    payload = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if args.output is None:
        sys.stdout.write(payload)
    else:
        output = args.output.expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(payload, encoding="utf-8")
        print(f"[update-manifest] 已生成：{output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
