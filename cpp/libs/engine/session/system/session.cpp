#include "session.h"

#include "proto/common/base/session.pb.h"
#include <muduo/base/Logging.h>
#include <limits>
#include "core/utils/encode/base64.h"

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";

using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByReplyContext(const GrpcReplyContext& context)
{
    // 回显在响应头(初始 metadata)里,不在 trailers:路由服用 grpc.SetHeader 回写(设计 §4.2)。
    const std::string* applicationValue = context.FindInitialMetadata(kSessionBinMetaKey);
    if (applicationValue == nullptr) {
        return nullptr;
    }
    return ParseSessionDetailsMetadataValue(*applicationValue);
}

SessionDetailsPtr ParseSessionDetailsMetadataValue(std::string_view applicationValue)
{
    // Base64Decode(boost beast)按 n/4*3 分配输出,只对带 padding 的输入安全;应用层值永远是
    // 带 padding 的标准 base64(生成客户端 Base64Encode 的产物,路由服原样回显),不是 4 的倍数就是坏值。
    if (applicationValue.empty() || applicationValue.size() % 4 != 0) {
        LOG_ERROR << "Malformed Base64 session details (length " << applicationValue.size() << ").";
        return nullptr;
    }

    std::vector<uint8_t> decoded = Base64Decode(std::string(applicationValue));
    if (decoded.empty()) {
        LOG_ERROR << "Failed to decode Base64 session details.";
        return nullptr;
    }

    auto sessionDetails = std::make_unique<SessionDetails>();
    if (decoded.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        LOG_ERROR << "Session details payload too large: " << decoded.size();
        return nullptr;
    }

    if (sessionDetails->ParseFromArray(decoded.data(), static_cast<int>(decoded.size()))) {
        return sessionDetails;
    }
    LOG_ERROR << "Failed to parse SessionDetails protobuf.";
    return nullptr;
}
