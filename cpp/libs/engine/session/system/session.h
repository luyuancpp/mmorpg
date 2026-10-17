#pragma once
#include "engine/core/type_define/type_define.h"
#include <memory>
#include <string_view>
#include "grpc_client/grpc_call_tag.h"


class  SessionDetails;
using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

// 从全局应答处理器拿到的应答上下文里取响应头回显的 x-session-detail-bin;没有回显返回 nullptr。
SessionDetailsPtr GetSessionDetailsByReplyContext(const GrpcReplyContext& context);

// x-session-detail-bin 的**应用层值**(传输层已解掉 -bin 那层 base64,剩下的仍是一段带 padding 的标准 base64)
// → SessionDetails。应答上下文路径(上面)与 gate 路由转发的手写回调共用这一处解析(docs/design/grpc-muduo-transport.md §4.2)。
// 值不合法(非 4 的倍数长度、解码失败、protobuf 解析失败)返回 nullptr 并记 ERROR。
SessionDetailsPtr ParseSessionDetailsMetadataValue(std::string_view applicationValue);
