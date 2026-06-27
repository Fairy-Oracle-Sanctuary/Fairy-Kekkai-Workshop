import json
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

from PySide6.QtCore import QThread, Signal

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.events import EventBuilder
from ..common.text import Text
from .project_service import project

CREATE_NO_WINDOW = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


class DownloadListThread(QThread):
    """下载线程 - 使用QProcess版本"""

    finished_signal = Signal(bool, str, bool)  # 成功/失败, 消息, 是否全部完成

    def __init__(self, url, project_name, project_title):
        super().__init__()
        self.globalText = Text()
        self.url = url
        self.project_name = project_name
        self.project_title = project_title

    def run(self):
        """下载整个视频列表"""
        url_list = []
        ytdlp_path = cfg.get(cfg.ytdlpPath)

        if not ytdlp_path or not os.path.exists(ytdlp_path):
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto012,
                True,
            )
            return

        command = [ytdlp_path, "-j", "--flat-playlist", self.url]

        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=60,
                check=False,
                creationflags=CREATE_NO_WINDOW,
            )
        except subprocess.TimeoutExpired:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto013,
                True,
            )
            return
        except OSError:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto012,
                True,
            )
            return

        if result.returncode != 0 and not result.stdout.strip():
            self.finished_signal.emit(
                False,
                result.stderr.strip() or self.globalText.TextAuto012,
                True,
            )
            return

        for line in result.stdout.splitlines():
            line = line.strip()
            if not line.startswith("{"):
                continue

            try:
                item = json.loads(line)
            except json.JSONDecodeError:
                continue

            title = item.get("title")
            if title is None:
                continue

            title = str(title).strip()
            if not title:
                continue

            video_url = item.get("webpage_url") or item.get("url")
            video_id = item.get("id")
            if video_url and not str(video_url).startswith("http") and video_id:
                video_url = f"https://www.youtube.com/watch?v={video_id}"
            elif not video_url and video_id:
                video_url = f"https://www.youtube.com/watch?v={video_id}"

            if not video_url:
                continue

            url_list.append([title, video_url])

        if not url_list:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto012,
                True,
            )
            return

        project.creat_files(self.project_name, len(url_list), self.project_title)

        with open(self.project_name + "/标题.txt", "w", encoding="utf-8") as title:
            for title_name in url_list:
                title.write(title_name[0])
                title.write("\n")
            title.write("\n")
            for title_name in url_list:
                title.write(title_name[0])
                title.write("\n")
                title.write(title_name[-1])
                title.write("\n\n")

        self.finished_signal.emit(
            True,
            self.globalText.TextAuto009,
            False,
        )

        video_list = []
        for i in url_list:
            video_list.append(i[-1])
        for num, video_url in enumerate(video_list):
            event_bus.download_requested.emit(
                EventBuilder.download_video(
                    video_url,
                    str(project.projects_location / self.project_name / str(num + 1)),
                )
            )

        cover_tasks = []
        max_workers = min(4, max(1, len(url_list)))
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            for i, video_url in enumerate(video_list, start=1):
                cover_tasks.append(
                    executor.submit(
                        self.download_image,
                        video_url,
                        self.project_name + "/" + str(i) + "/封面.jpg",
                    )
                )
            for task in as_completed(cover_tasks):
                task.result()

        self.finished_signal.emit(
            True,
            self.globalText.TextAuto010,
            False,
        )

        self.finished_signal.emit(
            True,
            self.globalText.TextAuto011,
            True,
        )

    def download_image(self, video_url, save_path):
        if not video_url:
            return

        ytdlp_path = cfg.get(cfg.ytdlpPath)
        if not ytdlp_path or not os.path.exists(ytdlp_path):
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto012,
                False,
            )
            return

        output_dir = os.path.dirname(save_path)
        output_template = os.path.join(output_dir, "封面.%(ext)s")
        command = [
            ytdlp_path,
            "--skip-download",
            "--write-thumbnail",
            "--convert-thumbnails",
            "jpg",
            "-o",
            output_template,
            video_url,
        ]

        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=120,
                check=False,
                creationflags=CREATE_NO_WINDOW,
            )
        except subprocess.TimeoutExpired as e:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto015.format(e),
                False,
            )
            return
        except OSError as e:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto015.format(e),
                False,
            )
            return

        if result.returncode != 0:
            self.finished_signal.emit(
                False,
                self.globalText.TextAuto015.format(result.stderr.strip()),
                False,
            )
            return

        self.finished_signal.emit(
            True,
            self.globalText.TextAuto014.format(save_path),
            False,
        )
