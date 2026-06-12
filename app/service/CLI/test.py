from videocr.api import save_subtitles_to_file

save_subtitles_to_file(
    video_path=r"D:\Touhou-project\projects\夜回三\10\生肉.mp4",
    file_path=r"D:\Touhou-project\projects\夜回三\10\原文.srt",
    temp_dir=r"D:\Touhou-project\projects\temp",
    # ── 语言 & 引擎 ──
    lang="japan",
    ocr_engine="paddleocr",
    use_server_model=True,
    use_gpu=True,
    # ── 时间范围 ──
    time_start="0:00",
    time_end="0:30",
    # ── 检测区域 ──
    use_fullframe=True,
    subtitle_position="any",
    # ── 去重 ──
    ssim_threshold=95,
    frames_to_skip=0,
    # ── 识别 ──
    ocr_image_max_width=1280,
    confidence_threshold=0.3,
    # ── 字幕合并 ──
    sim_threshold=65,
    max_merge_gap_sec=0.1,
    post_processing=False,
    min_subtitle_duration_sec=0.2,
    # ── 路径 ──
    paddleocr_path=r"D:\CODE\Fairy-Kekkai-Workshop\tools\PaddleOCR\paddleocr.exe",
    supportFilesPath=r"D:\CODE\Fairy-Kekkai-Workshop\tools\OCR.model",
)
