#!/usr/bin/env python3
"""核对 C++ Debug 构建输入，并登记同批节点与符号摘要；不执行构建或目标程序。"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def checked_source(root, relative):
    name = PurePosixPath(relative)
    if not relative or "\\" in relative or name.is_absolute() or ".." in name.parts:
        raise ValueError("源码清单含非法相对路径：" + relative)
    path = root.joinpath(*name.parts)
    if not path.resolve().is_relative_to(root) or not path.is_file():
        raise ValueError("源码清单文件缺失或超出源码根目录：" + relative)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path("/src"))
    parser.add_argument("--complete", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = json.loads(args.manifest.read_text("utf-8-sig"))
    if (not isinstance(manifest, dict) or manifest.get("schema") != 1
            or manifest.get("language") != "cpp" or manifest.get("sourceRoot") != "/src"
            or not isinstance(manifest.get("build"), dict)
            or manifest["build"].get("type") != "Debug"):
        raise ValueError("清单必须为 schema=1、language=cpp、sourceRoot=/src、build.type=Debug")
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError("源码清单 files 必须为非空的相对路径到 SHA256 映射")
    for relative, expected in files.items():
        if not isinstance(relative, str) or not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("源码清单的路径或 SHA256 格式错误")
        if digest(checked_source(root, relative)) != expected:
            raise ValueError("源码与本批清单不符：" + relative)
    if args.complete:
        if args.output is None:
            raise ValueError("登记产物时必须显式指定 --output")
        artifacts = {}
        for node in ("gate", "scene", "battle"):
            artifacts["/app/bin/" + node] = digest(root / "bin" / node)
            artifacts["/app/bin/" + node + ".debug"] = digest(root / "bin/symbols" / (node + ".debug"))
        manifest["artifacts"] = artifacts
        args.output.write_text(json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"files": len(files), "artifacts": len(manifest.get("artifacts", {})),
                      "buildType": "Debug", "complete": args.complete}, ensure_ascii=False))


if __name__ == "__main__":
    main()
