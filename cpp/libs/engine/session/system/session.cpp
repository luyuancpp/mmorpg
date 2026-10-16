#include "session.h"

#include "proto/common/base/session.pb.h"
#include <muduo/base/Logging.h>
#include <limits>
#include "core/utils/encode/base64.h"

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";

using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByReplyContext(const GrpcReplyContext& context)
{
    // initialMetadata 与官方客户端的 GetServerInitialMetadata 同义:-bin 键已解掉线上那层 base64,剩下的是应用层值。
    for (const auto& [key, value] : context.initialMetadata) {
        if (key == kSessionBinMetaKey) {
            return ParseSessionDetailsMetadataValue(value);
        }
        else {
            LOG_DEBUG << "Skipping metadata key: " << key;
        }
    }
    return nullptr;
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
