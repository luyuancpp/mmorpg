#!/usr/bin/env python3
"""封存当前网关源码，离线测试打包，并生成带源码收据的本地 Java 镜像。"""

from __future__ import annotations

import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
import zipfile


REPO_ROOT = Path(__file__).resolve().parents[2]
GATEWAY_ROOT = REPO_ROOT / "java/gateway_node"
DEFAULT_JDK = Path("D:/luyuan/tools/jdk23/jdk-23.0.2+7")
DEFAULT_MAVEN = Path("D:/luyuan/tools/apache-maven-3.9.9/bin/mvn.cmd")
BREAKPOINT_CLASSES = (
    "com.game.gateway.controller.LoginController",
    "com.game.gateway.controller.ServerListController",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    # 独占创建，失败证据和之前的构建结果都不覆盖。
    with path.open("x", encoding="utf-8", newline="\n") as target:
        json.dump(value, target, ensure_ascii=False, indent=2)
        target.write("\n")


def source_files() -> dict[str, str]:
    candidates = [GATEWAY_ROOT / "pom.xml"]
    candidates.extend(
        path for name in ("mvnw", "mvnw.cmd")
        if (path := GATEWAY_ROOT / name).exists()
    )
    for name in ("src", ".mvn"):
        directory = GATEWAY_ROOT / name
        if not directory.exists():
            if name == "src":
                raise RuntimeError("网关 src 输入目录不存在。")
            continue
        if directory.is_symlink():
            raise RuntimeError("Maven 输入目录不能是符号链接。")
        for path in directory.rglob("*"):
            if path.is_symlink():
                raise RuntimeError("Maven 输入不能包含符号链接。")
            if path.is_file():
                candidates.append(path)
    files: dict[str, str] = {}
    for path in sorted(candidates):
        if path.is_symlink() or not path.is_file():
            raise RuntimeError("Maven 输入缺失或包含符号链接。")
        relative = path.relative_to(REPO_ROOT).as_posix()
        files[relative] = sha256(path)
    if not any(path.endswith(".java") for path in files):
        raise RuntimeError("没有发现网关 Java 源码。")
    return files


def snapshot_files(snapshot: Path) -> dict[str, str]:
    prefix = GATEWAY_ROOT.relative_to(REPO_ROOT)
    return {
        (prefix / path.relative_to(snapshot)).as_posix(): sha256(path)
        for path in sorted(snapshot.rglob("*"))
        if path.is_file() and path.relative_to(snapshot).parts[0] != "target"
    }


def assert_inputs(expected: dict[str, str], snapshot: Path) -> None:
    if source_files() != expected:
        raise RuntimeError("本地 Maven 输入在构建期间变化，拒绝交付混合源码镜像。")
    if snapshot_files(snapshot) != expected:
        raise RuntimeError("封存的 Maven 输入发生变化，拒绝继续。")


def capture(command: list[str], *, timeout: int = 30) -> str:
    completed = subprocess.run(
        command, cwd=REPO_ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=timeout, check=False,
    )
    if completed.returncode:
        # 不把 docker inspect 或其他命令的整个输出带入终端。
        raise RuntimeError(f"只读前置命令失败，退出码 {completed.returncode}。")
    return completed.stdout.strip()


def logged(command: list[str], log: Path, *, cwd: Path,
           env: dict[str, str], timeout: int) -> None:
    with log.open("x", encoding="utf-8", newline="\n") as output:
        process = subprocess.Popen(
            command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT,
        )
        try:
            exit_code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            # PowerShell→mvn.cmd→JVM 是同一构建进程树，只结束本次创建的子进程。
            # 不按 java/docker 等进程名清理，避免影响正在运行的服务器。
            if os.name == "nt":
                subprocess.run(
                    ["taskkill.exe", "/PID", str(process.pid), "/T", "/F"],
                    stdout=output, stderr=subprocess.STDOUT, timeout=30, check=False,
                )
            if process.poll() is None:
                process.kill()
            process.wait(timeout=15)
            raise RuntimeError(f"命令超过 {timeout} 秒预算，参见 {log.name}。") from error
    if exit_code:
        raise RuntimeError(f"命令退出码 {exit_code}，参见 {log.name}。")


def test_summary(snapshot: Path) -> dict[str, int]:
    reports = sorted((snapshot / "target/surefire-reports").glob("TEST-*.xml"))
    cases: dict[tuple[str, str], str] = {}
    duplicates = 0
    for report in reports:
        root = ET.parse(report).getroot()
        for case in root.iter("testcase"):
            key = (case.get("classname", ""), case.get("name", ""))
            status = "passed"
            for candidate in ("error", "failure", "skipped"):
                if case.find(candidate) is not None:
                    status = {"error": "errors", "failure": "failures"}.get(candidate, candidate)
                    break
            if key in cases:
                duplicates += 1
                if cases[key] != status:
                    raise RuntimeError("同一测试在 XML 报告中有冲突结果。")
            cases[key] = status
    summary = dict(reports=len(reports), tests=len(cases), passed=0,
                   failures=0, errors=0, skipped=0, duplicateCases=duplicates)
    for status in cases.values():
        summary[status] += 1
    return summary


def ps_literal(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def maven_command(pwsh: str, maven: Path, version: str, commit: str) -> list[str]:
    # 使用编码后的固定参数调用 .cmd，不把路径或参数拼入 cmd /c。
    arguments = ["-o", "-B", "-T1", "clean", "package", "-DforkCount=1",
                 "-DreuseForks=true", f"-Dmmorpg.build.version={version}",
                 f"-Dmmorpg.build.commit={commit}"]
    script = ("$ErrorActionPreference='Stop'; "
              "$PSNativeCommandUseErrorActionPreference=$false; & "
              + ps_literal(str(maven)) + " "
              + " ".join(ps_literal(argument) for argument in arguments)
              + "; exit $LASTEXITCODE")
    encoded = base64.b64encode(script.encode("utf-16-le")).decode("ascii")
    return [pwsh, "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded]


def check_line_tables(javap: Path, snapshot: Path, jar: Path,
                      output: Path, env: dict[str, str]) -> None:
    classes = snapshot / "target/classes"
    with zipfile.ZipFile(jar) as archive:
        for class_name in BREAKPOINT_CLASSES:
            relative = class_name.replace(".", "/") + ".class"
            expected = (classes / relative).read_bytes()
            if archive.read("BOOT-INF/classes/" + relative) != expected:
                raise RuntimeError("JAR 里的断点类与本次 target/classes 不一致。")
    log = output / "javap.log"
    with log.open("x", encoding="utf-8", newline="\n") as target:
        for class_name in BREAKPOINT_CLASSES:
            completed = subprocess.run(
                [str(javap), "-classpath", str(classes), "-v", "-l", class_name],
                cwd=snapshot, env=env, capture_output=True, text=True,
                encoding="utf-8", errors="replace", timeout=30, check=False,
            )
            target.write(completed.stdout)
            target.write(completed.stderr)
            if completed.returncode or "LineNumberTable:" not in completed.stdout:
                raise RuntimeError("断点类没有可用行号表，参见 javap.log。")
            if 'SourceFile: "' not in completed.stdout:
                raise RuntimeError("断点类没有源码文件属性，参见 javap.log。")


def build(args: argparse.Namespace, output: Path,
          result: dict[str, object], details: dict[str, object]) -> None:
    with args.base_images.resolve().open(encoding="utf-8-sig") as source:
        bases = json.load(source)
    if not isinstance(bases, dict) or not all(
        isinstance(key, str) and isinstance(value, str) and value.strip()
        for key, value in bases.items()
    ) or "gateway" not in bases:
        raise RuntimeError("base-images 必须是包含 gateway 的 service→image JSON 对象。")
    java = args.jdk.resolve() / "bin/java.exe"
    javap = args.jdk.resolve() / "bin/javap.exe"
    maven = args.maven.resolve()
    if not all(path.is_file() for path in (java, javap, maven)):
        raise RuntimeError("已有 JDK23 或 Maven3.9.9 路径不可用；脚本不会安装工具。")
    pwsh = shutil.which("pwsh")
    docker = shutil.which("docker")
    if not pwsh or not docker:
        raise RuntimeError("需要已有 PowerShell7 与 Docker CLI。")
    docker_command = [docker, "--context", args.docker_context]
    base_id = capture(docker_command + ["image", "inspect", "--format", "{{.Id}}", bases["gateway"]])
    if not base_id.startswith("sha256:") or len(base_id) != 71:
        raise RuntimeError("无法解析基础网关镜像的不可变 image ID。")
    base_user = capture(docker_command + ["image", "inspect", "--format", "{{.Config.User}}", base_id])
    base_entrypoint = json.loads(capture(docker_command + [
        "image", "inspect", "--format", "{{json .Config.Entrypoint}}", base_id,
    ]))
    if base_user != "10001:10001" or base_entrypoint != ["java", "-jar", "/app/service.jar"]:
        raise RuntimeError("基础镜像不符合正式网关的非 root 用户或 JVM 入口契约。")
    commit = capture(["git", "rev-parse", "HEAD"])
    dirty = bool(capture(["git", "status", "--porcelain"]))
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ").lower()
    version = f"debug-{stamp}-{uuid.uuid4().hex[:8]}"
    image = f"local/mmorpg-gateway:{version}"
    details.update(baseImage=bases["gateway"], baseImageId=base_id,
                   image=image, commit=commit, dirty=dirty, version=version)

    before = source_files()
    source_hash = hashlib.sha256(json.dumps(before, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    revision = commit + ("-dirty" if dirty else "") + "-source." + source_hash[:12]
    details.update(sourceSHA256=source_hash, revision=revision)
    write_json(output / "source-before.json", before)
    snapshot = output / "snapshot"
    snapshot.mkdir()
    for relative in before:
        source = REPO_ROOT / relative
        target = snapshot / source.relative_to(GATEWAY_ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    assert_inputs(before, snapshot)
    details["sourceCount"] = len(before)
    details["snapshot"] = str(snapshot)

    env = os.environ.copy()
    env["JAVA_HOME"] = str(args.jdk.resolve())
    env["PATH"] = str(java.parent) + os.pathsep + env.get("PATH", "")
    env["MAVEN_OPTS"] = "-Xmx768m -XX:ActiveProcessorCount=1"
    # 同样约束 Surefire 子 JVM；只修改子进程环境，不修改宿主环境。
    env["JAVA_TOOL_OPTIONS"] = "-Xmx768m -XX:ActiveProcessorCount=1"
    env.pop("MAVEN_ARGS", None)
    env.pop("JDK_JAVA_OPTIONS", None)
    command = maven_command(pwsh, maven, version, revision)
    maven_error: Exception | None = None
    try:
        logged(command, output / "maven.log", cwd=snapshot, env=env, timeout=1800)
    except Exception as error:
        maven_error = error
    summary = test_summary(snapshot)
    write_json(output / "test-summary.json", summary)
    details["tests"] = summary
    if maven_error:
        raise maven_error
    if summary["tests"] == 0 or summary["failures"] or summary["errors"]:
        raise RuntimeError("Maven 测试报告为空或包含失败，拒绝打包镜像。")
    assert_inputs(before, snapshot)
    jars = sorted((snapshot / "target").glob("*.jar"))
    if len(jars) != 1:
        raise RuntimeError("本次 Maven 构建必须只产出一个可运行 JAR。")
    binary = output / "binary"
    binary.mkdir()
    jar = binary / "service.jar"
    shutil.copy2(jars[0], jar)
    jar_hash = sha256(jar)
    if jar_hash != sha256(jars[0]):
        raise RuntimeError("交付 JAR 与本次 Maven 产物 SHA256 不同。")
    check_line_tables(javap, snapshot, jar, output, env)

    source_meta = output / "source_meta"
    source_meta.mkdir()
    built_utc = utc_now()
    build_info = source_meta / "BUILD_INFO"
    build_info.write_text(
        f"version={version}\ncommit={revision}\nbuilt_at={built_utc}\nsource_sha256={source_hash}\n",
        encoding="utf-8", newline="\n",
    )
    manifest = source_meta / "source-manifest.json"
    manifest_value = dict(
        schema=1, language="java", sourceRoot=str(REPO_ROOT), files=before,
        artifacts={"/app/service.jar": jar_hash, "/app/BUILD_INFO": sha256(build_info)},
        build=dict(version=version, commit=commit, dirty=dirty,
                   revision=revision, sourceSHA256=source_hash,
                   startedUtc=details["startedUtc"], builtUtc=built_utc,
                   baseImageId=base_id, tests=summary,
                   maven="-o -B -T1 clean package", maxHeapMiB=768),
    )
    write_json(manifest, manifest_value)
    result["manifestPaths"] = {"gateway": str(manifest)}
    recipe = REPO_ROOT / "deploy/debug/Dockerfile.java"
    shutil.copy2(recipe, output / "Dockerfile.java")
    # BuildKit 的 FROM 不接受裸 config ID，独占本地标签只指向已解析的基础镜像。
    pinned = "local/mmorpg-gateway-runtime:pin-" + uuid.uuid4().hex
    capture(docker_command + ["tag", base_id, pinned])
    details["pinnedBaseImage"] = pinned
    if capture(docker_command + ["image", "inspect", "--format", "{{.Id}}", pinned]) != base_id:
        raise RuntimeError("基础镜像的独占固定标签与解析 ID 不符。")
    build_args = docker_command + [
        "build", "--pull=false", "--progress=plain", "--file", str(output / "Dockerfile.java"),
        "--tag", image, "--build-arg", f"GATEWAY_IMAGE={pinned}",
        "--label", f"org.opencontainers.image.version={version}",
        "--label", f"org.opencontainers.image.revision={revision}",
        "--label", f"org.opencontainers.image.created={utc_now()}",
        "--label", "org.opencontainers.image.title=mmorpg-gateway", str(output),
    ]
    # Docker 上下文只保留新 JAR 与收据，避免上传 Maven 日志、源码快照和缓存。
    (output / ".dockerignore").write_text(
        "*\n!binary/\nbinary/*\n!binary/service.jar\n"
        "!source_meta/\nsource_meta/*\n!source_meta/source-manifest.json\n!source_meta/BUILD_INFO\n",
        encoding="utf-8", newline="\n",
    )
    assert_inputs(before, snapshot)
    logged(build_args, output / "image.log", cwd=REPO_ROOT, env=os.environ.copy(), timeout=600)
    if capture(docker_command + ["image", "inspect", "--format", "{{.Id}}", pinned]) != base_id:
        raise RuntimeError("打包期间基础镜像的固定标签发生变化。")
    assert_inputs(before, snapshot)
    if (sha256(jar) != jar_hash
            or sha256(build_info) != manifest_value["artifacts"]["/app/BUILD_INFO"]
            or json.loads(manifest.read_text(encoding="utf-8")) != manifest_value):
        raise RuntimeError("镜像打包期间 JAR 或源码收据发生变化。")
    write_json(output / "source-after.json", source_files())
    image_id = capture(docker_command + ["image", "inspect", "--format", "{{.Id}}", image])
    if not image_id.startswith("sha256:") or len(image_id) != 71:
        raise RuntimeError("无法核对本次网关调试镜像 ID。")
    details.update(jarSHA256=jar_hash, imageId=image_id)
    result["images"] = {"gateway": {"image": image, "imageId": image_id}}
    result["success"] = True


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="必须尚不存在的独占证据目录")
    parser.add_argument("--base-images", required=True, type=Path, help="纯 service→image JSON 文件；必须包含 gateway")
    parser.add_argument("--jdk", type=Path, default=DEFAULT_JDK, help="已有 JDK23 目录")
    parser.add_argument("--maven", type=Path, default=DEFAULT_MAVEN, help="已有 Maven3.9.9 的 mvn.cmd")
    parser.add_argument("--docker-context", default="desktop-linux")
    args = parser.parse_args()
    output = args.output.resolve()
    try:
        output.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        print("输出目录已存在，拒绝覆盖。", file=sys.stderr)
        return 2
    result: dict[str, object] = dict(success=False, images={}, manifestPaths={})
    details: dict[str, object] = dict(startedUtc=utc_now())
    try:
        build(args, output, result, details)
    except Exception as error:
        details["error"] = str(error)
        print(f"Java 网关构建失败；证据位于 {output}。", file=sys.stderr)
    finally:
        details["endedUtc"] = utc_now()
        write_json(output / "build-details.json", details)
        write_json(output / "result.json", result)
    if result["success"]:
        print(f"Java 网关构建成功；结果位于 {output / 'result.json'}。")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
