# coding: utf-8
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent
sys.path.insert(0, str(PROJECT_ROOT))
from app.common.setting import VERSION  # noqa: E402

BASE_DIST_DIR = PROJECT_ROOT / "dist" / "Fairy-Kekkai-Workshop.dist"
EXE_PATH = BASE_DIST_DIR / "Fairy-Kekkai-Workshop.exe"
PADDLEOCR_FILE = PROJECT_ROOT / "PADDLEOCR"
TOOLS_DIR = PROJECT_ROOT / "tools"
ISS_TEMPLATE = PROJECT_ROOT / "Fairy-Kekkai-Workshop.iss"
TEMP_DIR = PROJECT_ROOT / "dist" / "_release_temp"

# (OCR 文件夹名, 安装包标签)
OCR_VERSIONS = [
    ("PaddleOCR-CPU-v1.5.1", "CPU-v1.5.1"),
    ("PaddleOCR-GPU-v1.5.1-CUDA-11.8", "GPU-v1.5.1-CUDA-11.8"),
    ("PaddleOCR-GPU-v1.5.1-CUDA-12.9", "GPU-v1.5.1-CUDA-12.9"),
]

# tools 目录下每次都排除的文件夹（只单独复制当前版本 OCR）
EXCLUDE_DIRS = {
    "PaddleOCR-CPU-v1.5.1",
    "PaddleOCR-GPU-v1.5.1-CUDA-11.8",
    "PaddleOCR-GPU-v1.5.1-CUDA-12.9",
    "Whisper.model",
}


def get_dir_size_str(path: Path) -> str:
    """获取目录大小并格式化为人类可读字符串"""
    total = 0
    for f in path.rglob("*"):
        if f.is_file():
            total += f.stat().st_size
    if total >= 1024**3:
        return f"{total / 1024**3:.1f} GB"
    elif total >= 1024**2:
        return f"{total / 1024**2:.1f} MB"
    elif total >= 1024:
        return f"{total / 1024:.1f} KB"
    return f"{total} B"


def make_paddleocr_content(ocr_dir_name: str) -> str:
    """生成当前版本的 PADDLEOCR 文件内容"""
    lines = PADDLEOCR_FILE.read_text(encoding="utf-8").splitlines()
    if lines:
        lines[0] = ocr_dir_name
    else:
        lines = [ocr_dir_name]
    return "\n".join(lines) + "\n"


def generate_iss(ocr_tag: str, dist_dir: Path) -> Path:
    """从模板生成独立 iss 文件，返回路径"""
    content = ISS_TEMPLATE.read_text(encoding="utf-8")
    content = re.sub(
        r'#define MyAppVersion "[^"]*"',
        f'#define MyAppVersion "{VERSION}"',
        content,
    )
    output_name = f"Fairy-Kekkai-Workshop-v{VERSION}-{ocr_tag}-Windows-x86_64-Setup"
    content = re.sub(
        r"OutputBaseFilename=.*",
        f"OutputBaseFilename={output_name}",
        content,
    )
    # 将 dist Source 路径替换为当前版本的独立 dist 目录
    dist_dir_escaped = str(dist_dir).replace("\\", "\\\\")
    content = re.sub(
        r'Source: "D:\\CODE\\Fairy-Kekkai-Workshop\\dist\\Fairy-Kekkai-Workshop\.dist\\\*"',
        f'Source: "{dist_dir_escaped}\\*"',
        content,
    )
    iss_path = TEMP_DIR / f"Fairy-Kekkai-Workshop-{ocr_tag}.iss"
    iss_path.write_text(content, encoding="utf-8")
    return iss_path


