from videocr.api import save_subtitles_to_file

save_subtitles_to_file(
    video_path=r"D:\Touhou-project\projects\夜回三\10\生肉.mp4",
    file_path=r"D:\Touhou-project\projects\夜回三\10\原文.srt",
    temp_dir=r"D:\Touhou-project\projects\temp",
    lang="japan",
    use_fullframe=True,
    conf_threshold=10,
    sim_threshold=65,
    ssim_threshold=90,
    subtitle_position="any",
    paddleocr_path=r"D:\Touhou-project\projects\Fairy-Kekkai-Workshop\tools\PaddleOCR-GPU-v1.4.0-CUDA-12.9\paddleocr.exe",
    supportFilesPath=r"D:\Touhou-project\projects\Fairy-Kekkai-Workshop\tools\PaddleOCR.PP-OCRv5.support.files",
    time_end="0:30",
    use_gpu=True,
    use_server_model=True,
)
