# coding: utf-8
import re
import subprocess
import sys
from pathlib import Path

import requests
from PySide6.QtCore import QThread, QVersionNumber, Signal

from ..common.setting import PADDLEOCR_VERSION, VERSION


class DownloadThread(QThread):
    """安装包下载线程"""

    progress = Signal(int, int)  # downloaded, total
    succeeded = Signal(str)  # filepath
    error = Signal(str)

    def __init__(self, url, filepath):
        super().__init__()
        self.url = url
        self.filepath = filepath

    def run(self):
        try:
            headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"}
            response = requests.get(
                self.url, headers=headers, stream=True, timeout=30, allow_redirects=True
            )
            response.raise_for_status()

            total = int(response.headers.get("content-length", 0))
            downloaded = 0

            with open(self.filepath, "wb") as f:
                for chunk in response.iter_content(chunk_size=65536):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        self.progress.emit(downloaded, total)

            self.succeeded.emit(self.filepath)
        except Exception as e:
            self.error.emit(str(e))


class VersionService:
    """Version service"""

    GITHUB_API = "https://api.github.com/repos/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/latest"

    def __init__(self):
        self.currentVersion = VERSION
        self.lastestVersion = VERSION
        self.versionPattern = re.compile(r"v(\d+)\.(\d+)\.(\d+)")
        self._releaseInfo = None

    def _fetchReleaseInfo(self):
        """获取并缓存 GitHub release 信息"""
        if self._releaseInfo is not None:
            return self._releaseInfo

        headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
        }
        try:
            response = requests.get(
                self.GITHUB_API, headers=headers, timeout=5, allow_redirects=True
            )
            response.raise_for_status()
            self._releaseInfo = response.json()
        except Exception as e:
            print(f"Error fetching release info: {e}")
        return self._releaseInfo

    def getLatestVersion(self):
        """get latest version"""
        info = self._fetchReleaseInfo()
        if not info:
            return VERSION

        version = info.get("tag_name", "")
        match = self.versionPattern.search(version)
        if not match:
            return VERSION

        self.lastestVersion = version[1:]
        return self.lastestVersion

    def hasNewVersion(self) -> bool:
        """check whether there is a new version"""
        version = QVersionNumber.fromString(self.getLatestVersion())
        currentVersion = QVersionNumber.fromString(self.currentVersion)
        return version > currentVersion

    def getUpdateInfo(self):
        """从 GitHub release body 解析更新日志和下载链接

        Returns:
            dict: {"changelog": str, "downloads": [(name, url), ...],
                   "ocr_update": bool}
        """
        result = {"changelog": "", "downloads": [], "ocr_update": False}
        info = self._fetchReleaseInfo()
        if not info:
            return result

        content = info.get("body", "")
        if not content:
            return result

        # 统一换行符为 \n，避免 \r\n 导致正则匹配失败
        content = content.replace("\r\n", "\n").replace("\r", "\n")

        # 检测并去除末尾的 !OCRUPDATE! 标记（不在 UI 中显示）
        stripped = content.rstrip()
        if stripped.endswith("!OCRUPDATE!"):
            result["ocr_update"] = True
            content = stripped[: -len("!OCRUPDATE!")].rstrip()

        # 解析更新日志
        changelog_match = re.search(
            r"## 更新日志\n(.*?)(?=\n## )", content, re.DOTALL
        )
        if changelog_match:
            result["changelog"] = changelog_match.group(1).strip()

        # 解析下载链接
        download_match = re.search(
            r"## 下载提示\n(.*?)(?=\n# |\Z)", content, re.DOTALL
        )
        if download_match:
            section = download_match.group(1)
            for name, link in re.findall(r"\[([^\]]+)\]\(([^)]+)\)", section):
                result["downloads"].append((name, link))
        return result

    def getDefaultDownloadUrl(self):
        """获取默认下载链接

        - 若更新信息末尾包含 !OCRUPDATE! 标记：根据本地
          PADDLEOCR_VERSION 匹配对应的 CPU/GPU 安装包
        - 否则：返回 Clear 安装包（适用于已安装用户）
        """
        info = self.getUpdateInfo()
        downloads = info["downloads"]

        if info.get("ocr_update"):
            url = self._matchOcrInstaller(downloads)
            if url:
                return url

        for name, url in downloads:
            if "Clear" in name:
                return url

        # 回退：根据版本号构造 Clear 链接
        version = self.lastestVersion
        return (
            f"https://github.com/Fairy-Oracle-Sanctuary/"
            f"Fairy-Kekkai-Workshop/releases/download/v{version}/"
            f"Fairy-Kekkai-Workshop-v{version}-Clear-Windows-x86_64-Setup.exe"
        )

    def _matchOcrInstaller(self, downloads):
        """根据本地 PADDLEOCR_VERSION 匹配对应的安装包 URL

        PADDLEOCR_VERSION 格式:
          - PaddleOCR-CPU-v1.5.1
          - PaddleOCR-GPU-v1.5.1-CUDA-11.8
          - PaddleOCR-GPU-v1.5.1-CUDA-12.9

        下载链接名中包含对应标识（如 "GPU-v1.5.1-CUDA-12.9"）即匹配。
        """
        if not PADDLEOCR_VERSION:
            return None

        # 提取 PaddleOCR- 之后的部分作为匹配标识
        m = re.search(r"PaddleOCR-(.+)", PADDLEOCR_VERSION)
        if not m:
            return None
        ocr_tag = m.group(1).strip()

        for name, url in downloads:
            if ocr_tag in name:
                return url
        return None

    def getDownloadDir(self):
        """获取下载目录"""
        if sys.platform == "win32":
            downloads = Path.home() / "Downloads"
            if downloads.exists():
                return downloads / "Fairy-Kekkai-Workshop"
        import tempfile

        return Path(tempfile.gettempdir()) / "Fairy-Kekkai-Workshop"

    def createDownloadThread(self, url=None):
        """创建下载线程

        Returns:
            (DownloadThread, filepath)
        """
        if url is None:
            url = self.getDefaultDownloadUrl()

        download_dir = self.getDownloadDir()
        download_dir.mkdir(parents=True, exist_ok=True)

        filename = url.split("/")[-1] or "Fairy-Kekkai-Workshop-Setup.exe"
        filepath = str(download_dir / filename)

        thread = DownloadThread(url, filepath)
        return thread, filepath

    @staticmethod
    def openFolder(filepath):
        """在文件管理器中打开文件所在目录"""
        try:
            if sys.platform == "win32":
                subprocess.Popen(f'explorer /select,"{filepath}"')
            elif sys.platform == "darwin":
                subprocess.Popen(["open", "-R", filepath])
            else:
                subprocess.Popen(["xdg-open", str(Path(filepath).parent)])
        except Exception as e:
            print(f"Error opening folder: {e}")
