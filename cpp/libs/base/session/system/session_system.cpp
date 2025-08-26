#include "session_system.h"

#include "proto/common/session.pb.h"
#include <muduo/base/Logging.h>
#include <grpcpp/grpcpp.h>
#include "util/encode/base64.h"

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";


using SessionDetailsPtr = std::unique_ptr<SessionDetails>;

SessionDetailsPtr GetSessionDetailsByClientContext(const grpc::ClientContext& context)
{
    auto& trailingMetadata = context.GetServerInitialMetadata();
    for (const auto& pair : trailingMetadata) {
        if (pair.first == kSessionBinMetaKey) {
            std::string base64Str = std::string(pair.second.data(), pair.second.size());

            std::vector<uint8_t> decoded = Base64Decode(base64Str);
            if (decoded.empty()) {
                LOG_ERROR << "Failed to decode Base64 session details.";
                return nullptr;
            }

            auto sessionDetails = std::make_unique<SessionDetails>();
            if (sessionDetails->ParseFromArray(decoded.data(), decoded.size())) {
                return sessionDetails;
            }
            else {
                LOG_ERROR << "Failed to parse SessionDetails protobuf.";
                return nullptr;
            }
        }
        else {
            LOG_DEBUG << "Skipping metadata key: " << pair.first.data();
        }
    }
    return nullptr;
}



