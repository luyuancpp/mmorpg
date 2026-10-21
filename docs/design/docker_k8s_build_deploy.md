# Docker & K8s Build/Deploy Guide (Windows)

**Updated:** 2026-03-27

## Overview

三类 Docker 镜像，均可在 Windows 上通过 Docker Desktop（Linux containers 模式）构建。

| 类型 | Dockerfile | Build Context | 镜像名 |
|------|-----------|---------------|--------|
| C++ 节点 (gate+scene) | `deploy/k8s/Dockerfile.cpp` | 仓库根目录 | `mmorpg-node` |
| Go 服务 (5个) | `deploy/k8s/Dockerfile.go-svc` | `go/` | `mmorpg-{service}` |
| Java Gateway | `deploy/k8s/Dockerfile.java-svc` | `java/gateway_node/` | `mmorpg-gateway` |

## C++ 节点镜像 — 多阶段构建

```
Stage 1 (deps)    : gcc:13 + setup_dependencies.sh
                    → 编译 gRPC, muduo, hiredis, librdkafka, yaml-cpp, zlib
Stage 2 (builder) : vcxproj2cmake.py → build_linux.sh --skip-deps --release
                    → 编译 14 个静态库 + gate/scene 可执行文件
Stage 3 (runtime) : ubuntu:24.04
                    → 仅复制 gate/scene 二进制 + 数据表 + zoneinfo
```

首次构建约 30–60 分钟（gRPC 从源码编译）。后续构建利用 Docker 层缓存，只要 `third_party/` 没变就不会重编依赖。

## 完整命令

### 前提条件

- 安装 Docker Desktop，开启 Linux containers 模式
- 安装 `kubectl`，配置好集群连接
- （可选）本地基础设施：`cd deploy; docker compose up -d`

### 构建镜像

```powershell
# C++ 节点 (多阶段编译，无需 Linux 环境)
docker build -f deploy/k8s/Dockerfile.cpp -t ghcr.io/luyuancpp/mmorpg-node:v1 .

# Go 服务 (5个)
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1

# Java Auth
pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-build-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### 推送镜像

```powershell
docker push ghcr.io/luyuancpp/mmorpg-node:v1

pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-push-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1

pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-push-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### 部署到 K8s

```powershell
# 开单区
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1 `
  -WaitReady

# 批量开区
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1 `
  -WaitReady
```

### 关区（危险操作，删除整个 namespace）

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday
```

## 替代方案：预编译二进制

如果 C++ 已在 Linux 主机/WSL 上编译好，可以跳过多阶段构建，直接使用轻量 `Dockerfile.runtime`：

```powershell
# 1. 暂存二进制到 staging 目录
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-stage-runtime `
  -BinarySourceRoot D:/linux-build/bin `
  -ZoneInfoSource bin/zoneinfo `
  -TableSource generated/tables

# 2. 构建 runtime-only 镜像（秒级，不编译）
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
```

## 关键文件

| 文件 | 用途 |
|------|------|
| `.dockerignore` | 排除 bin/、.git、docs 等减小构建上下文 |
| `deploy/k8s/Dockerfile.cpp` | 多阶段 C++ 编译镜像 |
| `deploy/k8s/Dockerfile.runtime` | 轻量运行镜像（需预编译二进制） |
| `deploy/k8s/Dockerfile.go-svc` | Go 服务多阶段镜像 |
| `deploy/k8s/Dockerfile.java-svc` | Java Auth 多阶段镜像 |
| `tools/scripts/k8s_image.ps1` | 镜像 preflight / build / push |
| `tools/scripts/k8s_stage_runtime.ps1` | 暂存 Linux 二进制 |
| `tools/scripts/k8s_deploy.ps1` | zone-up / zone-down / zone-status |
| `deploy/k8s/zones.ops-recommended.yaml` | 推荐开区配置 |

## 注意事项

- `centre` 节点已废弃，已从 preflight/staging 检查中移除
- K8s 基础设施 (mysql/redis/kafka/etcd) 使用 `emptyDir`，仅限开发，生产需配 PVC
- Gate 暴露方式：托管云 → `LoadBalancer`；自建裸金属 → `NodePort` + 外部 L4
- 每个 zone = 1 个 K8s namespace (`mmorpg-zone-{name}`)

## 本机 Docker Desktop 与 VS Code 断点（2026-10-20）

本机完整开发栈入口为 `deploy/debug/compose.game.yml`，调试覆盖为同目录 `compose.debug.yml`。它包含 15 个常驻服务与 3 个初始化任务，使用独立网络及持久卷；不复用已有容器或历史演示数据。只启用登录、建角、主世界进场和战斗所需的服务。

在仓库根准备配置：

```powershell
python tools/scripts/prepare_docker_debug.py
docker --context desktop-linux compose --env-file build/docker-debug-current/.env -f deploy/debug/compose.game.yml -f deploy/debug/compose.debug.yml config --quiet
```

准备程序从当前十份源配置派生容器配置，随机密钥仅写入被 Git 忽略的 `build/docker-debug-current/`，再次执行保留原密钥。不要打印、上传或提交展开后的 Compose、`.env` 和带口令的派生配置。

