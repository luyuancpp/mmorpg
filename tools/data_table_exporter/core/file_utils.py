"""File I/O utilities: write, MD5-based copy, directory helpers."""

from __future__ import annotations

import hashlib
import logging
import os
import shutil
from pathlib import Path

logger = logging.getLogger(__name__)


def write_file(path: Path | str, content: str) -> None:
    """Write *content* to *path*, creating parent directories as needed."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content)


def write_file_bytes(path: Path | str, data: bytes) -> None:
    """Write binary *data* to *path*, creating parent directories as needed."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def md5_hash(file_path: Path | str, block_size: int = 2 ** 20) -> str | None:
    """Return the hex MD5 digest of a file, or ``None`` on error."""
    try:
        md5 = hashlib.md5()
        with open(file_path, "rb") as f:
            while chunk := f.read(block_size):
                md5.update(chunk)
        return md5.hexdigest()
    except Exception as exc:
        logger.error("MD5 error for %s: %s", file_path, exc)
        return None


def md5_copy_file(src: Path, dst: Path) -> None:
    """Copy *src* → *dst* only when content differs (by MD5)."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and md5_hash(src) == md5_hash(dst):
        return
    shutil.copyfile(str(src), str(dst))
    logger.debug("Copied %s → %s", src, dst)


def md5_copy_dir(src_dir: Path, dst_dir: Path) -> None:
    """Recursively copy *src_dir* → *dst_dir*, skipping unchanged files."""
    src_dir = Path(src_dir)
    dst_dir = Path(dst_dir)
    if not src_dir.exists():
        return
    dst_dir.mkdir(parents=True, exist_ok=True)
    for item in src_dir.iterdir():
        dst_item = dst_dir / item.name
        if item.is_dir():
            md5_copy_dir(item, dst_item)
        elif item.is_file():
            md5_copy_file(item, dst_item)


def md5_copy(src: str | Path, dst: str | Path) -> None:
    """Smart dispatcher: copy file or directory with MD5 change detection."""
    src, dst = Path(src), Path(dst)
    if src.is_file():
        md5_copy_file(src, dst)
    elif src.is_dir():
        md5_copy_dir(src, dst)
    else:
        logger.warning("md5_copy: source does not exist: %s", src)


def ensure_dirs(*dirs: Path | str) -> None:
    """Create directories if they do not already exist."""
    for d in dirs:
        Path(d).mkdir(parents=True, exist_ok=True)


def list_xlsx(directory: Path) -> list[Path]:
    """Return all *.xlsx* files directly under *directory*."""
    if not directory.exists():
        return []
    return sorted(f for f in directory.iterdir() if f.is_file() and f.suffix == ".xlsx")
