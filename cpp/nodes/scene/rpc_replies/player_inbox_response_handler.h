#pragma once

// 给生成的 PlayerInbox gRPC 客户端装应答 / 失败处理器(docs/design/player-inbox-service.md §7.11)。
// 生成器不会为 gRPC 服务注册回包处理函数,手写引导代码自己装(形状同 InitSceneManagerReply / InitDataServiceReply)。
// 只是给全局 std::function 赋值,没有前置依赖;在 main 里发出任何请求之前调用一次,幂等。
void InitPlayerInboxReply();
