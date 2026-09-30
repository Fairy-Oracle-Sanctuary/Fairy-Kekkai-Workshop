"""Prepare the pinned upstream source and Silero VAD model; never compile."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
TAG = "v1.9.4"
COMMIT = "927cfce34f31707e17f2bff35c349632fb9e2c3a"
SOURCE = ROOT / "third_party/whisper.cpp"
MODELS = ROOT / "tools/Whisper.model"
VAD_SHA256 = "2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987"


def download(url, target, expected_hash=None):
    if target.exists():
        if expected_hash and hashlib.sha256(target.read_bytes()).hexdigest() != expected_hash:
            raise RuntimeError(f"Existing model checksum mismatch: {target}. File was preserved.")
        print(f"Already exists: {target}")
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + ".part")
    try:
        with urllib.request.urlopen(url, timeout=60) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        # ggml models have a fixed magic; reject HTML/error pages.
        with temporary.open("rb") as model:
            if model.read(4) != b"lmgg":
                raise RuntimeError(f"Invalid ggml model: {target.name}")
        if expected_hash and hashlib.sha256(temporary.read_bytes()).hexdigest() != expected_hash:
            raise RuntimeError(f"Downloaded model checksum mismatch: {target.name}")
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Downloaded: {target}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download-small", action="store_true", help="Also download the multilingual small ASR model (~466 MB)")
    args = parser.parse_args()
    if not SOURCE.exists():
        subprocess.run(["git", "clone", "--depth", "1", "--branch", TAG,
                        "https://github.com/ggml-org/whisper.cpp.git", str(SOURCE)], check=True)
    actual = subprocess.check_output(["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
    if actual != COMMIT:
        raise RuntimeError(f"Existing source is {actual}; expected {COMMIT}. No checkout was changed.")
    download("https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin",
             MODELS / "ggml-silero-v6.2.0.bin", VAD_SHA256)
    if args.download_small:
        download("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin",
                 MODELS / "ggml-small.bin")
    print(f"Source ready: {SOURCE} ({TAG}). Build instructions: cpp/WHISPER.md")


if __name__ == "__main__":
    main()
