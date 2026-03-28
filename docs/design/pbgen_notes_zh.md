# Proto-Gen (pbgen) 笔记

- pbgen C++ 代码生成在遇到缺失的 proto 源目录时应跳过（发出警告并继续），而非直接报错退出。
- code_parser 的全局用户代码块必须仅在第一个方法签名之前检测；否则方法的 BEGIN/END 标记会被错误分类。
- `tools/generated/` 在根目录 `.gitignore` 中被忽略；除非先调整忽略规则，否则不要将已跟踪的辅助文件移至该目录。

## 2026-03-14 项目结构重组
- `doc/` 重命名为 `docs/`；设计文档位于 `docs/design/`，笔记位于 `docs/notes/`
- 根目录脚本移至 `scripts/`（autogen.sh、setup-dependencies.sh、sync-submodules.*）
- `vcxproj2cmake.py` 移至 `tools/`
- `generated/generated_{code,data,proto,tables}/` → `generated/{code,data,proto,tables}/`
- `cpp/unit_test/` → `cpp/tests/`；game.sln 已更新
- `data_tables/` → `data/`；paths.py 已更新
- `go/github.com/`（GOPATH 历史遗留）已删除
- 已更新的引用：go/build.bat、proto_gen.yaml、tools/data-table-exporter/core/paths.py、.github/copilot-instructions.md

## tools/scene_manager 现状
- `tools/scene_manager/` 仅包含生成的 protobuf 文件（5 个 .pb.go 文件）：
  - base/{empty, node}.pb.go
  - internal/storage/storage.pb.go
  - scene_manager/{scene_manager_service, scene_manager_service_grpc}.pb.go
- 所有文件均由 pbgen 在 2026-03-14 重新生成
- pbgen 输出配置指向 `go/generated/scene_manager/` 而非 `tools/scene_manager/`
- tools 中没有其他代码导入这些包
- SceneManager 的实际实现位于 `go/scene_manager/`，其中也有相同的 proto 输出文件
- 结论：**tools/scene_manager 看起来是冗余的，可以删除**。规范位置为 `go/scene_manager/`。

## 2026-03-14 命名清理工具
- 新增 `tools/scripts/normalize_names.ps1`，支持 `audit` 和 `apply` 模式，用于递归文件名/目录名规范化。
- 默认行为排除 `third_party` 及高风险/构建目录。
- 已将命令接入 `tools/scripts/dev_tools.ps1`：`naming-audit`、`naming-apply`。

## 2026-03-14 命名清理第二波（已完成，已提交 fb9fd09a9）
主要重命名：
- `cpp/libs/services/scene/scene/scene/mananger/` → `manager/`（拼写修正）
- `cpp/tests/message_limiter` → `message_limiter_test`
- `tools/proto-generator` → `proto_generator`；`data-table-exporter` → `data_table_exporter`
- `tools/scripts/dev-tools.ps1` → `dev_tools.ps1`
- `docs/design/ai-session-*.md` → `ai_session_*.md`
- `java/sa-token-node` → `sa_token_node`
- 最终审计结果：**0 个候选项**

## 会话文档规范
- 每当对话涉及框架/架构设计思考时，将关键想法记录到 `docs/design/ai_session_NN.md`（使用下一个可用编号）。
- 当前最新：`ai_session_10.md`（空文件）。下一个新会话从 `ai_session_11.md` 开始。

## 节点重启 / 网络断开安全性（2026-03-14）

### 基于 UUID 的身份保证
- 每次节点启动都通过 `boost::uuids::random_generator` 生成新的 UUID。
- 所有节点比较必须使用 `NodeUtils::IsSameNode(uuid1, uuid2)`，绝不使用原始 `node_id`。
- **BUG / TODO**：`ConnectToGrpcNode` 没有 UUID 检查——需要与 TCP 路径相同的 UUID 守卫。

### 重连 ≠ 数据刷新（关键架构规则）
- **绝不将业务数据同步与原始 TCP 重连/断开事件耦合。**
- 正确模式：TCP 重连仅重建传输层。数据同步由握手成功后发送的**显式业务消息**驱动。

## contracts/kafka C++ 链接陷阱
- 在 `proto/contracts/kafka/` 下添加新 proto 文件时，需要在 `cpp/generated/proto/proto.vcxproj` 中包含 `contracts/kafka/*.pb.cc`。

## pbgen event-id 推进笔记
- 当前运行 `pbgen` 会作为副作用重新生成 `tools/robot/proto/**` 中已跟踪的 `.pb.go` 文件。
- 如果 `tools/robot/logic/handler/` 目录不存在，`pbgen` 可能在后期阶段失败。
- Gate Kafka 处理器应确保每个消费的 `GateCommand` 仅被分发一次。

## Windows 下 pbgen C++ gRPC 代码生成
- 当 `--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe` 依赖 PATH 时可能会静默失败；应使用绝对路径解析插件。
- Gate C++ gRPC 生成基于 `domain_meta.<domain>.rpc.type`，而非目录级别的 proto service 扫描。

## 命名规范
- 面向用户的命名应优先使用 `proto-gen`；仅保留 `pbgen` 作为历史/兼容性别名。
- 规范的内部生成器源代码现位于 `tools/proto_generator/protogen/`。
