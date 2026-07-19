import os

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import QGridLayout, QHBoxLayout, QVBoxLayout, QWidget
from qfluentwidgets import (
    CheckBox,
    ComboBox,
    LineEdit,
    MessageBoxBase,
    PillPushButton,
    PlainTextEdit,
    ProgressBar,
    ProgressRing,
    PushButton,
    ScrollArea,
    StrongBodyLabel,
    SubtitleLabel,
)

from ..common.event_bus import event_bus
from ..service.project_service import project
from ..service.version_service import VersionService
from ..common.text import Text


class BaseInputDialog(MessageBoxBase):
    """基础输入对话框，提供通用功能"""

    def __init__(self, title, min_width=400, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.titleLabel = SubtitleLabel(title)
        self.viewLayout.addWidget(self.titleLabel)

        # 设置按钮文本
        self.yesButton.setText(self.globalText.OK)
        self.cancelButton.setText(self.globalText.Cancel)

        # 设置最小宽度
        self.widget.setMinimumWidth(min_width)

    def validate_non_empty(self, fields):
        """验证字段是否非空"""
        errors = []
        for field_name, field_value, error_msg in fields:
            if not field_value.strip():
                errors.append(error_msg)
        return errors

    def accept(self):
        """重写接受方法，停止定时器"""
        if hasattr(self, "timer"):
            self.timer.stop()
        super().accept()


class AddProject(BaseInputDialog):
    """添加新项目"""

    def __init__(self, parent=None):
        globalText = Text()
        super().__init__(globalText.AddNewProject, min_width=450, parent=parent)
        self.globalText = globalText
        self.setup_ui()

    def setup_ui(self):
        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        self.nameInput = LineEdit(self)
        self.nameInput.setPlaceholderText(self.globalText.EnterProjectName)

        self.numInput = LineEdit(self)
        self.numInput.setPlaceholderText(self.globalText.ETNOEITS)

        self.titleInput = LineEdit(self)
        self.titleInput.setPlaceholderText(self.globalText.EOTOTS)

        fields = [
            (self.globalText.ProjectName, self.nameInput),
            (self.globalText.TotalEpisodes, self.numInput),
            (self.globalText.OriginalTitle, self.titleInput),
        ]

        for row, (label_text, widget) in enumerate(fields):
            label = StrongBodyLabel(label_text, self)
            grid_layout.addWidget(label, row, 0)
            grid_layout.addWidget(widget, row, 1)

        # 设置列拉伸，使输入框能够扩展
        grid_layout.setColumnStretch(1, 1)

    def validateInput(self):
        errors = []
        base_path = ""
        project_name = self.nameInput.text()
        subfolder_count = self.numInput.text()
        title = self.titleInput.text()

        # 验证非空字段
        non_empty_checks = [
            (self.globalText.ProjectName2, project_name, self.globalText.PEPN),
            (self.globalText.TotalEpisodes2, subfolder_count, self.globalText.PETNOE),
            (self.globalText.OriginalTitle2, title, self.globalText.PEOT),
        ]
        errors.extend(self.validate_non_empty(non_empty_checks))

        # 检查项目是否已存在
        if project_name.strip() and os.path.exists(base_path + project_name):
            errors.append(self.globalText.EFAE.format(project_name))

        # 验证集数格式
        if subfolder_count.strip():
            try:
                subfolder_count_int = int(subfolder_count)
                if subfolder_count_int <= 0:
                    errors.append(self.globalText.TEMBGT0)
                elif subfolder_count_int >= 128:
                    errors.append(self.globalText.TEMBLT1)
            except ValueError:
                errors.append(self.globalText.TEMBAVN)

        return errors


class AddProjectFromPlaylist(BaseInputDialog):
    """添加新项目"""

    def __init__(self, parent=None):
        globalText = Text()
        super().__init__(
            globalText.ANPFP, min_width=450, parent=parent
        )
        self.globalText = globalText
        self.setup_ui()

    def setup_ui(self):
        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        self.urlInput = LineEdit(self)
        self.urlInput.setPlaceholderText(self.globalText.EnterPlaylistURL)

        self.nameInput = LineEdit(self)
        self.nameInput.setPlaceholderText(self.globalText.EnterProjectName)

        self.titleInput = LineEdit(self)
        self.titleInput.setPlaceholderText(self.globalText.EOTOTS)

        fields = [
            (self.globalText.VideoListURL, self.urlInput),
            (self.globalText.ProjectName, self.nameInput),
            (self.globalText.OriginalTitle, self.titleInput),
        ]

        for row, (label_text, widget) in enumerate(fields):
            label = StrongBodyLabel(label_text, self)
            grid_layout.addWidget(label, row, 0)
            grid_layout.addWidget(widget, row, 1)

        # 设置列拉伸，使输入框能够扩展
        grid_layout.setColumnStretch(1, 1)

    def validateInput(self):
        errors = []
        base_path = ""
        list_url = self.urlInput.text()
        project_name = self.nameInput.text()
        title = self.titleInput.text()

        # 验证非空字段
        non_empty_checks = [
            (self.globalText.VideoListURL2, list_url, self.globalText.PEPU),
            (self.globalText.ProjectName2, project_name, self.globalText.PEPN),
            (self.globalText.OriginalTitle2, title, self.globalText.PEOT),
        ]
        errors.extend(self.validate_non_empty(non_empty_checks))

        # 检查项目是否已存在
        if project_name.strip() and os.path.exists(base_path + project_name):
            errors.append(self.globalText.EFAE.format(project_name))

        return errors


class CustomMessageBox(BaseInputDialog):
    """单输入对话框"""

    def __init__(self, title, text, min_width=350, parent=None):
        super().__init__(title, min_width, parent)
        self.globalText = Text()

        self.LineEdit = LineEdit()
        self.LineEdit.setPlaceholderText(text)
        self.LineEdit.setClearButtonEnabled(True)

        # 将组件添加到布局中
        self.viewLayout.addWidget(self.LineEdit)

    def validateInput(self):
        return self.validate_non_empty(
            [
                (
                    self.globalText.InputContent,
                    self.LineEdit.text().strip(),
                    self.globalText.PleaseEnterContent,
                )
            ]
        )


class CustomDoubleMessageBox(BaseInputDialog):
    """双输入对话框"""

    def __init__(
        self,
        title,
        input1,
        input2,
        text1,
        text2,
        error1,
        error2,
        min_width=400,
        parent=None,
    ):
        super().__init__(title, min_width, parent)
        self.globalText = Text()
        self.error1 = error1
        self.error2 = error2
        self.setup_ui(input1, input2, text1, text2)

    def setup_ui(self, input1, input2, text1, text2):
        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        self.input_label_1 = StrongBodyLabel(input1, self)
        self.input_label_2 = StrongBodyLabel(input2, self)

        self.LineEdit_1 = LineEdit()
        self.LineEdit_2 = LineEdit()

        self.LineEdit_1.setPlaceholderText(text1)
        self.LineEdit_1.setClearButtonEnabled(True)

        self.LineEdit_2.setPlaceholderText(text2)
        self.LineEdit_2.setClearButtonEnabled(True)

        # 将输入框组件添加到网格布局中
        grid_layout.addWidget(self.input_label_1, 0, 0)
        grid_layout.addWidget(self.LineEdit_1, 0, 1)
        grid_layout.addWidget(self.input_label_2, 1, 0)
        grid_layout.addWidget(self.LineEdit_2, 1, 1)

        # 设置列拉伸，使输入框能够扩展
        grid_layout.setColumnStretch(1, 1)

        # 左对齐
        grid_layout.setAlignment(Qt.AlignmentFlag.AlignLeft)

    def validateInput(self):
        return self.validate_non_empty(
            [
                (self.globalText.FirstInput, self.LineEdit_1.text().strip(), self.error1),
                (self.globalText.SecondInput, self.LineEdit_2.text().strip(), self.error2),
            ]
        )


class CustomTripleMessageBox(BaseInputDialog):
    """三输入对话框"""

    def __init__(
        self,
        title,
        input1,
        input2,
        input3,
        text1,
        text2,
        text3,
        error1,
        error2,
        error3,
        min_width=450,
        parent=None,
    ):
        super().__init__(title, min_width, parent)
        self.error1 = error1
        self.error2 = error2
        self.error3 = error3
        self.setup_ui(input1, input2, input3, text1, text2, text3)

    def setup_ui(self, input1, input2, input3, text1, text2, text3):
        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        # 创建标签和输入框
        self.input_label_1 = StrongBodyLabel(input1, self)
        self.input_label_2 = StrongBodyLabel(input2, self)
        self.input_label_3 = StrongBodyLabel(input3, self)

        self.LineEdit_1 = LineEdit()
        self.LineEdit_2 = LineEdit()
        self.LineEdit_3 = LineEdit()

        # 设置占位符文本和清除按钮
        inputs = [
            (self.LineEdit_1, text1),
            (self.LineEdit_2, text2),
            (self.LineEdit_3, text3),
        ]

        for line_edit, placeholder in inputs:
            line_edit.setPlaceholderText(placeholder)
            line_edit.setClearButtonEnabled(True)

        # 将组件添加到网格布局
        grid_layout.addWidget(self.input_label_1, 0, 0)
        grid_layout.addWidget(self.LineEdit_1, 0, 1)
        grid_layout.addWidget(self.input_label_2, 1, 0)
        grid_layout.addWidget(self.LineEdit_2, 1, 1)
        grid_layout.addWidget(self.input_label_3, 2, 0)
        grid_layout.addWidget(self.LineEdit_3, 2, 1)

        # 设置列拉伸，使输入框能够扩展
        grid_layout.setColumnStretch(1, 1)

        # 左对齐
        grid_layout.setAlignment(Qt.AlignmentFlag.AlignLeft)

    def validateInput(self):
        return self.validate_non_empty(
            [
                ("第一个输入", self.LineEdit_1.text().strip(), self.error1),
                ("第二个输入", self.LineEdit_2.text().strip(), self.error2),
                ("第三个输入", self.LineEdit_3.text().strip(), self.error3),
            ]
        )


class EditProjectDialog(BaseInputDialog):
    """编辑项目对话框（包含图标选择）"""

    def __init__(self, current_name, current_title, current_icon, parent=None):
        globalText = Text()
        super().__init__(globalText.EditProject, min_width=500, parent=parent)
        self.globalText = globalText
        self.current_name = current_name
        self.current_title = current_title
        self.current_icon = current_icon
        self.setup_ui()

    def setup_ui(self):
        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        # 文件夹名
        self.nameLabel = StrongBodyLabel(self.globalText.FolderName, self)
        self.nameInput = LineEdit(self)
        self.nameInput.setText(self.current_name)
        self.nameInput.setClearButtonEnabled(True)

        # 原标题
        self.titleLabel = StrongBodyLabel(self.globalText.OriginalTitle, self)
        self.titleInput = LineEdit(self)
        self.titleInput.setText(self.current_title)
        self.titleInput.setClearButtonEnabled(True)

        # 图标选择
        self.iconLabel = StrongBodyLabel(self.globalText.Icon, self)
        self.iconComboBox = ComboBox(self)

        # 添加图标选项
        icon_display_names = {
            ":/app/images/icons/牛排.svg": "牛排",
            ":/app/images/icons/牛油果.svg": "牛油果",
            ":/app/images/icons/番茄.svg": "番茄",
            ":/app/images/icons/豆腐.svg": "豆腐",
            ":/app/images/icons/紫薯.svg": "紫薯",
            ":/app/images/icons/生菜.svg": "生菜",
            ":/app/images/logo.ico": "默认",
        }

        self.iconComboBox.addItems(list(icon_display_names.values()))
        self.iconComboBox.setCurrentIndex(
            list(icon_display_names.keys()).index(self.current_icon)
            if self.current_icon in icon_display_names
            else 0
        )

        # 添加到布局
        grid_layout.addWidget(self.nameLabel, 0, 0)
        grid_layout.addWidget(self.nameInput, 0, 1)
        grid_layout.addWidget(self.titleLabel, 1, 0)
        grid_layout.addWidget(self.titleInput, 1, 1)
        grid_layout.addWidget(self.iconLabel, 2, 0)
        grid_layout.addWidget(self.iconComboBox, 2, 1)

        # 设置列拉伸
        grid_layout.setColumnStretch(1, 1)
        grid_layout.setAlignment(Qt.AlignmentFlag.AlignLeft)

    def validateInput(self):
        errors = []
        if not self.nameInput.text().strip():
            errors.append(self.globalText.PEFN)
        if not self.titleInput.text().strip():
            errors.append(self.globalText.PEOT)
        return errors

    def get_selected_icon(self):
        """获取选中的图标路径"""
        icon_display_names = {
            "牛排": ":/app/images/icons/牛排.svg",
            "牛油果": ":/app/images/icons/牛油果.svg",
            "番茄": ":/app/images/icons/番茄.svg",
            "豆腐": ":/app/images/icons/豆腐.svg",
            "紫薯": ":/app/images/icons/紫薯.svg",
            "生菜": ":/app/images/icons/生菜.svg",
            "默认": ":/app/images/logo.ico",
        }
        return icon_display_names[self.iconComboBox.currentText()]


class ProjectProgressDialog(MessageBoxBase):
    """项目进度对话框"""

    def __init__(self, progress, title, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.progress = progress
        self.title = title
        self.setup_ui()

    def setup_ui(self):
        self.yesButton.setText(self.globalText.GotIt)
        self.cancelButton.setVisible(False)

        progress_all = sum(self.progress) / 5

        progress_view = QHBoxLayout()
        self.viewLayout.addLayout(progress_view)

        self.titleLabel = SubtitleLabel(self.title)
        self.progress_pill = PillPushButton(self)
        self.progress_pill.setText(f"{progress_all:.2f}%")
        self.progress_pill.setEnabled(False)
        progress_view.addWidget(self.titleLabel)
        progress_view.addStretch(1)
        progress_view.addWidget(self.progress_pill)

        grid_layout = QGridLayout()
        self.viewLayout.addLayout(grid_layout)

        # 一个项目的每一集都有一个封面, 一个原视频，一个翻译后的视频，一个原字幕，一个翻译后的字幕，对应五个ring
        self.ring_cover = ProgressRing(self)
        self.ring_video = ProgressRing(self)
        self.ring_translated_video = ProgressRing(self)
        self.ring_subtitle = ProgressRing(self)
        self.ring_translated_subtitle = ProgressRing(self)

        self.label_cover = StrongBodyLabel(self.globalText.Cover, self)
        self.label_video = StrongBodyLabel(self.globalText.OriginalVideo, self)
        self.label_translated_video = StrongBodyLabel(self.globalText.TranslatedVideo, self)
        self.label_subtitle = StrongBodyLabel(self.globalText.OriginalSubtitle, self)
        self.label_translated_subtitle = StrongBodyLabel(self.globalText.TranslatedSubtitle, self)

        rings = [
            self.ring_cover,
            self.ring_video,
            self.ring_translated_video,
            self.ring_subtitle,
            self.ring_translated_subtitle,
        ]
        for ring, p in zip(rings, self.progress):
            ring.setMinimum(0)
            ring.setMaximum(100)
            ring.setValue(int(p))
            ring.setTextVisible(True)

        # 设置所有标签文字居中
        labels = [
            self.label_cover,
            self.label_video,
            self.label_translated_video,
            self.label_subtitle,
            self.label_translated_subtitle,
        ]

        for label in labels:
            label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        grid_layout.addWidget(self.ring_cover, 0, 0)
        grid_layout.addWidget(self.ring_video, 0, 1)
        grid_layout.addWidget(self.ring_translated_video, 0, 2)
        grid_layout.addWidget(self.ring_subtitle, 0, 3)
        grid_layout.addWidget(self.ring_translated_subtitle, 0, 4)

        grid_layout.addWidget(self.label_cover, 1, 0)
        grid_layout.addWidget(self.label_video, 1, 1)
        grid_layout.addWidget(self.label_translated_video, 1, 2)
        grid_layout.addWidget(self.label_subtitle, 1, 3)
        grid_layout.addWidget(self.label_translated_subtitle, 1, 4)

        self.progress_bar = ProgressBar(self)
        self.progress_bar.setMinimum(0)
        self.progress_bar.setMaximum(100)
        self.progress_bar.setValue(int(progress_all))
        self.viewLayout.addWidget(self.progress_bar)


class TranslateProgressDialog(MessageBoxBase):
    """翻译进度对话框"""

    def __init__(self, task=None, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.task = task
        self.current_content = ""  # 存储当前翻译内容
        self.setup_ui()
        if task:
            self.connect_signals()
            # 优先使用output_history，如果为空则从文件读取
            if hasattr(task, "output_history") and task.output_history:
                self.current_content = task.output_history
                self.textEdit.setPlainText(self.current_content)
            elif self.task.output_file and os.path.exists(self.task.output_file):
                try:
                    with open(self.task.output_file, "r", encoding="utf-8") as f:
                        self.current_content = f.read()
                        self.textEdit.setPlainText(self.current_content)
                except Exception as e:
                    print(f"读取翻译文件失败: {e}")

    def setup_ui(self):
        self.titleLabel = SubtitleLabel(self.globalText.TranslationProgress)
        self.viewLayout.addWidget(self.titleLabel)

        self.textEdit = PlainTextEdit(self)
        self.textEdit.setReadOnly(True)
        self.textEdit.setPlaceholderText(self.globalText.TTWBDH)
        self.viewLayout.addWidget(self.textEdit)

        self.yesButton.setText(self.globalText.Close)
        self.cancelButton.setVisible(False)

        # 设置对话框大小
        self.widget.setMinimumWidth(600)
        self.widget.setMinimumHeight(500)

    def connect_signals(self):
        """连接实时翻译信号"""
        event_bus.translate_update_signal.connect(self.on_translate_update)

    def on_translate_update(self, task_id, content_chunk):
        """处理实时翻译更新"""
        # 只处理当前任务的更新
        if self.task and str(self.task.id) == task_id:
            self.current_content += content_chunk
            self.textEdit.setPlainText(self.current_content)
            scrollbar = self.textEdit.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def accept(self):
        """重写接受方法，断开信号连接"""
        event_bus.translate_update_signal.disconnect(self.on_translate_update)
        super().accept()


class FFmpegProgressDialog(MessageBoxBase):
    """FFmpeg压制视频进度对话框"""

    def __init__(self, task=None, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.task = task
        self.current_content = ""  # 存储当前输出内容
        self.setup_ui()
        if task:
            # 加载已有的输出历史
            if hasattr(task, "output_history"):
                self.current_content = task.output_history
                self.textEdit.setPlainText(self.current_content)
            self.connect_signals()

    def setup_ui(self):
        self.titleLabel = SubtitleLabel(self.globalText.VEP)
        self.viewLayout.addWidget(self.titleLabel)

        self.textEdit = PlainTextEdit(self)
        self.textEdit.setReadOnly(True)
        self.textEdit.setPlaceholderText(self.globalText.OWBDH)
        self.viewLayout.addWidget(self.textEdit)

        self.yesButton.setText(self.globalText.Close)
        self.cancelButton.setVisible(False)

        # 设置对话框大小
        self.widget.setMinimumWidth(600)
        self.widget.setMinimumHeight(500)

    def connect_signals(self):
        """连接实时ffmpeg输出信号"""
        event_bus.ffmpeg_update_signal.connect(self.on_ffmpeg_update)

    def on_ffmpeg_update(self, task_id, output_chunk):
        """处理实时ffmpeg输出更新"""
        # 只处理当前任务的更新
        if self.task and str(self.task.id) == task_id:
            self.current_content += output_chunk
            self.textEdit.setPlainText(self.current_content)
            scrollbar = self.textEdit.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def accept(self):
        """重写接受方法，断开信号连接"""
        event_bus.ffmpeg_update_signal.disconnect(self.on_ffmpeg_update)
        super().accept()


class ReleaseProgressDialog(MessageBoxBase):
    """B站上传进度对话框"""

    def __init__(self, task=None, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.task = task
        self.current_content = ""  # 存储当前输出内容
        self.setup_ui()
        if task:
            # 加载已有的输出历史
            if hasattr(task, "output_history"):
                self.current_content = task.output_history
                self.textEdit.setPlainText(self.current_content)
            self.connect_signals()

    def setup_ui(self):
        self.titleLabel = SubtitleLabel(self.globalText.VideoUploadProgress)
        self.viewLayout.addWidget(self.titleLabel)

        self.textEdit = PlainTextEdit(self)
        self.textEdit.setReadOnly(True)
        self.textEdit.setPlaceholderText(self.globalText.OWBDH)
        self.viewLayout.addWidget(self.textEdit)

        self.yesButton.setText(self.globalText.Close)
        self.cancelButton.setVisible(False)

        # 设置对话框大小
        self.widget.setMinimumWidth(600)
        self.widget.setMinimumHeight(500)

    def connect_signals(self):
        """连接实时上传输出信号"""
        event_bus.release_update_signal.connect(self.on_release_update)

    def on_release_update(self, task_id, output_chunk):
        """处理实时上传输出更新"""
        # 只处理当前任务的更新
        if self.task and str(self.task.id) == task_id:
            self.current_content += output_chunk
            self.textEdit.setPlainText(self.current_content)
            scrollbar = self.textEdit.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def accept(self):
        """重写接受方法，断开信号连接"""
        event_bus.release_update_signal.disconnect(self.on_release_update)
        super().accept()


class WhisperProgressDialog(MessageBoxBase):
    """Whisper语音识别进度对话框"""

    def __init__(self, task=None, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.task = task
        self.current_content = ""
        self.setup_ui()
        if task:
            # 加载已有的输出历史
            if hasattr(task, "output_history"):
                self.current_content = task.output_history
                self.textEdit.setPlainText(self.current_content)
            self.connect_signals()

    def setup_ui(self):
        self.titleLabel = SubtitleLabel(self.globalText.SRP)
        self.viewLayout.addWidget(self.titleLabel)

        self.textEdit = PlainTextEdit(self)
        self.textEdit.setReadOnly(True)
        self.textEdit.setPlaceholderText(self.globalText.OWBDH)
        self.viewLayout.addWidget(self.textEdit)

        self.yesButton.setText(self.globalText.Close)
        self.cancelButton.setVisible(False)

        # 设置对话框大小
        self.widget.setMinimumWidth(600)
        self.widget.setMinimumHeight(500)

    def connect_signals(self):
        """连接实时Whisper输出信号"""
        event_bus.whisper_update_signal.connect(self.on_whisper_update)

    def on_whisper_update(self, task_id, output_chunk):
        """处理实时Whisper输出更新"""
        # 只处理当前任务的更新
        if self.task and str(self.task.id) == task_id:
            self.current_content += output_chunk
            self.textEdit.setPlainText(self.current_content)
            scrollbar = self.textEdit.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def accept(self):
        """重写接受方法，断开信号连接"""
        event_bus.whisper_update_signal.disconnect(self.on_whisper_update)
        super().accept()


class BatchTaskDialog(MessageBoxBase):
    """批量任务对话框 —— 选择任务类型后显示符合条件剧集，可多选添加"""

    TASK_TYPES = ["下载", "语音识别", "翻译", "压制"]

    def __init__(self, card_id, subfolders, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.card_id = card_id
        self.subfolders = subfolders  # [(folder_num, folder_path), ...]
        self._checkboxes = []  # [(checkbox, folder_num, folder_path)]

        self.titleLabel = SubtitleLabel(self.globalText.BatchAddTasks)
        self.viewLayout.addWidget(self.titleLabel)

        self.widget.setMinimumWidth(520)

        # 任务类型下拉框
        self.taskTypeCombo = ComboBox()
        self.taskTypeCombo.addItems(self.TASK_TYPES)
        self.taskTypeCombo.currentTextChanged.connect(self._on_task_type_changed)
        self.viewLayout.addWidget(self.taskTypeCombo)

        # 全选 / 取消全选
        select_layout = QHBoxLayout()
        select_all_btn = PushButton(self.globalText.SelectAll)
        select_all_btn.clicked.connect(self._select_all)
        deselect_all_btn = PushButton(self.globalText.DeselectAll)
        deselect_all_btn.clicked.connect(self._deselect_all)
        select_layout.addWidget(select_all_btn)
        select_layout.addWidget(deselect_all_btn)
        select_layout.addStretch()
        self.viewLayout.addLayout(select_layout)

        # 可滚动剧集列表

        self.episodeScrollArea = ScrollArea()
        self.episodeScrollArea.setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
        )
        self.episodeWidget = QWidget()
        self.episodeWidget.setStyleSheet("background: transparent;")
        self.episodeLayout = QVBoxLayout(self.episodeWidget)
        self.episodeLayout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.episodeScrollArea.setWidget(self.episodeWidget)
        self.episodeScrollArea.setWidgetResizable(True)
        self.episodeScrollArea.setFixedHeight(320)
        self.viewLayout.addWidget(self.episodeScrollArea)

        # 初始化列表
        self._on_task_type_changed(self.taskTypeCombo.currentText())

        self.yesButton.setText(self.globalText.AddTask)
        self.yesButton.setEnabled(False)
        self.cancelButton.setText(self.globalText.Cancel)

    def _on_task_type_changed(self, task_type):
        """切换任务类型时刷新剧集列表"""
        self._clear_checkboxes()

        for folder_num, folder_path in self.subfolders:
            eligible, label = self._check_eligible(task_type, folder_num, folder_path)
            if eligible:
                cb = CheckBox(label)
                cb.stateChanged.connect(self._on_check_state_changed)
                self.episodeLayout.addWidget(cb)
                self._checkboxes.append((cb, folder_num, folder_path))

        self.episodeLayout.addStretch()
        self._update_confirm_button()

    def _on_check_state_changed(self):
        """任一 checkbox 状态改变时更新确认按钮"""
        self._update_confirm_button()

    def _update_confirm_button(self):
        """有勾选时启用确定按钮"""
        for cb, _, _ in self._checkboxes:
            if cb.isChecked():
                self.yesButton.setEnabled(True)
                return
        self.yesButton.setEnabled(False)

    def _check_eligible(self, task_type, folder_num, folder_path):
        """检查某集是否可添加该类型任务，返回 (是否合格, 显示标签)"""

        raw = str(folder_path)
        idx = folder_num - 1
        sub_title = project.project_subtitle[self.card_id][idx]
        label = self.globalText.Episode.format(folder_num, sub_title)

        if task_type == "下载":
            video_url = project.project_video_url[self.card_id][idx]
            has_raw = os.path.exists(os.path.join(raw, "生肉.mp4"))
            if video_url and video_url.strip() and not has_raw:
                return True, label
            return False, ""

        elif task_type == "语音识别":
            has_raw = os.path.exists(os.path.join(raw, "生肉.mp4"))
            has_whisper = os.path.exists(os.path.join(raw, "原文_Whisper.srt"))
            if has_raw and not has_whisper:
                return True, label
            return False, ""

        elif task_type == "翻译":
            has_trans = os.path.exists(os.path.join(raw, "译文.srt"))
            if has_trans:
                return False, ""
            for src_name in ("原文.srt", "原文_OCR.srt", "原文_Whisper.srt"):
                if os.path.exists(os.path.join(raw, src_name)):
                    return True, label
            return False, ""

        elif task_type == "压制":
            has_cooked = os.path.exists(os.path.join(raw, "熟肉.mp4"))
            if has_cooked:
                return True, label
            return False, ""

        return False, ""

    def _clear_checkboxes(self):
        """清空现有 checkbox 列表（含所有布局项）"""
        while self.episodeLayout.count():
            item = self.episodeLayout.takeAt(0)
            w = item.widget()
            if w is not None:
                w.deleteLater()
        self._checkboxes.clear()

    def _select_all(self):
        for cb, _, _ in self._checkboxes:
            cb.setChecked(True)
        self._update_confirm_button()

    def _deselect_all(self):
        for cb, _, _ in self._checkboxes:
            cb.setChecked(False)
        self._update_confirm_button()

    def get_selected(self):
        """返回 [({task_type}, folder_num, folder_path), ...]"""
        task_type = self.taskTypeCombo.currentText()
        selected = []
        for cb, folder_num, folder_path in self._checkboxes:
            if cb.isChecked():
                selected.append((task_type, folder_num, folder_path))
        return selected


class BatchDeleteFileDialog(MessageBoxBase):
    """批量删除文件对话框 —— 选择文件类型后显示存在该文件的剧集，可多选删除"""

    FILE_TYPES = {
        "封面": "封面.jpg",
        "生肉视频": "生肉.mp4",
        "熟肉视频": "熟肉.mp4",
        "原文字幕": "原文.srt",
        "OCR字幕": "原文_OCR.srt",
        "Whisper字幕": "原文_Whisper.srt",
        "译文字幕": "译文.srt",
    }

    def __init__(self, card_id, subfolders, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.card_id = card_id
        self.subfolders = subfolders
        self._checkboxes = []  # [(checkbox, folder_num, folder_path)]

        self.titleLabel = SubtitleLabel(self.globalText.BatchDeleteFiles)
        self.viewLayout.addWidget(self.titleLabel)

        self.widget.setMinimumWidth(520)

        self.fileTypeCombo = ComboBox()
        self.fileTypeCombo.addItems(self.FILE_TYPES.keys())
        self.fileTypeCombo.currentTextChanged.connect(self._on_file_type_changed)
        self.viewLayout.addWidget(self.fileTypeCombo)

        select_layout = QHBoxLayout()
        select_all_btn = PushButton(self.globalText.SelectAll)
        select_all_btn.clicked.connect(self._select_all)
        deselect_all_btn = PushButton(self.globalText.DeselectAll)
        deselect_all_btn.clicked.connect(self._deselect_all)
        select_layout.addWidget(select_all_btn)
        select_layout.addWidget(deselect_all_btn)
        select_layout.addStretch()
        self.viewLayout.addLayout(select_layout)

        self.episodeScrollArea = ScrollArea()
        self.episodeScrollArea.setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
        )
        self.episodeWidget = QWidget()
        self.episodeWidget.setStyleSheet("background: transparent;")
        self.episodeLayout = QVBoxLayout(self.episodeWidget)
        self.episodeLayout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.episodeScrollArea.setWidget(self.episodeWidget)
        self.episodeScrollArea.setWidgetResizable(True)
        self.episodeScrollArea.setFixedHeight(320)
        self.viewLayout.addWidget(self.episodeScrollArea)

        self._on_file_type_changed(self.fileTypeCombo.currentText())

        self.yesButton.setText(self.globalText.DeleteFiles)
        self.yesButton.setEnabled(False)
        self.cancelButton.setText(self.globalText.Cancel)

    def _on_file_type_changed(self, file_type):
        self._clear_checkboxes()

        target_name = self.FILE_TYPES.get(file_type)
        for folder_num, folder_path in self.subfolders:
            idx = folder_num - 1
            sub_title = project.project_subtitle[self.card_id][idx]
            file_path = os.path.join(str(folder_path), target_name)
            if os.path.exists(file_path):
                label = self.globalText.Episode.format(folder_num, sub_title)
                cb = CheckBox(label)
                cb.stateChanged.connect(self._on_check_state_changed)
                self.episodeLayout.addWidget(cb)
                self._checkboxes.append((cb, folder_num, folder_path))

        self.episodeLayout.addStretch()
        self._update_confirm_button()

    def _on_check_state_changed(self):
        self._update_confirm_button()

    def _update_confirm_button(self):
        for cb, _, _ in self._checkboxes:
            if cb.isChecked():
                self.yesButton.setEnabled(True)
                return
        self.yesButton.setEnabled(False)

    def _clear_checkboxes(self):
        while self.episodeLayout.count():
            item = self.episodeLayout.takeAt(0)
            w = item.widget()
            if w is not None:
                w.deleteLater()
        self._checkboxes.clear()

    def _select_all(self):
        for cb, _, _ in self._checkboxes:
            cb.setChecked(True)
        self._update_confirm_button()

    def _deselect_all(self):
        for cb, _, _ in self._checkboxes:
            cb.setChecked(False)
        self._update_confirm_button()

    def get_selected(self):
        """返回 [(folder_num, folder_path, target_file_name), ...]"""
        target_name = self.FILE_TYPES.get(self.fileTypeCombo.currentText())
        selected = []
        for cb, folder_num, folder_path in self._checkboxes:
            if cb.isChecked():
                selected.append((folder_num, folder_path, target_name))
        return selected


class UpdateDialog(MessageBoxBase):
    """更新对话框 - 显示更新日志并下载安装包"""

    def __init__(self, version_service, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.versionService = version_service
        self.downloadThread = None
        self.filepath = ""
        self._downloading = False
        self._downloaded = False

        self.setup_ui()
        # 延迟获取更新信息，让对话框先显示
        QTimer.singleShot(100, self._fetch_update_info)

    def setup_ui(self):
        version = self.versionService.lastestVersion
        self.titleLabel = SubtitleLabel(
            self.globalText.UpdateAvailable.format(version)
        )
        self.viewLayout.addWidget(self.titleLabel)

        # 更新日志
        self.changelogEdit = PlainTextEdit(self)
        self.changelogEdit.setReadOnly(True)
        self.changelogEdit.setPlainText(self.globalText.FetchingUpdateInfo)
        self.viewLayout.addWidget(self.changelogEdit)

        # 进度条
        self.progressBar = ProgressBar(self)
        self.progressBar.setVisible(False)
        self.viewLayout.addWidget(self.progressBar)

        # 按钮
        self.yesButton.setText(self.globalText.DownloadInstaller)
        self.cancelButton.setText(self.globalText.Close)

        self.widget.setMinimumWidth(500)
        self.widget.setMinimumHeight(350)

    def _fetch_update_info(self):
        """获取更新信息"""
        info = self.versionService.getUpdateInfo()
        changelog = info.get("changelog", "")
        if changelog:
            self.changelogEdit.setPlainText(changelog)
        else:
            self.changelogEdit.setPlainText(self.globalText.NoChangelog)

    def accept(self):
        """重写接受方法：首次点击开始下载，再次点击打开文件夹并关闭"""
        if self._downloading:
            return  # 下载进行中，忽略点击

        if self._downloaded:
            # 已下载完成，打开文件夹并关闭
            VersionService.openFolder(self.filepath)
            super().accept()
            return

        # 开始下载
        self._start_download()

    def _start_download(self):
        """开始下载安装包"""
        self._downloading = True
        self.yesButton.setEnabled(False)
        self.yesButton.setText(self.globalText.Downloading3)
        self.cancelButton.setEnabled(False)
        self.progressBar.setVisible(True)
        self.progressBar.setValue(0)

        self.downloadThread, self.filepath = (
            self.versionService.createDownloadThread()
        )
        self.downloadThread.progress.connect(self._on_progress)
        self.downloadThread.succeeded.connect(self._on_finished)
        self.downloadThread.error.connect(self._on_error)
        # 连接 QThread 内置 finished 信号，在线程完全结束后安全清理
        self.downloadThread.finished.connect(self._on_thread_finished)
        self.downloadThread.start()

    def _on_progress(self, downloaded, total):
        if total > 0:
            percent = int(downloaded * 100 / total)
            self.progressBar.setValue(percent)

    def _on_finished(self, filepath):
        """下载成功（线程即将结束，但尚未完全退出）"""
        self.progressBar.setValue(100)
        self.yesButton.setText(self.globalText.OpenFolder)
        self.yesButton.setEnabled(True)
        self.cancelButton.setEnabled(True)
        self._downloading = False
        self._downloaded = True
        self.changelogEdit.appendPlainText(
            f"\n{self.globalText.DownloadedTo.format(filepath)}"
        )
        # 自动打开文件夹
        VersionService.openFolder(filepath)

    def _on_error(self, error_msg):
        """下载失败（线程即将结束，但尚未完全退出）"""
        self.progressBar.setVisible(False)
        self.yesButton.setEnabled(True)
        self.yesButton.setText(self.globalText.DownloadInstaller)
        self.cancelButton.setEnabled(True)
        self._downloading = False
        self.changelogEdit.appendPlainText(
            f"\n{self.globalText.DownloadFailed3.format(error_msg)}"
        )

    def _on_thread_finished(self):
        """QThread 内置 finished 信号：线程已完全停止，可安全释放引用"""
        if self.downloadThread is not None:
            self.downloadThread.deleteLater()
            self.downloadThread = None

    def reject(self):
        """重写拒绝方法：下载进行中时不允许关闭"""
        if self._downloading:
            return
        super().reject()
