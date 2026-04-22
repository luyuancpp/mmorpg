# SceneManager跨节点切场景释放玩家方案选型

## 方案对比

### ① SceneManager 主动 RPC 通知旧 node（当前实现）
- SceneManager 检查 currentLoc.NodeId != nodeId 时，直接调用旧 node 的 ReleasePlayer（或 LeaveScene）RPC。
- 旧 node 立即 SavePlayerToRedis + HandleExitGameNode，保证 Save 在新 node 加载前发起。
- 顺序、观测性、扇出成本、幂等性都优于方案②。

### ② 旧 node 监听 player_locator 变更
- 每个 scene node 订阅 player_locator 变更，发现玩家迁出后自行 Save + Destroy。
- 存在 watch 延迟，Save 可能晚于新 node 加载，导致数据覆盖。
- 扇出大，调试难，死节点不会收到通知。

## 当前选型理由
- 顺序保证：Save 必然在新 node 加载前发起，race window 最小。
- 观测性：RPC 失败可见，易于重试/报警。
- 复用基础设施：沿用 SceneNodeGrpc 连接、endpoint 发现。
- 扇出成本低：仅跨节点切换时 1 次 RPC。
- 幂等性：旧 node 没有该玩家直接返回 OK。

## 适用范围
- 仅跨节点切换时触发。节点内切场景无需 Save/Destroy，直接复用 entity。
- 未来如需去中心化/无 SceneManager，可考虑方案②。

## 相关文件
- proto/scene_manager/scene_node_service.proto
- cpp/nodes/scene/handler/grpc/scene_node_service.{h,cpp}
- go/scene_manager/internal/logic/scene_node_client.go
- go/scene_manager/internal/logic/enterscenelogic.go