C++ 调试镜像通过 `deploy/debug/Dockerfile` 继承同一次构建的运行镜像与 builder 镜像，增加容器内 GDB，并把对应的分离符号放到 `/app/bin/`。三个节点通过 `SYS_PTRACE` 支持附加，不开放 SSH 或调试 TCP 端口。本节启停命令使用本轮已构建的 `local/*:debug-20261020` 镜像，不会采用上文的注册表 `v1` 镜像。各服务镜像成功构建后，在仓库根执行：

```powershell
$dc = @('--context', 'desktop-linux', 'compose',
  '--env-file', 'build/docker-debug-current/.env',
  '-f', 'deploy/debug/compose.game.yml',
  '-f', 'deploy/debug/compose.debug.yml')
docker @dc up -d --no-build --pull never --wait --wait-timeout 300
docker @dc ps --all --format json |
  ConvertFrom-Json | Select-Object Service,State,Health,ExitCode

# 日常停止，保留本轮数据卷；下次使用同一条 up 命令恢复。
docker @dc stop
```

本轮配置已经准备好，日常启停不必重跑准备程序。修改 C++ 源码后须重新构建镜像并重建对应容器，再附加调试；仅重启原容器不会更新二进制。

本机客户端默认 HTTP 入口为 `http://127.0.0.1:8081`，同一网关保留 `http://127.0.0.1:18081` 供本轮已有工具使用（该附加端口可用 `GATEWAY_PORT` 调整）。`GET /api/server-list` 正常返回 `zones` 数组；本轮可选区服为 `zone-1`。如果客户端未设置过网关地址，它使用 `8081`，因此 Docker 必须映射这个端口。配置准备程序将 `8081` 保留给 HTTP，Gate、Battle 不可使用该端口。

Gate 入口为 `127.0.0.1:28000`、Battle 为 `127.0.0.1:28010`，通过 `CLIENT_ENDPOINT_*` 发布给客户端；内部 `NODE_IP` 仍为容器地址。Scene 与 gRPC 不发布到宿主机。

本栈启用受限的开发口令认证，账号须以 `dev_` 或 `robot_` 开头，密码来自本轮 `.env` 的 `LOGIN_DEV_PASSWORD_SHARED_SECRET`。本机已验证的账号为 `dev_demo`；完整登录信息保存在忽略目录 `build/docker-debug-current/client-login-docker.txt`，密码仅在本机文件中提供。HTTP 登录须同时检查响应业务码为 `0` 且令牌存在；HTTP 200 本身也可能携带认证拒绝。新账号尚无角色属于正常结果，进入游戏还需后续选角／建角与进场。

调试多个新账号时，分别使用 `dev_test_001`、`dev_test_002` 或 `robot_000001` 等不同账号名，密码仍取同一份本机登录信息文件，无需为每个账号修改容器配置。登录后按各自账号的角色列表选角或建角；多客户端测试时使用不同账号名。

本机 `.vscode/launch.json` 提供「Docker：附加 Gate / Scene / Battle」：VS Code 的 C/C++ 扩展通过 `docker --context desktop-linux exec` 调用容器内 GDB，`sourceFileMap` 把编译路径 `/src` 映射为当前工作区。用 VS Code 打开本仓库根目录，在自己的 C++ 源码设置断点，从「运行和调试」下拉选择「Docker：附加 Scene」，按 `F5` 后选择 `scene`（PID 为 `1`）。Gate、Battle 选择各自的入口与同名进程。源码必须与镜像构建输入一致；当前 `RelWithDebInfo` 有优化，部分局部变量可能不可见。长时间断点仍会触发正常的分布式超时与租约处理。

**本轮验证状态（已完成）**：Gate、Scene、Battle 的 Linux `RelWithDebInfo` 构建、运行镜像与调试镜像均成功；镜像内 1,456 个 C++ 源文件与本地逐一 SHA256 匹配。七个 Go 服务及 DB 迁移工具共八个 Linux 镜像、Java 网关镜像均已构建。最终调试配置下 15 个常驻服务运行，11 个带健康检查的服务均为 healthy，3 个初始化任务正常退出；网关健康接口为 UP，HTTP 登录返回业务码 0。

三个节点通过与 VS Code 相同的 Docker/GDB MI 管道完成附加和解除附加；Scene 实际命中本仓 `cpp/libs/engine/core/node/system/node/node.cpp:159`，随后恢复运行，三个节点的 `TracerPid` 均为 0。切换到最终调试镜像后，双玩家功能冒烟再次退出 0，登录、进场、战斗与观战通过，战斗直连和观战直连各收到 9 个回合结果。本轮验证了 GDB 管道与真实源码断点，没有操作 VS Code 界面按 F5。

负责人已授权补齐 `data_service` 的分页围栏接口；对账包 73 项测试、`go vet`、真实 Redis 的 6 项围栏测试通过且无跳过。Java 网关 Maven 测试 205 项通过、5 项数据库条件测试跳过、无失败。首轮构建失败及本机 gRPC 精确快照适配原件均保留；负责人收紧范围后未再运行或排查第三方测试。证据位于 `build/verification/docker-debug-20261020-052903/`，最终结果见 `cpp-runtime/package-result.json`、`cpp-debug-up-result.json` 和 `smoke/` 中的源码匹配、GDB、功能冒烟记录。
