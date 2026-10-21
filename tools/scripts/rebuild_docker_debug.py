"""本机 Docker 调试入口：构建成功后更新指定服务，附加前核对源码。"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


REPO = Path(__file__).resolve().parents[2]
STATE = REPO / "build/docker-debug-current"
CPP = ("gate", "scene", "battle")
GO = ("login", "db", "data-service", "scene-manager", "player-locator", "client-rpc-router", "match")
IMAGE_KEYS = {
    **dict.fromkeys(CPP, "NODE_DEBUG_IMAGE"),
    "login": "LOGIN_IMAGE", "db": "DB_IMAGE", "data-service": "DATA_SERVICE_IMAGE",
    "scene-manager": "SCENE_MANAGER_IMAGE", "player-locator": "PLAYER_LOCATOR_IMAGE",
    "client-rpc-router": "CLIENT_RPC_ROUTER_IMAGE", "match": "MATCH_IMAGE", "gateway": "GATEWAY_IMAGE",
}
DOCKER = ["docker", "--context", "desktop-linux"]
COMPOSE = DOCKER + ["compose", "--env-file", str(STATE / ".env"),
                    "-f", str(REPO / "deploy/debug/compose.game.yml"),
                    "-f", str(REPO / "deploy/debug/compose.debug.yml")]


def execute(args: list[str], *, capture: bool = True, timeout: int = 90) -> str:
    result = subprocess.run(args, cwd=REPO, capture_output=capture, text=True,
                            encoding="utf-8", errors="replace", timeout=timeout)
    if result.returncode:
        # Compose 展开内容含密钥；失败时只报告命令类型，不打印捕获的数据。
        raise RuntimeError(f"{Path(args[0]).name} 执行失败，退出码 {result.returncode}")
    return result.stdout if capture else ""


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def compose_config() -> dict:
    return json.loads(execute(COMPOSE + ["config", "--format", "json"]))


def check(services: list[str], output: Path | None = None) -> bool:
    success = True
    receipts = []
    for service in services:
        record = {"service": service, "success": False, "differences": []}
        try:
            ids = execute(COMPOSE + ["ps", "--all", "-q", service]).splitlines()
            if len(ids) != 1:
                raise RuntimeError("服务容器不存在或数量不为一")
            container = ids[0].strip()
            identity = json.loads(execute(DOCKER + ["inspect", "--format",
                '{{json .}}', container]))
            if not identity["State"]["Running"]:
                raise RuntimeError("服务容器未运行")
            record["imageId"] = identity["Image"]
            raw = execute(DOCKER + ["exec", container, "cat", "/app/DEBUG_SOURCE.json"])
            manifest = json.loads(raw)
            if manifest.get("schema") != 1 or not isinstance(manifest.get("files"), dict) or not manifest["files"]:
                raise RuntimeError("镜像源码清单缺失或不完整，请先重新构建该服务")
            for name, expected in manifest["files"].items():
                relative = PurePosixPath(name)
                if relative.is_absolute() or ".." in relative.parts or "\\" in name:
                    raise RuntimeError("镜像源码清单路径非法")
                path = REPO.joinpath(*relative.parts)
                if not path.resolve().is_relative_to(REPO):
                    raise RuntimeError("本地源码路径越界")
                if not path.is_file() or digest(path) != expected:
                    record["differences"].append(name)
            artifacts = manifest.get("artifacts", {})
            if not artifacts or any(not p.startswith("/app/") and p != "/usr/local/bin/dlv" for p in artifacts):
                raise RuntimeError("镜像产物摘要缺失或路径非法")
            hashes = execute(DOCKER + ["exec", container, "sha256sum", "--", *artifacts]).splitlines()
            actual = {line[66:]: line[:64] for line in hashes if len(line) > 66}
            if actual != artifacts:
                raise RuntimeError("容器二进制或调试符号已偏离构建清单")
            record["sourceCount"] = len(manifest["files"])
            record["success"] = not record["differences"]
            print(f"{service}：" + (f"源码与产物一致（{record['sourceCount']} 文件）" if record["success"]
                else f"{len(record['differences'])} 份源码已变化，须重新构建"))
            for name in record["differences"][:8]:
                print("  " + name)
        except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as error:
            record["error"] = str(error)
            print(f"{service}：检查未通过，{error}")
        success = success and record["success"]
        receipts.append(record)
    if output is not None:
        output.write_text(json.dumps({"success": success, "services": receipts}, ensure_ascii=False, indent=2), encoding="utf-8")
    return success


def update_images(images: dict) -> None:
    path = STATE / ".env"
    original = path.read_bytes()
    text = original.decode("utf-8-sig")
    replacements = {}
    for service, image in images.items():
        if service not in IMAGE_KEYS or not re.fullmatch(r"local/[a-z0-9._/-]+:[a-z0-9._-]+", image["image"]):
            raise RuntimeError("构建结果包含非法镜像引用")
        actual = execute(DOCKER + ["image", "inspect", "--format", "{{.Id}}", image["image"]]).strip()
        if actual != image["imageId"]:
            raise RuntimeError("本地镜像标签与构建结果不同，未更新配置")
        key = IMAGE_KEYS[service]
        if key in replacements and replacements[key] != image["image"]:
            raise RuntimeError("C++ 节点必须来自同一镜像")
        replacements[key] = image["image"]
    for key, image in replacements.items():
        pattern = re.compile(r"^" + re.escape(key) + r"=.*$", re.MULTILINE)
        if len(pattern.findall(text)) != 1:
            raise RuntimeError("配置镜像键缺失或重复，未更新配置：" + key)
        text = pattern.sub(key + "=" + image, text)
    if path.read_bytes() != original:
        raise RuntimeError("开发配置被其他任务更新，未覆盖配置")
    temporary = path.with_name(".env.debug-update.tmp")
    with temporary.open("x", encoding="utf-8", newline="") as stream:
        stream.write(text)
    os.replace(temporary, path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("language", choices=("cpp", "go", "java", "check"))
    parser.add_argument("--services", help="逗号分隔；C++ 默认三个节点，Go 默认 login，Java 默认 gateway")
    parser.add_argument("--jobs", type=int, default=1, choices=range(1, 5))
    parser.add_argument("--build-only", action="store_true", help="仅构建，不修改配置或更新容器")
    args = parser.parse_args()
    defaults = {"cpp": CPP, "go": ("login",), "java": ("gateway",), "check": ("scene",)}
    services = args.services.split(",") if args.services else list(defaults[args.language])
    allowed = {"cpp": CPP, "go": GO, "java": ("gateway",), "check": tuple(IMAGE_KEYS)}
    if len(set(services)) != len(services) or not services or any(s not in allowed[args.language] for s in services):
        parser.error("服务名不属于选择的语言，或包含重复项")
    if args.language == "cpp" and set(services) != set(CPP):
        parser.error("C++ 共享库统一重编后须一起更新 gate,scene,battle")
    if args.language == "check":
        return 0 if check(services) else 1
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    root = REPO / "build/verification" / ("docker-rebuild-" + args.language + "-" + stamp)
    root.mkdir(parents=True, exist_ok=False)
    config = compose_config()
    bases = {s: config["services"][s]["image"] for s in services}
    base_file = root / "base-images.json"
    base_file.write_text(json.dumps(bases, indent=2), encoding="utf-8")
    command = [sys.executable, str(REPO / f"tools/scripts/build_docker_{args.language}_debug.py"),
               "--output", str(root / "build"), "--base-images", str(base_file)]
    if args.language != "java":
        command += ["--services", ",".join(services), "--jobs", str(args.jobs)]
    print("本轮证据：" + str(root), flush=True)
    # 子构建器自行保存日志；失败立即返回，不替换正在运行的服务。
    result = subprocess.run(command, cwd=REPO)
    if result.returncode:
        print("构建失败，现有容器未更新；请检查本轮 build 目录。")
        return result.returncode
    build = json.loads((root / "build/result.json").read_text("utf-8-sig"))
    if not build.get("success") or set(build.get("images", {})) != set(services):
        raise RuntimeError("构建结果不完整，未更新容器")
    if args.build_only:
        print("构建完成；按要求未修改配置或更新容器。")
        return 0
    update_images(build["images"])
    with (root / "recreate.log").open("x", encoding="utf-8") as log:
        result = subprocess.run(COMPOSE + ["up", "-d", "--no-build", "--pull", "never", "--no-deps",
            "--force-recreate", "--wait", "--wait-timeout", "180", *services], cwd=REPO,
            stdout=log, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    if result.returncode:
        print("容器更新失败，配置与现场已保留；检查 " + str(root / "recreate.log"))
        return result.returncode
    if not check(services, root / "verification.json"):
        return 1
    print("构建、容器更新及源码检查完成，可以在 VS Code 按 F5 附加。")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print("已停止：" + str(error), file=sys.stderr)
        raise SystemExit(1)
