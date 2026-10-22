#!/usr/bin/env python3
"""核对 C++ Debug 构建输入，并登记同批节点与符号摘要；不执行构建或目标程序。"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import os


DEBUG_PROJECTS = ("cpp/libs/engine/core", "cpp/nodes/gate", "cpp/nodes/scene", "cpp/nodes/battle")


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


def compiler_flags(root):
    evidence = {}
    for project in DEBUG_PROJECTS:
        build = root / project / ".build"
        cache = (build / "CMakeCache.txt").read_text("utf-8")
        actual = re.search(r"^CMAKE_BUILD_TYPE:[^=]+=(.*)$", cache, re.MULTILINE)
        if actual is None or actual.group(1).strip() != "Debug":
            raise ValueError("实际 CMake 构建类型不是 Debug：" + project)
        targets = {}
        for path in sorted(build.glob("CMakeFiles/*/flags.make")):
            match = re.search(r"^CXX_FLAGS\s*=\s*(.*)$", path.read_text("utf-8"), re.MULTILINE)
            if match is None:
                continue
            flags = match.group(1).strip()
            tokens = shlex.split(flags)
            optimization = [word for word in tokens if re.fullmatch(r"-O(?:[0-9]+|s|z|g|fast)?", word)]
            debug = [word for word in tokens if re.fullmatch(r"-g(?:gdb)?[0-3]?", word)]
            if not optimization or optimization[-1] != "-O0" or not debug or debug[-1].endswith("0"):
                raise ValueError("实际 C++ 参数未满足最终 -O0 与调试信息：" + project)
            targets[path.relative_to(build).as_posix()] = flags
        if not targets:
            raise ValueError("缺少实际 CXX_FLAGS 证据：" + project)
        evidence[project] = {"buildType": "Debug", "targets": targets}
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path("/src"))
    parser.add_argument("--complete", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cache-state", type=Path,
                        help="仅在受 SDK SHA 隔离的 .build 缓存内保留成功批次的输入 SHA")
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
    if not isinstance(manifest.get("artifacts", {}), dict):
        raise ValueError("清单 artifacts 必须为产物路径到 SHA256 映射")
    for relative, expected in files.items():
        if not isinstance(relative, str) or not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("源码清单的路径或 SHA256 格式错误")
        if digest(checked_source(root, relative)) != expected:
            raise ValueError("源码与本批清单不符：" + relative)
    if args.cache_state is not None:
        state = args.cache_state.resolve()
        if not state.is_relative_to(root / "third_party/.build"):
            raise ValueError("源码缓存状态必须位于 third_party/.build 内")
        if not args.complete:
            previous = json.loads(state.read_text("utf-8")) if state.is_file() else {}
            previous_files = previous.get("files", {}) if previous.get("schema") == 1 else {}
            # copy2 保留未变输入的时间；内容变化即使带旧时间，也必须使 make 重新编译。
            for relative, expected in files.items():
                if previous_files.get(relative) != expected:
                    checked_source(root, relative).touch()
    if args.complete:
        if args.output is None:
            raise ValueError("登记产物时必须显式指定 --output")
        manifest["build"]["compilerFlags"] = compiler_flags(root)
        artifacts = {}
        for node in ("gate", "scene", "battle"):
            for relative, runtime in (("bin/" + node, "/app/bin/" + node),
                                      ("bin/symbols/" + node + ".debug", "/app/bin/" + node + ".debug")):
                path = checked_source(root, relative)
                if path.stat().st_size == 0:
                    raise ValueError("构建产物为空：" + relative)
                artifacts[runtime] = digest(path)
        manifest["artifacts"] = artifacts
        args.output.write_text(json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        if args.cache_state is not None:
            temporary = state.with_name(state.name + ".tmp")
            temporary.write_text(json.dumps({"schema": 1, "files": files}, sort_keys=True) + "\n", encoding="utf-8")
            os.replace(temporary, state)
    print(json.dumps({"files": len(files), "artifacts": len(manifest.get("artifacts", {})),
                      "buildType": "Debug", "complete": args.complete}, ensure_ascii=False))


if __name__ == "__main__":
    main()
