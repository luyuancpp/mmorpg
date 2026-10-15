#pragma once
#include "engine/core/type_define/type_define.h"
#include <memory>
#include <string_view>
#include <grpcpp/client_context.h>


class  SessionDetails;
using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByClientContext(const grpc::ClientContext& context);

// x-session-detail-bin 的**应用层值**(传输层已解掉 -bin 那层 base64,剩下的仍是一段带 padding 的标准 base64)
// → SessionDetails。官方 gRPC 路径(上面)与 muduo_grpc 路径共用这一处解析(docs/design/grpc-muduo-transport.md §4.2)。
// 值不合法(非 4 的倍数长度、解码失败、protobuf 解析失败)返回 nullptr 并记 ERROR。
SessionDetailsPtr ParseSessionDetailsMetadataValue(std::string_view applicationValue);
