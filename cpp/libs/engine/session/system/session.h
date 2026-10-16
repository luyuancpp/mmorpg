#pragma once
#include "engine/core/type_define/type_define.h"
#include <memory>
#include <string_view>
#include "grpc_client/grpc_call_tag.h"


class  SessionDetails;
using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

// 从生成客户端全局应答处理器的上下文(GrpcReplyContext.initialMetadata = 服务端回写的响应头)里找 x-session-detail-bin,
// 交给下面的 ParseSessionDetailsMetadataValue 解析;没有这个键返回 nullptr。上下文只在回调期间有效,这里不留引用。
SessionDetailsPtr GetSessionDetailsByReplyContext(const GrpcReplyContext& context);

// x-session-detail-bin 的**应用层值**(传输层已解掉 -bin 那层 base64,剩下的仍是一段带 padding 的标准 base64)
// → SessionDetails。生成客户端的应答上下文(上面)与直接用 muduo_grpc::Channel 的路径(gate 路由 Forward)共用这一处解析
// (docs/design/grpc-muduo-transport.md §4.2)。
// 值不合法(非 4 的倍数长度、解码失败、protobuf 解析失败)返回 nullptr 并记 ERROR。
SessionDetailsPtr ParseSessionDetailsMetadataValue(std::string_view applicationValue);
