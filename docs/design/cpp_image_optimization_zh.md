# C++ Docker 镜像优化 & 分离调试符号

## 问题

| 镜像 | 优化前 | 优化后 |
|------|--------|--------|
| C++ 节点 (gate+scene) | **1.33 GB** | **194 MB** |
| Go 服务 | ~64 MB | — |

1.33 GB 的原因：
- 未 strip 的二进制：gate = 174 MB，scene = 334 MB
- `/usr/local/lib/` 全量拷贝：173 MB（大部分是 `.a` 静态库，`.so` 只有 ~1.3 MB）
- ubuntu:24.04 基础 + apt 包：~88 MB

## 方案一：镜像体积缩减

`deploy/k8s/Dockerfile.cpp` 两处改动：

1. **Strip 二进制**：`strip --strip-unneeded` 把 gate 174→24 MB，scene 334→28 MB
2. **只拷贝 `.so`**：`COPY --from=builder /usr/local/lib/*.so* /usr/local/lib/` 跳过 `.a` 静态库（节省 ~170 MB）

结果：**缩减 85%**（1.33 GB → 194 MB）。

## 方案二：分离调试符号用于 Release 崩溃分析

问题：strip 后 core dump 丢失变量/行号信息。

标准做法——**分离调试信息文件**（separate debug info）：

### 构建流程

`build_linux.sh` 新增选项：
- `--relwithdebinfo`：使用 `CMAKE_BUILD_TYPE=RelWithDebInfo`（保持 `-O2` 优化 + `-g` 调试信息）
- `--split-debug`：提取 `.debug` 文件、strip 二进制、添加 gnu-debuglink

`--split-debug` 执行步骤：
```bash
# 1. 提取调试信息
objcopy --only-keep-debug bin/gate bin/symbols/gate.debug

# 2. Strip 部署二进制
strip --strip-unneeded bin/gate

# 3. 在二进制中嵌入 debuglink（GDB 自动找到对应 .debug 文件）
objcopy --add-gnu-debuglink=bin/symbols/gate.debug bin/gate
```

### Dockerfile 多阶段构建

```
Stage 1 (deps):    gcc:13 + 第三方库
Stage 2 (builder): 用 --relwithdebinfo --split-debug 构建
Stage 2.5 (symbols): FROM scratch，仅包含 .debug 文件
Stage 3 (runtime): ubuntu:24.04，stripped 二进制 + .so 动态库
```

`symbols` 阶段是 `FROM scratch`，不会包含在默认构建目标中，运行镜像保持小体积。

### 使用方法

```bash
# 构建运行镜像（和以前一样，~194 MB）
docker build -f deploy/k8s/Dockerfile.cpp -t mmorpg-node:v3 .

# 提取调试符号到本地文件系统
docker build -f deploy/k8s/Dockerfile.cpp --target=symbols -o ./debug-symbols .
# → debug-symbols/gate.debug (~150 MB)
# → debug-symbols/scene.debug (~300 MB)
```

### 崩溃分析流程

```bash
# 1. 从崩溃 pod 拷贝 core dump
kubectl cp <pod>:/tmp/core.12345 ./core.12345

# 2. 用 GDB 加载符号文件
gdb ./gate ./core.12345 -s gate.debug

# 3. 完整堆栈 + 变量 + 行号
(gdb) bt full
(gdb) info locals
(gdb) frame 3
(gdb) print some_variable
```

### 符号归档

每次发版：
```bash
# 提取并归档
docker build -f deploy/k8s/Dockerfile.cpp --target=symbols -o ./symbols-v3 .
tar czf symbols-v3.tar.gz symbols-v3/
# 和 release tag 一起存放 / 放到制品库
```

每个部署版本都要保留对应的符号文件。没有匹配的符号，core dump 无法分析。

## 关键文件

| 文件 | 用途 |
|------|------|
| `deploy/k8s/Dockerfile.cpp` | 多阶段构建，包含 `symbols` 阶段 |
| `tools/scripts/build_linux.sh` | `--relwithdebinfo` + `--split-debug` 选项 |

## 备注

- `RelWithDebInfo` 生成的（strip 前）二进制比 `Release` 略大，但 strip 后体积相当。
- 二进制中嵌入的 `gnu-debuglink` 是 CRC + 文件名提示——GDB 会在标准路径（`/usr/lib/debug/`、同目录）自动查找对应 `.debug` 文件。
- 生产 K8s 环境：需启用 core dump（`ulimit -c unlimited`）并配置 core pattern（如 `/tmp/core.%e.%p`）。
