# Docker & K8s Build/Deploy Guide (Windows)

**Updated:** 2026-03-27

## Overview

三类 Docker 镜像，均可在 Windows 上通过 Docker Desktop（Linux containers 模式）构建。

| 类型 | Dockerfile | Build Context | 镜像名 |
|------|-----------|---------------|--------|
| C++ 节点 (gate+scene) | `deploy/k8s/Dockerfile.cpp` | 仓库根目录 | `mmorpg-node` |
| Go 服务 (5个) | `deploy/k8s/Dockerfile.go-svc` | `go/` | `mmorpg-{service}` |
| Java Auth | `deploy/k8s/Dockerfile.java-svc` | `java/sa_token_node/` | `mmorpg-auth` |

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
