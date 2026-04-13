# Gate 客户端 TCP 高水位处理（重要）

## 问题
Gate 通过 muduo TcpConnection 向客户端发送数据。如果客户端不在正常消费数据（断网、外挂、慢客户端），
muduo 用户态发送缓冲区会持续增长，最终导致 Gate 进程内存膨胀甚至 OOM。

## 处理策略
- **游戏客户端连接**：设置高水位回调（2MB 阈值），触发后 **直接 forceClose**。
  - 理由：正常游戏客户端必须持续消费数据，不存在合理的"暂存大量数据"场景。
  - 不需要像 server-to-server 那样做节流（throttle）/ 降级 — 直接踢掉。
- 阈值选择：2MB 对游戏包已经极其宽裕（单帧同步包通常 << 100KB）。

## 实现位置
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`
- 在 `HandleConnectionEstablished` 中调用 `conn->setHighWaterMarkCallback(OnClientHighWaterMark, kClientHighWaterMark)`
- `OnClientHighWaterMark` 回调记录 WARN 日志并 `conn->forceClose()`

## muduo 高水位机制简述
- `TcpConnection::sendInLoop()` 中检测：当 outputBuffer 从低于阈值增长到 >= 阈值时，触发一次回调。
- `setHighWaterMarkCallback(cb, threshold)` 在连接建立后调用。
- 配合 `setWriteCompleteCallback` 可实现节流模式（游戏客户端场景不需要）。
