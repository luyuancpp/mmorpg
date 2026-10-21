# 仅供本机开发：继承对应服务的运行镜像，保留它的配置、数据表、证书及目录权限。
# GO_RUNTIME_IMAGE 必须指向已经存在的同服务 Linux amd64 镜像。
# 两个命名上下文必须显式传入，默认 scratch 只用于缺少产物时立即失败：
#   --build-context binary=<目录，仅含本次编译的 service>
#   --build-context delve=<目录，仅含固定版本的 Linux amd64 dlv>
#   --build-context source_meta=<目录，含本次 source-manifest.json 与 BUILD_INFO>
# service 使用 Go 1.27.2、CGO_ENABLED=0、-gcflags="all=-N -l" 编译，
# 不使用 -trimpath 或删除调试信息的 -s/-w；源码路径由 VS Code substitutePath 映射。
# dlv 使用支持 Go 1.27 的 v1.27.2，并保留默认的 Go 版本兼容检查。
ARG GO_RUNTIME_IMAGE
FROM scratch AS binary
FROM scratch AS delve
FROM scratch AS source_meta

FROM ${GO_RUNTIME_IMAGE}
COPY --from=binary --chmod=0755 /service /app/service
COPY --from=delve --chmod=0755 /dlv /usr/local/bin/dlv
COPY --from=source_meta /source-manifest.json /app/DEBUG_SOURCE.json
COPY --from=source_meta /BUILD_INFO /app/BUILD_INFO

# 运行镜像的 /app 归 root 只读；Delve 自己的配置写入现有可写临时目录。
ENV XDG_CONFIG_HOME=/tmp
WORKDIR /app
USER 10001:10001

# Compose 只向宿主 127.0.0.1 映射该端口，不暴露到局域网。
# --continue 保持服务正常启动；--accept-multiclient 允许反复附加和断开。
# 原 Compose 的 -f 等参数会追加到末尾的 -- 之后，仍由业务进程解析。
EXPOSE 40000
ENTRYPOINT ["/usr/local/bin/dlv", "exec", "/app/service", "--headless=true", "--listen=0.0.0.0:40000", "--api-version=2", "--accept-multiclient", "--continue", "--"]
