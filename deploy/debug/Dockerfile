# 运行镜像与构建镜像必须来自同一次编译，确保二进制与分离符号完全匹配。
ARG NODE_IMAGE
ARG NODE_BUILDER_IMAGE

FROM ${NODE_BUILDER_IMAGE} AS node_builder
FROM ${NODE_IMAGE} AS debug

USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
    gdb \
    procps \
    && rm -rf /var/lib/apt/lists/*

# GDB 通过二进制内的 .gnu_debuglink 在同目录查找这些符号。
COPY --from=node_builder /src/bin/symbols/gate.debug /app/bin/gate.debug
COPY --from=node_builder /src/bin/symbols/scene.debug /app/bin/scene.debug
COPY --from=node_builder /src/bin/symbols/battle.debug /app/bin/battle.debug

WORKDIR /app/bin
USER 10001:10001
