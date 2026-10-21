# 基础镜像由构建脚本以独占标签固定，并在构建前后核对 image ID。
# 保留原镜像的非 root 用户、工作目录及 java -jar 入口，不安装额外工具。
ARG GATEWAY_IMAGE
FROM ${GATEWAY_IMAGE}

COPY binary/service.jar /app/service.jar
COPY source_meta/source-manifest.json /app/DEBUG_SOURCE.json
COPY source_meta/BUILD_INFO /app/BUILD_INFO