def prepare_version(
    ocr_dir_name: str, ocr_tag: str, index: int, total: int
) -> tuple[Path, Path, str]:
    """为一个版本准备独立的 dist 目录和 iss 文件，返回 (iss_path, dist_dir, output_name)"""
    prefix = f"[{index}/{total}] [{ocr_tag}]"
    print(f"{prefix} 开始准备...")

    version_dir = TEMP_DIR / ocr_tag
    if version_dir.exists():
        shutil.rmtree(version_dir)
    version_dir.mkdir(parents=True, exist_ok=True)

    # 复制整个 base dist 目录
    dist_dir = version_dir / "Fairy-Kekkai-Workshop.dist"
    print(f"{prefix} 复制 dist 目录...")
    shutil.copytree(BASE_DIST_DIR, dist_dir, dirs_exist_ok=True)
    print(f"{prefix} dist 目录复制完成 ({get_dir_size_str(dist_dir)})")

    # 写入当前版本的 PADDLEOCR 文件
    (dist_dir / "PADDLEOCR").write_text(
        make_paddleocr_content(ocr_dir_name), encoding="utf-8"
    )
    print(f"{prefix} 已写入 PADDLEOCR: {ocr_dir_name}")

    # 确保 AppData/config.json 存在
    appdata_dir = dist_dir / "AppData"
    appdata_dir.mkdir(exist_ok=True)
    (appdata_dir / "config.json").write_text("{}", encoding="utf-8")

    # 清理并重建 tools 目录
    dest_tools = dist_dir / "tools"
    if dest_tools.exists():
        shutil.rmtree(dest_tools)
    dest_tools.mkdir(parents=True, exist_ok=True)

    # 复制 tools（排除三个 OCR 和 Whisper.model）
    print(f"{prefix} 复制 tools（排除 OCR 和 Whisper.model）...")
    for item in TOOLS_DIR.iterdir():
        if item.name in EXCLUDE_DIRS:
            continue
        dest = dest_tools / item.name
        if item.is_dir():
            shutil.copytree(item, dest, dirs_exist_ok=True)
        else:
            shutil.copy2(item, dest)

    # 只复制当前版本的 OCR 文件夹
    ocr_src = TOOLS_DIR / ocr_dir_name
    if ocr_src.exists():
        shutil.copytree(ocr_src, dest_tools / ocr_dir_name, dirs_exist_ok=True)
        print(f"{prefix} 已复制 OCR: tools/{ocr_dir_name}")
    else:
        print(f"{prefix} 警告: 未找到 OCR 目录 {ocr_src}")

    print(f"{prefix} tools 复制完成 ({get_dir_size_str(dest_tools)})")

    # 生成独立 iss
    iss_path = generate_iss(ocr_tag, dist_dir)
    output_name = f"Fairy-Kekkai-Workshop-v{VERSION}-{ocr_tag}-Windows-x86_64-Setup"
    print(f"{prefix} iss 文件已生成: {iss_path.name}")
    print(f"{prefix} 准备完成 ✓\n")

    return iss_path, dist_dir, output_name


def run_iscc(ocr_tag: str, iss_path: Path, output_name: str) -> tuple[str, bool]:
    """调用 Inno Setup 编译器，实时显示输出"""
    print(f"  [{ocr_tag}] ISCC 开始编译...")
    result = subprocess.run(
        ["ISCC.exe", str(iss_path)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    # 打印 ISCC 输出，每行加标签前缀
    output = result.stdout or ""
    for line in output.strip().splitlines():
        print(f"  [{ocr_tag}] {line}")
    if result.returncode != 0:
        stderr = result.stderr or ""
        for line in stderr.strip().splitlines():
            print(f"  [{ocr_tag}] {line}")
        print(f"  [{ocr_tag}] ISCC 编译失败 (返回码: {result.returncode})")
        return ocr_tag, False
    print(f"  [{ocr_tag}] 编译完成 ✓: {output_name}.exe")
    return ocr_tag, True


def main():
    if sys.platform != "win32":
        print("此脚本仅支持 Windows 系统")
        sys.exit(1)

    if not EXE_PATH.exists():
        print(f"错误: 未找到 {EXE_PATH}")
        sys.exit(1)

    print(f"检测到 {EXE_PATH.name}，开始并行打包三个版本...\n")

    # 将 exe 复制到桌面（iss 中 Source 指向桌面路径，三个版本共用）
    desktop = Path.home() / "Desktop"
    if desktop.exists():
        shutil.copy2(EXE_PATH, desktop / "Fairy-Kekkai-Workshop.exe")
        print("已复制 exe 到桌面")
        # 删除 dist 中的 exe，避免被打包进安装包
        EXE_PATH.unlink(missing_ok=True)
        print("已删除 dist 中的 exe\n")
    else:
        print(f"警告: 未找到桌面目录 {desktop}\n")

    # 准备临时目录
    if TEMP_DIR.exists():
        shutil.rmtree(TEMP_DIR)
    TEMP_DIR.mkdir(parents=True, exist_ok=True)

    # 串行准备三个版本的独立目录（磁盘 IO 密集，串行更快）
    tasks = []
    total = len(OCR_VERSIONS)
    for i, (ocr_dir_name, ocr_tag) in enumerate(OCR_VERSIONS, 1):
        iss_path, dist_dir, output_name = prepare_version(
            ocr_dir_name, ocr_tag, i, total
        )
        tasks.append((ocr_tag, iss_path, dist_dir, output_name))

    print(f"{'=' * 60}")
    print(f"开始并行编译 {len(tasks)} 个版本...")
    print(f"{'=' * 60}\n")

    # 并行调用 ISCC
    success = []
    failed = []
    completed_count = 0
    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = {
            executor.submit(run_iscc, ocr_tag, iss_path, output_name): ocr_tag
            for ocr_tag, iss_path, _, output_name in tasks
        }
        for future in as_completed(futures):
            ocr_tag, ok = future.result()
            completed_count += 1
            print(f"\n  进度: {completed_count}/{len(tasks)} 完成\n")
            if ok:
                success.append(ocr_tag)
            else:
                failed.append(ocr_tag)

    # 清理临时目录
    print("\n清理临时文件...")
    if TEMP_DIR.exists():
        shutil.rmtree(TEMP_DIR, ignore_errors=True)
        print("已清理临时目录")

    print(f"\n{'=' * 60}")
    print(f"打包完成！成功 {len(success)} 个，失败 {len(failed)} 个")
    for tag in success:
        print(f"  ✓ {tag}")
    for tag in failed:
        print(f"  ✗ {tag}")

    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
