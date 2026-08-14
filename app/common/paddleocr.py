"""PaddleOCR 语言分组与模型目录解析（主程序侧副本）。

本模块是 app/service/CLI/videocr/lang_dictionaries.py 中的 PADDLEOCR_LANGS
以及 app/service/CLI/videocr/utils.py 中的 resolve_model_dirs 的完整副本，
仅用于主程序解耦对 CLI 目录的 import 依赖，避免 Nuitka 编译主程序时
静态跟踪到 CLI 的重型第三方库（av / fast_ssim / numpy / cpuid / PIL 等）。

注意：
    - CLI 目录内的原文件保持不动，CLI 仍使用自己的副本。
    - 若 PADDLEOCR_LANGS 或 resolve_model_dirs 业务逻辑变更，需双向同步。
    - 主程序侧不要从此模块导入 GOOGLE_LENS_LANGS（CLI 专用）。
"""

import os
import sys

PADDLEOCR_LANGS = {
    "latin": {
        "af",
        "az",
        "bs",
        "cs",
        "cy",
        "da",
        "de",
        "es",
        "et",
        "fr",
        "ga",
        "hr",
        "hu",
        "id",
        "is",
        "it",
        "ku",
        "la",
        "lt",
        "lv",
        "mi",
        "ms",
        "mt",
        "nl",
        "no",
        "oc",
        "pi",
        "pl",
        "pt",
        "ro",
        "rs_latin",
        "sk",
        "sl",
        "sq",
        "sv",
        "sw",
        "tl",
        "tr",
        "uz",
        "vi",
        "french",
        "german",
        "fi",
        "eu",
        "gl",
        "lb",
        "rm",
        "ca",
        "qu",
    },
    "arabic": {"ar", "fa", "ug", "ur", "ps", "ku", "sd", "bal"},
    "eslav": {"ru", "be", "uk"},
    "cyrillic": {
        "ru",
        "rs_cyrillic",
        "be",
        "bg",
        "uk",
        "mn",
        "abq",
        "ady",
        "kbd",
        "ava",
        "dar",
        "inh",
        "che",
        "lbe",
        "lez",
        "tab",
        "kk",
        "ky",
        "tg",
        "mk",
        "tt",
        "cv",
        "ba",
        "mhr",
        "mo",
        "udm",
        "kv",
        "os",
        "bua",
        "xal",
        "tyv",
        "sah",
        "kaa",
    },
    "devanagari": {
        "hi",
        "mr",
        "ne",
        "bh",
        "mai",
        "ang",
        "bho",
        "mah",
        "sck",
        "new",
        "gom",
        "sa",
        "bgc",
    },
    "specific": {
        "ch",
        "chinese_cht",
        "japan",
        "en",
        "korean",
        "th",
        "el",
        "te",
        "ta",
        "ka",
    },
}


def resolve_model_dirs(
    lang: str, use_server_model: bool, support_files_path
) -> tuple[str, str, str]:
    """Resolves the model directory for the specified language and mode."""
    program_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    base_path = support_files_path or os.path.join(
        program_dir, "PaddleOCR.PP-OCRv5.support.files"
    )

    det_path = os.path.join(base_path, "det")
    rec_path = os.path.join(base_path, "rec")
    cls_path = os.path.join(base_path, "cls", "PP-LCNet_x1_0_textline_ori")

    mode = "server" if use_server_model else "mobile"

    # DET
    if lang == "ka":
        det_sub = "PP-OCRv3_mobile_det"
    else:
        det_sub = f"PP-OCRv5_{mode}_det"

    # REC
    if lang in ("ch", "chinese_cht", "japan"):
        rec_sub = f"PP-OCRv5_{mode}_rec"
    elif lang in PADDLEOCR_LANGS["latin"]:
        rec_sub = "latin_PP-OCRv5_mobile_rec"
    elif lang in PADDLEOCR_LANGS["arabic"]:
        rec_sub = "arabic_PP-OCRv5_mobile_rec"
    elif lang in PADDLEOCR_LANGS["eslav"]:
        rec_sub = "eslav_PP-OCRv5_mobile_rec"
    elif lang in PADDLEOCR_LANGS["cyrillic"]:
        rec_sub = "cyrillic_PP-OCRv5_mobile_rec"
    elif lang in PADDLEOCR_LANGS["devanagari"]:
        rec_sub = "devanagari_PP-OCRv5_mobile_rec"
    elif lang in ("en", "korean", "th", "el", "te", "ta"):
        rec_sub = f"{lang}_PP-OCRv5_mobile_rec"
    elif lang == "ka":
        rec_sub = "ka_PP-OCRv3_mobile_rec"

    return (os.path.join(det_path, det_sub), os.path.join(rec_path, rec_sub), cls_path)
