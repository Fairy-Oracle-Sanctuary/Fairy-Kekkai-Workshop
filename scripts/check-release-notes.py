#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""校验 RELEASE_NOTES.md 是否满足客户端「检查更新」的解析契约。

程序内的「检查更新」完全由 GitHub Release 正文驱动（C++ 见
``cpp/src/service/version_service.cpp``，Python 参考实现见
``app/service/version_service.py``），正文格式因此是有约束的接口。发布前跑一遍
本脚本，可以在打 tag 之前就发现「解析不到更新日志」「安装包链接缺一个（404）」
「换了引擎却忘了加 !OCRUPDATE! 导致所有用户只拿到 Clear 包」这类问题。

用法::

    python scripts/check-release-notes.py --version 3.0.0
    python scripts/check-release-notes.py --version 3.0.0 --check-ocr-update
    python scripts/check-release-notes.py --require-ocr-update
    python scripts/check-release-notes.py --notes RELEASE_NOTES.md

退出码 0 表示通过，1 表示有不满足契约的地方。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request

DEFAULT_NOTES = "RELEASE_NOTES.md"
DEFAULT_REPO = "Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop"
MARKER = "!OCRUPDATE!"

# 安装包文件名形如
#   Fairy-Kekkai-Workshop-v3.0.0-CPU-v3.7.0-Windows-x86_64-Setup.exe
#   Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe
#   Fairy-Kekkai-Workshop-v3.0.0-Clear-Windows-x86_64-Setup.exe
INSTALLER_RE = re.compile(
    r"Fairy-Kekkai-Workshop-v(?P<version>[0-9][0-9.]*)-(?P<variant>[A-Za-z0-9.\-]+)"
    r"-Windows-x86_64-Setup\.exe"
)
# 从变体名里抠出 OCR 引擎代次（CPU-v3.7.0 / GPU-v3.7.0-CUDA-11.8 -> 3.7.0）
ENGINE_RE = re.compile(r"(?:CPU|GPU)-v?(?P<engine>[0-9][0-9.]*)")
LINK_RE = re.compile(r"\[(?P<label>[^\]]*)\]\((?P<url>[^)\s]+)\)")
HEADING_RE = re.compile(r"(?m)^##(?!#)\s*(?P<title>\S.*?)\s*$")

# (人类可读名称, 变体段必须满足的条件, 提示里期望的变体段写法)
REQUIRED_VARIANTS = [
    ("CPU", lambda v: v == "CPU" or v.startswith("CPU-"), "CPU-v<引擎版本>"),
    ("GPU CUDA 11.8",
     lambda v: v.startswith("GPU-") and "CUDA-11.8" in v,
     "GPU-v<引擎版本>-CUDA-11.8"),
    ("GPU CUDA 12.9",
     lambda v: v.startswith("GPU-") and "CUDA-12.9" in v,
     "GPU-v<引擎版本>-CUDA-12.9"),
    ("Clear 增量包", lambda v: v == "Clear", "Clear"),
]


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.notes: list[str] = []

    def ok(self, message: str) -> None:
        self.notes.append(message)
        print(f"[ ok ] {message}")

    def warn(self, message: str) -> None:
        self.warnings.append(message)
        print(f"[warn] {message}")

    def fail(self, message: str) -> None:
        self.errors.append(message)
        print(f"[FAIL] {message}")


def parse_variant(filename: str) -> str:
    """从安装包文件名里取变体段（CPU-v3.7.0 / GPU-v3.7.0-CUDA-11.8 / Clear）。"""
    match = INSTALLER_RE.search(filename)
    return match.group("variant") if match else ""


def parse_version(filename: str) -> str:
    """从安装包文件名里取发布版本号（不含 v）。"""
    match = INSTALLER_RE.search(filename)
    return match.group("version") if match else ""


def engine_of(text: str) -> str:
    """从安装包文件名或链接里取 OCR 引擎代次，取不到返回空串。"""
    match = ENGINE_RE.search(text)
    return match.group("engine") if match else ""


def split_sections(text: str) -> dict:
    """按二级标题切分正文，返回 {标题: 该节正文}。"""
    sections: dict = {}
    matches = list(HEADING_RE.finditer(text))
    for index, match in enumerate(matches):
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        sections[match.group("title")] = text[start:end]
    return sections


def collect_installers(section: str) -> list:
    """从下载提示节里取出所有安装包链接（保留出现顺序）。"""
    found = []
    for match in LINK_RE.finditer(section):
        url = match.group("url").strip()
        label = match.group("label").strip()
        name = url.rsplit("/", 1)[-1]
        if not name.endswith("-Windows-x86_64-Setup.exe"):
            continue
        found.append({"label": label, "url": url, "name": name,
                      "variant": parse_variant(name),
                      "version": parse_version(name)})
    return found


