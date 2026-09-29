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
        changelog_match = re.search(r"## 更新日志\n(.*?)(?=\n## )", content, re.DOTALL)
        if changelog_match:
            result["changelog"] = changelog_match.group(1).strip()

        # 解析下载链接
        download_match = re.search(r"## 下载提示\n(.*?)(?=\n# |\Z)", content, re.DOTALL)
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
          - PaddleOCR-CPU-v3.7.0
          - PaddleOCR-GPU-v3.7.0-CUDA-11.8
          - PaddleOCR-GPU-v3.7.0-CUDA-12.9

        匹配分三级，逐步放宽（与 C++ 版 VersionService::defaultDownloadUrl 保持一致）：
          1. 链接名包含完整标识（如 "GPU-v3.7.0-CUDA-12.9"）——引擎代次未变时的精确匹配；
          2. 仅按「机型 + 算力」匹配（如 -CPU- / -CUDA-12.9-）——换引擎后版本号跳变时仍能命中整包；
          3. 本地标识读不到（只装过 Clear 包的机器）时退回 CPU 整包——
             任何 Windows x64 机器都能跑，比给出不带引擎的 Clear 包安全。
        任何一级都必须排除 Clear 增量包：它不携带 PADDLEOCR 标识与新模型，
        配上启动时的旧资源清理会得到「旧引擎在、旧模型被删」的失效状态。
        """
        if not PADDLEOCR_VERSION:
            return None

        # 提取 PaddleOCR- 之后的部分作为匹配标识
        m = re.search(r"PaddleOCR-(.+)", PADDLEOCR_VERSION)
        identifier = m.group(1).strip() if m else ""

        def is_installer(text, flavor):
            if not flavor or "clear" in text.lower():
                return False
            upper = text.upper()
            if flavor == "CPU":
                return "-CPU-" in upper
            if "-GPU-" not in upper:
                return False
            if flavor == "GPU":
                return True
            return f"-{flavor.upper()}-" in upper

        # 1) 精确匹配本地标识
        if identifier:
            for name, url in downloads:
                if identifier in name or identifier in url:
                    return url

        # 2) 跨代次：按机型 + 算力匹配同规格整包
        flavor = ""
        upper_identifier = identifier.upper()
        if "CPU" in upper_identifier:
            flavor = "CPU"
        elif "CUDA-11.8" in upper_identifier:
            flavor = "CUDA-11.8"
        elif "CUDA-12.9" in upper_identifier:
            flavor = "CUDA-12.9"
        elif "GPU" in upper_identifier:
            flavor = "GPU"
        if flavor:
            for name, url in downloads:
                if is_installer(name, flavor) or is_installer(url, flavor):
                    return url

        # 3) 读不到标识时退回 CPU 整包
        for name, url in downloads:
            if is_installer(name, "CPU") or is_installer(url, "CPU"):
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
