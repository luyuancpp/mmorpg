# 脚本目录规范

- 脚本放置规则：`tools/scripts` 是仓库中维护脚本的标准目录。
- `tools/scripts/third_party` 是第三方构建/维护脚本的标准目录。
- 根目录的 `scripts/` 仅用于引导或薄封装入口，不要在此重复实现逻辑。
- 不要将项目维护的脚本放在第三方依赖目录（如 `third_party/*`）中，除非该脚本明确属于上游项目。
- 当前示例：gRPC 构建脚本位于 `tools/scripts/third_party/build_grpc.ps1`，在 `scripts/build_grpc.ps1` 中有薄封装。