def fetch_previous_engines(repo: str, notes_path_hint: str) -> tuple:
    """查上一次 Release 的安装包资产，返回 (引擎代次集合, 说明文字)。

    拿不到网络/接口时返回 (None, 原因)，调用方据此降级为「无法判定」。
    """
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "check-release-notes",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/releases?per_page=10", headers=headers
    )
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            releases = json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, urllib.error.HTTPError, OSError, ValueError) as exc:
        return None, f"无法访问 GitHub API（{exc}）"

    for release in releases:
        if release.get("draft"):
            continue
        engines = set()
        for asset in release.get("assets") or []:
            engine = engine_of(asset.get("name", ""))
            if engine:
                engines.add(engine)
        if engines:
            return engines, f"上一次 Release {release.get('tag_name')} 的引擎代次 {sorted(engines)}"
    return None, "历史 Release 中没有可识别的安装包资产"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="校验 RELEASE_NOTES.md 是否满足客户端「检查更新」的解析契约"
    )
    parser.add_argument("--notes", default=DEFAULT_NOTES, help="发布说明文件路径")
    parser.add_argument("--version", default="",
                        help="本次发布的版本号（不带 v），例如 3.0.0；传入后会校验链接版本")
    parser.add_argument("--repo", default=DEFAULT_REPO, help="GitHub 仓库 owner/name")
    parser.add_argument("--require-ocr-update", action="store_true",
                        help="强制要求正文末尾含 !OCRUPDATE! 标记")
    parser.add_argument("--forbid-ocr-update", action="store_true",
                        help="强制要求正文不含 !OCRUPDATE! 标记")
    parser.add_argument("--check-ocr-update", action="store_true",
                        help="联网比对上一次 Release 的引擎代次，自动判定是否必须带标记")
    args = parser.parse_args(argv)

    report = Report()

    try:
        with open(args.notes, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError as exc:
        print(f"[FAIL] 读不到发布说明 {args.notes}: {exc}")
        return 1

    if text.startswith("\ufeff"):
        text = text.lstrip("\ufeff")
        report.warn("文件带 UTF-8 BOM，建议去掉")

    # --- 1. 二级标题结构 ---------------------------------------------------
    sections = split_sections(text)
    if "更新日志" not in sections:
        report.fail("缺少 `## 更新日志` 二级标题，客户端解析不到更新内容")
    elif not sections["更新日志"].strip():
        report.fail("`## 更新日志` 节内容为空")
    else:
        report.ok("`## 更新日志` 存在且有内容")

    if "下载提示" in sections:
        report.ok("`## 下载提示` 存在")
    else:
        report.fail("缺少 `## 下载提示` 二级标题，客户端拿不到下载地址")
        print()
        print(f"共 {len(report.errors)} 处不满足契约")
        return 1

    # 客户端正则 `## 更新日志\n(.*?)(?=\n## )` 需要后面还有别的二级标题
    titles = list(sections.keys())
    if "更新日志" in titles and titles.index("更新日志") == len(titles) - 1:
        report.fail("`## 更新日志` 是最后一个二级标题，客户端 lookahead 会失败，"
                    "请在它后面至少再保留一个 `## ` 标题")

    # --- 2. 安装包链接 -----------------------------------------------------
    installers = collect_installers(sections["下载提示"])
    if not installers:
        report.fail("`## 下载提示` 中没有 `[名称](URL)` 形式的安装包链接")
    versions = set()
    for name, predicate, expected in REQUIRED_VARIANTS:
        hits = [item for item in installers if predicate(item["variant"])]
        if not hits:
            report.fail(f"`## 下载提示` 缺少 {name} 安装包链接（变体段应为 {expected}）")
            continue
        if len(hits) > 1:
            report.warn(f"{name} 有 {len(hits)} 条链接，客户端只会取第一条")
        for item in hits:
            versions.add(item["version"])
        report.ok(f"{name} 安装包链接存在：{hits[0]['name']}")

    if len(versions) > 1:
        report.fail(f"安装包链接里的版本号不一致：{sorted(versions)}")
    elif versions and args.version:
        found = versions.pop()
        if found != args.version:
            report.fail(f"安装包链接里的版本号 {found} 与本次发布版本 {args.version} 不一致")

    if args.version:
        for item in installers:
            if f"/v{args.version}/" not in item["url"]:
                report.fail(f"链接未指向本次 tag：{item['url']}")

    # --- 3. !OCRUPDATE! 标记 ----------------------------------------------
    stripped = text.strip()
    marker_present = stripped.endswith(MARKER)
    if MARKER in text and not marker_present:
        report.fail(f"正文里出现了 {MARKER}，但它不在正文末尾独占一行，"
                    "客户端 `endsWith` 判定不到")
    elif marker_present:
        report.ok(f"正文末尾含 {MARKER} 标记")

    if args.require_ocr_update and not marker_present:
        report.fail(f"要求本次必须带 {MARKER}，但正文末尾没有")
    if args.forbid_ocr_update and marker_present:
        report.fail(f"要求本次不带 {MARKER}，但正文末尾存在")

    # --- 4. 联网判定引擎代次是否变化 ---------------------------------------
    current_engine = ""
    for item in installers:
        current_engine = current_engine or engine_of(item["name"])
    if current_engine:
        report.ok(f"本次安装包的 OCR 引擎代次：{current_engine}")
    else:
        report.warn("从安装包名里解析不出 OCR 引擎代次（命名需含 CPU-v<版本> / GPU-v<版本>-CUDA-*）")

    if args.check_ocr_update:
        previous, why = fetch_previous_engines(args.repo, args.notes)
        if previous is None:
            report.warn(f"跳过引擎代次比对：{why}")
        else:
            report.ok(why)
            if current_engine and current_engine not in previous:
                if marker_present:
                    report.ok(f"引擎代次由 {sorted(previous)} 换到 {current_engine}，"
                              f"且已带 {MARKER}")
                else:
                    report.fail(f"引擎代次由 {sorted(previous)} 换到 {current_engine}，"
                                f"正文末尾必须加一行 {MARKER}，否则所有用户只会拿到"
                                "不含引擎的 Clear 包")
            elif current_engine:
                if marker_present:
                    report.warn("引擎代次未变却带了 "
                                f"{MARKER}，会让用户重新下载整包（可接受但没必要）")
                else:
                    report.ok("引擎代次未变，无需标记")

    print()
    if report.errors:
        print(f"共 {len(report.errors)} 处不满足契约，共 {len(report.warnings)} 处提醒")
        return 1
    print(f"发布说明满足契约（{len(report.warnings)} 处提醒）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
