# Gate Client TCP High Water Mark Handling (Important)

## Problem
Gate sends data to clients via muduo TcpConnection. If a client is not consuming data normally (disconnected network, cheat software, slow client), the muduo user-space send buffer will grow continuously, eventually causing Gate process memory bloat or even OOM.

## Handling Strategy
- **Game client connections**: Set a high water mark callback (2MB threshold). When triggered, **directly forceClose**.
  - Rationale: A normal game client must continuously consume data; there is no legitimate scenario for "buffering large amounts of data."
  - No need for throttle/degrade like server-to-server connections — just kick the client.
- Threshold choice: 2MB is extremely generous for game packets (single-frame sync packets are typically << 100KB).

## Implementation Location
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`
- In `HandleConnectionEstablished`, call `conn->setHighWaterMarkCallback(OnClientHighWaterMark, kClientHighWaterMark)`
- `OnClientHighWaterMark` callback logs a WARN and calls `conn->forceClose()`

## muduo High Water Mark Mechanism Summary
- In `TcpConnection::sendInLoop()`: when outputBuffer grows from below the threshold to >= threshold, the callback fires once.
- `setHighWaterMarkCallback(cb, threshold)` is called after connection establishment.
- Can be combined with `setWriteCompleteCallback` for throttle mode (not needed for game client scenarios).
