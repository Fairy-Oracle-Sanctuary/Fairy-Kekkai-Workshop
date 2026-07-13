from videocr.api import save_subtitles_to_file

save_subtitles_to_file(
    video_path=r"C:\Users\ZHANGBaoHang\Downloads\1.mkv",
    file_path=r"C:\Users\ZHANGBaoHang\Downloads\1.srt",
    temp_dir=r"D:\CODE\Fairy-Kekkai-Workshop\temp1",
    # ── 语言 & 引擎 ──
    lang="japan",
    ocr_engine="paddleocr",
    use_server_model=True,
    use_gpu=True,
    # ── 时间范围 ──
    time_start="0:00",
    time_end="0:30",
    # ── 检测区域 ──
    use_fullframe=False,
    subtitle_position="any",
    crop_zones=[{"x": 12, "y": 582, "width": 1265, "height": 135}],
    # ── 去重 ──
    ssim_threshold=95,
    frames_to_skip=0,
    # ── 识别 ──
    ocr_image_max_width=1280,
    conf_threshold=0.3,
    # ── 字幕合并 ──
    sim_threshold=65,
    max_merge_gap_sec=0.1,
    post_processing=False,
    min_subtitle_duration_sec=0.2,
    # ── 路径 ──
    paddleocr_path=r"D:\Program Files\VideOCR\PaddleOCR-GPU-v1.4.0-CUDA-12.9\paddleocr.exe",
    support_files_path=r"D:\Program Files\VideOCR\PaddleOCR.PP-OCRv5.support.files",
)
